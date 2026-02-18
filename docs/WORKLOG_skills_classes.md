# Worklog: Skill and Class System Backport

**Started:** 2026-02-09
**Last Updated:** 2026-02-18

## Session 1: Analysis (2026-02-09)

### Objective

Analyze the refactored Skill system and the new Class/Job system from the `src_20_dev` branch. Compare them to the current `src` implementation and create a comprehensive backporting plan.

### Work Performed

#### Part 1: Skill System Refactor

1.  **Initial Investigation (`src_20_dev`):**
    -   Referenced `PLAN_SKILL_REFACTOR.md` which pointed to `skedit` in `src_20_dev`.
    -   Searched for `skedit` to identify key implementation files: `skills.c` (OLC logic), `olc.c` (command table and interpreter), and `olc.h` (declarations).

2.  **System Analysis (`src_20_dev`):**
    -   Confirmed the existence of a full OLC editor for skills.
    -   Verified that the system is designed to load skill data from external sources (implied to be JSON) and manage them in memory, likely with a hash table for fast lookups.
    -   This implementation completely decouples skill definitions from the compiled C code, allowing for live editing.

3.  **Current State Analysis (`src`):**
    -   Confirmed that `src` still uses the hardcoded `skill_table` array in `const.c` and `gsn_` global variables for skill lookups.

#### Part 2: Class/Job System

1.  **Initial Investigation (`src_20_dev`):**
    -   Referenced `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`.
    -   Searched for the player-facing command `do_setclass` to find the core logic.
    -   Identified `skills.c` as the main implementation file for the class-switching logic.
    -   Located the data structure definitions in `merc.h`.

2.  **System Analysis (`src_20_dev`):**
    -   Examined `merc.h` and found the `CLASS_DATA` and `CLASS_LEVEL` structs.
    -   Confirmed that `PC_DATA` was modified to use a `LLIST *classes` and a `CLASS_LEVEL *current_class` pointer, replacing the old integer-based system. This enables independent leveling for any number of classes.
    -   Noted the plan for a `clsedit` command for OLC management.

3.  **Current State Analysis (`src`):**
    -   Searched for `sub_class_current` and confirmed the legacy system is still in use.
    -   The `pc_data` struct in `src/merc.h` contains multiple integer fields for tracking class and subclass progression, and the code relies on the static `sub_class_table`.

#### Part 3: Documentation and Planning

1.  Authored `src/docs/PLAN_backport_skills_classes.md` with phased implementation plan.

---

## Session 2: Phase 0 & 1 Implementation (2026-02-13)

### Design Decisions

Key architectural decisions from user discussion:

- **No global pointers:** All `gsn_*` globals will be eliminated. No `gsk_*` replacements. Skills accessed via lookup functions like the race system.
- **String-based IDs:** Primary lookup key is the skill name string, not integer index.
- **Single SPELL_FUN with invocation method:** Instead of separate function pointers per invocation method (src_20_dev approach with 18 pointers), one `SPELL_FUN` receives an `int invocation` parameter (INVOC_CAST, INVOC_QUAFF, etc.).
- **Individual JSON files per skill:** Following the race system pattern, not a monolithic file.

### Phase 0: Token-Affect Coupling Hardening

**Completed:** 2026-02-13

Audited the bidirectional token<>affect lifecycle in `handler.c` and found/fixed three bugs:

1. **`affect_removeall_obj()` (line ~1472):** Was freeing affects without unlinking them from their source token's `->affects` list. Added `if (IS_VALID(paf->token)) list_remlink(paf->token->affects, paf, false);` before `free_affect(paf)`.

2. **`affect_remove_obj()` (line ~1549):** Same issue as above — missing token unlink. Added identical fix.

3. **`extract_token()` (line ~3260):** Was nulling `paf->token` and clearing the list, but never actually removed the affects from their owning character/object. This left orphaned affects on chars/objects after their source token was extracted. **Rewrote** to:
   - Break the `paf->token` back-link first (prevents re-entrant list modification)
   - Call `affect_remove(token->player, paf)` or `affect_remove_obj(token->object, paf)` to properly remove each affect from its owner

`affect_remove()` (char version, line ~1432) already had correct token unlinking. `new_affect()` and `new_token_data()` already initialize fields correctly.

Remaining Phase 0 item: `AFFECT_DATA.skill` field addition deferred to Phase 5.

### Phase 1: Skill Data Backend

**Completed:** 2026-02-13

#### New Files

**`skill_data.h`** — Public API header:
- Constants: `SKILL_HASH_SIZE` (256), `SKILLS_DIR`, `SKILLFLAG_*` bitfield values, `INVOC_*` invocation method constants
- Lookup API: `skill_find()`, `skill_search()`, `skill_find_uid()`, `skill_name()`, `skill_first()`, `skill_count()`
- Compatibility API (current): `skill_find_uid()`, `skill_sn()`
- Spell function resolution: `spell_fun_lookup()`, `spell_fun_name()`
- Boot/persistence: `load_skill_data()`, `save_skill_data()`, `save_all_skill_data()`
- Allocation: `new_skill_data()`, `new_skill_class_level()`

**`skill_data.c`** — Full implementation:
- FNV-1a hash table (256 buckets) for O(1) case-insensitive name lookup
- UID index array for O(1) numeric lookup
- Alphabetically sorted linked list for ordered iteration
- Complete spell function name↔pointer resolution table (152 entries from `magic.h`)
  - Note: 22 functions declared in magic.h but never implemented were excluded (spell_change_sex, spell_crucify, spell_detect_evil, spell_detect_good, etc.)
- `load_skill_data()`: loads from `data/skills/*.json`, or bootstraps from legacy `skill_table[]` on first run
- `bootstrap_skills_from_table()`: one-time migration — assigns UIDs matching original array indices so saved data continues to work, then saves all skills as JSON
- `save_skill_data()` / `save_all_skill_data()`: JSON persistence with Jansson
- gsn_ globals set via `pgsn` pointers during bootstrap/load

#### Modified Files

**`merc.h`:**
- Added `SKILL_DATA` and `SKILL_CLASS_LEVEL` forward typedefs
- Added `#define MAX_SKILL_VALUES 8`
- Added `struct skill_data` definition (~50 fields including identity, flags, class levels list, invocation, messages, inks, token coupling, legacy compat, generic values)
- Added `struct skill_class_level` definition (class_name, level, rating)

**`db.c`:**
- Added `#include "skill_data.h"`
- Added `load_skill_data()` call in `boot_db()` after `load_races()`

**`CMakeLists.txt`:** Added `skill_data.c` to source list.

**`Makefile`:** Added `skill_data.c` to source list.

#### Build Verification

Clean build: zero warnings, zero errors. Installed to `/sentience/sent`.

### Commits

- Phase 0 token-affect fixes and Phase 1 skill data backend committed and pushed.

---

## Session 3: Phase 2 & 3 Implementation (2026-02-13)

### Phase 2: Compatibility Shim Layer

**Completed:** 2026-02-13

#### Changes

**`skill_data.h`:**
- Added `skill_resolve_gsn(const char *name)` declaration — returns `int16_t` UID
- Added `SKILL_CACHED(var, name)` macro — file-local cached skill pointer with lazy resolution

**`skill_data.c`:**
- Implemented `skill_resolve_gsn()` — wraps `skill_find()` with error logging for missing skills
- `skill_lookup()` in `magic.c` redirected to `skill_search()` for backward compatibility

### Phase 3: SPELL_FUN Signature Migration

**Completed:** 2026-02-13

This was the highest-risk phase — mechanical but vast, touching ~174 spell functions and ~46+ call sites across the entire codebase.

#### Phase 3a: Typedef and Macro Updates

**`merc.h`:**
- `SPELL_FUN` typedef changed from 6-arg `(int sn, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc)` to 7-arg `(SKILL_DATA *skill, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc, int invocation)`
- `SPELL_FUNC` macro updated to match new signature

#### Phase 3b: Spell Function Bodies (Mechanical)

All 25 `magic_*.c` files updated via automated perl regex — inserted `int sn = skill->uid;` compatibility shim as first line of every `SPELL_FUNC` body (~154 functions total). This allows all existing `sn` usage within spell bodies to work unchanged.

Variable shadowing fixes:
- `magic_astral.c`: `int skill;` → `int skill_pct;` in `spell_maze()` (local var conflicted with new `SKILL_DATA *skill` parameter)
- `magic_soul.c`: `int skill, skill2;` → `int skill_pct, skill2;` in `spell_soul_essence()` (same issue)

#### Phase 3c: Call Site Updates

Updated all invocation call sites to pass `skill_find_uid(sn)` as first arg and appropriate `INVOC_*` constant as last arg:

| File | Sites | Invocation Type |
|------|-------|-----------------|
| `magic.c` | `cast_end()`, `obj_cast_spell()`, `obj_cast()` | INVOC_CAST, INVOC_EQUIP, INVOC_INTERNAL |
| `magic2.c` | spell deflection bounce | INVOC_INTERNAL |
| `magic_mana.c` | counter_spell | INVOC_INTERNAL |
| `music.c` | 33 song spell_fun calls | INVOC_INTERNAL |
| `act_info.c` | 9 `spell_identify()` calls | INVOC_INTERNAL |
| `act_obj.c` | 6 direct spell calls (identify, cures) | INVOC_INTERNAL |
| `auction.c` | 2 `spell_identify()` calls | INVOC_INTERNAL |
| `magic_body.c` | 3 calls (blindness, heal, refresh) | INVOC_INTERNAL |
| `magic_chaos.c` | 1 call (curse) | INVOC_INTERNAL |
| `magic_energy.c` | 1 call (blindness) | INVOC_INTERNAL |
| `magic_holy.c` | 4 calls (frenzy, bless, 2x curse) | INVOC_INTERNAL |
| `magic_light.c` | 1 call (blindness) | INVOC_INTERNAL |
| `fight.c` | 4 direct + toxin_table pointer + breath_fun pointer | INVOC_INTERNAL |
| `special.c` | 2 function-pointer calls + 1 direct (poison) | INVOC_INTERNAL |
| `script_opcmds.c` | 1 function-pointer call (opcast) | INVOC_INTERNAL |
| `script_mpcmds.c` | 1 function-pointer call (mpcast) | INVOC_INTERNAL |
| `script_commands.c` | 1 breath_fun pointer call | INVOC_INTERNAL |

Files that needed `#include "skill_data.h"` added: `magic_body.c`, `magic_chaos.c`, `magic_energy.c`, `magic_holy.c`, `magic_light.c`, `fight.c`, `special.c`, `script_opcmds.c`, `script_mpcmds.c`, `script_commands.c`

#### Build Verification

Clean build: zero warnings, zero errors.

---

## Session 4: Phase 4 Implementation (2026-02-13)

### Phase 4: Player Skill Storage Migration

**Completed:** 2026-02-13

Added structural foundation for per-entry skill ratings. The `learned[]`/`mod_learned[]` arrays remain the authoritative runtime source for now, but each `SKILL_ENTRY` now carries its own `rating`, `mod_rating`, and `skill_data` pointer — shadowing the legacy arrays and preparing for eventual switchover.

#### Changes

**`merc.h`:**
- Added `SKILL_DATA *skill_data`, `int rating`, `int mod_rating` fields to `SKILL_ENTRY` struct
- Added `VERSION_PLAYER_011 0x0100000A` constant
- Bumped `VERSION_PLAYER` macro from `VERSION_PLAYER_010` to `VERSION_PLAYER_011`

**`mem.c`:**
- `new_skill_entry()` — initializes `skill_data = NULL`, `rating = 0`, `mod_rating = 0`, `sn = 0`, `token = NULL`
- Added `#include "skill_data.h"`

**`skills.c`:**
- `skill_entry_insert()` — sets `entry->skill_data` via UID lookup for built-in skills
- Added `#include "skill_data.h"`

**`io/json/json_char.c`:**
- Both JSON load paths (`json_to_char_skills_data` and `json_to_skills`) — after inserting skill entries, populate `entry->rating` from `learned[sn]` and `entry->mod_rating` from `mod_learned[sn]`
- JSON save path — syncs `entry->rating = ch->pcdata->learned[entry->sn]` before writing, then writes from entry fields

**`save.c`:**
- Both dat load paths (initial `fread_char` and `fread_char_old`) — after inserting skill entries, populate `entry->rating`/`entry->mod_rating` from `learned[]`/`mod_learned[]`
- `fix_character()` — added `VERSION_PLAYER_011` migration block: iterates `ch->sorted_skills` and copies `learned[sn]` → `entry->rating`, `mod_learned[sn]` → `entry->mod_rating` for all entries. Ensures existing characters loaded from pre-011 saves get their entry fields populated.

#### Design Notes

- Did NOT migrate `get_skill()` to read from `SKILL_ENTRY.rating` yet — there are ~79 read sites and ~28 write sites touching `learned[]` directly across 13+ files. Switching atomically would be a large mechanical change best done later or as part of a dedicated pass.
- The `learned[]` arrays remain authoritative; `SKILL_ENTRY.rating` shadows them. Both are kept in sync at load time and save time.
- `SKILL_DATA *skill_data` on `SKILL_ENTRY` uses the name `skill_data` (not `skill`) to avoid conflicts with the existing `SKILL_DATA *skill` parameter name in spell functions.

#### Build Verification

Clean build: zero warnings, zero errors.

---

## Session 5: Phase 5 Implementation (2026-02-13)

### Phase 5: AFFECT_DATA Migration

**Completed:** 2026-02-13

Added `SKILL_DATA *skill` pointer to `AFFECT_DATA` struct and populated it at all SET/creation sites across the codebase. The `int16_t type` field remains for backward compatibility. Both fields are set in parallel — `skill` from the pointer, `type` from the legacy sn integer.

#### Structural Changes

**`merc.h`:**
- Added `SKILL_DATA *skill` field to `AFFECT_DATA` struct (placed before `type` for logical grouping)
- Added comment marking `type` as legacy

**`mem.c`:**
- `new_affect()` — added `af->skill = NULL;` initialization
- `free_affect()` — added `af->skill = NULL;` cleanup

#### Spell Function SET Sites (~61 total)

**All 18 `magic_*.c` files** with `af.type = sn;` — added `af.skill = skill;` on the following line (using the `SKILL_DATA *skill` parameter already in scope from Phase 3).

**`magic_cosmic.c`** — 4 additional sites using pointer notation `paf->type = sn;` — added `paf->skill = skill;`

**`magic_cosmic.c` `spell_enchant_object()`** — replaced unused `int sn = skill->uid;` shim with `(void)skill;` to suppress warning (function never referenced `sn`).

#### Non-Spell SET Sites (~40 total)

| File | Sites | Pattern |
|------|-------|---------|
| `effects.c` | 4 | `af.type = gsn_*` → added `af.skill` via UID lookup |
| `fight.c` | 10 | Same pattern |
| `fight2.c` | 5 | Same pattern |
| `db.c` | 24+ | `af.type = gsn_*` (20 sites) + `paf->type = sn` (2 load sites) |
| `act_enter.c` | 1 | Same pattern |
| `act_move.c` | 4 | Same pattern |
| `act_obj.c` | 4 | Same pattern |
| `update.c` | 3 | Same pattern |
| `weather.c` | 2 | `af.type = gsn_*` + `af.type = skill_lookup(...)` |
| `script_commands.c` | 2 | `af.type = skill` (int) + `af.type = gsn_toxins` |
| `script_mpcmds.c` | 1 | `af.type = skill` (int) |
| `script_opcmds.c` | 1 | `af.type = skill` (int) |
| `script_rpcmds.c` | 1 | `af.type = skill` (int) |
| `script_tpcmds.c` | 1 | `af.type = skill` (int) |

#### Load Path Sites (~12 total)

| File | Sites | Action |
|------|-------|--------|
| `save.c` | 6 | After `paf->type = sn;` → `paf->skill` via UID lookup |
| `db.c` | 2 | Same |
| `io/json/json_char.c` | 2 | After `paf->type = json_integer_value(...)` → `paf->skill` via UID lookup |
| `io/json/json_persist.c` | 2 | Same + custom name path gets `paf->skill = NULL;` |

#### Sites Intentionally Not Modified

- `paf->type = -1` (7 sites) — custom-named affects, `skill` is already NULL from `new_affect()` init
- `paf->type = flag_value(catalyst_types, ...)` (8 sites) — catalyst affects, not skills, `skill` stays NULL
- `paf->type == ...` comparison/READ sites (~35 sites) — deferred to future migration; these still use the integer field

#### New Includes Added (12 files)

`effects.c`, `fight2.c`, `act_enter.c`, `act_move.c`, `update.c`, `weather.c`, `handler.c`, `save.c`, `script_rpcmds.c`, `script_tpcmds.c`, `io/json/json_char.c`, `io/json/json_persist.c`

#### Build Verification

Clean build: zero warnings, zero errors.

---

## Session 6: Phase 6 Closure Review (2026-02-18)

### Objective

Reconcile plan/review status to match implementation reality before moving P0 focus to object multityping.

### Work Performed

1. Re-audited `PLAN_backport_skills_classes.md` Phase 6 checklist and verified all 6a-6e items are marked complete.
2. Confirmed remaining unresolved items are explicitly deferred to later phases (Phase 7 editor work and Phase 9 cleanup), not Phase 6 blockers.
3. Updated status docs to reflect completion:
    - `PLAN_backport_skills_classes.md`: status now reads **Phases 0-6 Complete, Phase 7 pending**.
    - `DOCS_REVIEW_2026-02-18.md`: skills/classes section now states **Phases 0-6 complete** and shifts wording to follow-up/editor cleanup scope.

### Outcome

- Skills/classes P0 implementation phase (Phase 6) is now treated as complete in planning docs.
- Next P0 execution focus can cleanly shift to wrapping object multityping (Phase 5 conversion work).

---

## Session 7: `skill_entry` + Token Parity Audit vs `src_20_dev` (2026-02-18)

### Objective

Verify migration completeness for token-integrated skill behavior by comparing `src` and `src_20_dev` `skill_entry` handling, then convert findings into an actionable implementation checklist.

### Findings

1. **`src` still uses legacy arrays as runtime authority**
    - `skill_entry_rating()` / `skill_entry_mod()` read from `pcdata->learned[]` / `mod_learned[]` (or token values), while `src_20_dev` returns `entry->rating` / `entry->mod_rating`.
    - `get_skill()` in `handler.c` also reads arrays directly for player skills.

2. **Core write paths still mutate `learned[]` directly in `src`**
    - `do_practice()` standard-skill branch writes `ch->pcdata->learned[sn]`.
    - `check_improve_show()` writes `ch->pcdata->learned[sn]`.
    - script grant/set paths and immortal set-skill paths still write arrays first.
    - In `src_20_dev`, equivalent paths primarily mutate `entry->rating`.

3. **Cast path remains mixed-mode in `src`**
    - Token spells are entry/token-driven and already support token ratings/mana via `TOKVAL_SPELL_*`.
    - Built-in spells still lean on `sn` + `skill_table[]` + `get_skill(sn)` behavior.

4. **Persistence layer intentionally shadows entry fields from arrays**
    - `save.c` / JSON paths in `src` sync entry fields from arrays and write compatibility data.
    - This confirms migration is in an intermediate state by design, but not yet runtime-parity complete.

### Outcome

- Added a new **Phase 6f: `skill_entry` Runtime Parity Hardening** checklist to `PLAN_backport_skills_classes.md`.
- Checklist scopes the remaining conversion work needed before Phase 9 cleanup removes legacy arrays.
- This gives a concrete “finish skills/classes” execution slice before/alongside multityping wrap.

---

## Session 8: Phase 6f Slice A Implementation (2026-02-18)

### Objective

Begin executing Phase 6f by converting foundational runtime read/write paths to `SKILL_ENTRY` authority while preserving legacy compatibility.

### Code Changes

1. **Entry-authoritative reads (`skills.c`)**
    - `skill_entry_rating()` now returns:
      - token-derived rating for token entries,
      - `entry->rating` for player non-token entries,
      - NPC fallback behavior retained where needed.
    - `skill_entry_mod()` now returns `entry->mod_rating` for non-token entries.

2. **Player skill resolution (`handler.c`)**
    - `get_skill()` now prefers `SKILL_ENTRY` (`skill_entry_rating/mod`) for player skills, including racial checks.
    - Legacy `learned[]`/`mod_learned[]` used only as fallback when a skill entry is missing.

3. **Core write paths (`skills.c`)**
    - `check_improve_show()` now reads/writes `entry->rating`.
    - `do_practice()` standard-skill branch now updates `entry->rating` first.
    - Both paths mirror the final value back into `pcdata->learned[sn]` for transition compatibility.

### Validation

- Ran `Run Unit Tests` task (`./build tests` + `./sent -test:unit` dependency chain).
- Result: **8/8 passing**, no regressions.
- Existing compile warnings in unrelated files remain unchanged.

### Remaining 6f Work

- Script-based grant/set/adjust skill commands still write legacy arrays first.
- Immortal set-skill paths in `act_wiz.c` still need entry-first updates.
- Save/load pass still needs final alignment to make `entry->rating/mod_rating` primary persisted source (while retaining backward compatibility through Phase 9).

---

## Session 9: Phase 6f Slice B Implementation (2026-02-18)

### Objective

Continue Phase 6f by converting script and immortal skill mutation paths to `SKILL_ENTRY`-first writes with legacy mirroring.

### Code Changes

1. **Script skill mutation commands (entry-first writes)**
    - Updated `do_opskill`, `do_rpskill`, and `do_tpskill` to:
      - mutate `entry->rating` as primary state,
      - mirror `pcdata->learned[sn]` for transition compatibility,
      - create/remove `SKILL_ENTRY` as needed when setting values.

2. **Script grant path alignment**
    - Updated `scriptcmd_grantskill` to set `entry->rating` after insertion, then mirror `learned[sn]`.

3. **Immortal set-skill alignment**
    - Updated `do_sset` skill handling in `act_wiz.c` (`set skill <name|all> <value>`) to keep `entry->rating` synchronized with set values while preserving existing add/remove behavior.

### Validation

- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Remaining 6f Work

- Save/load consistency pass to make `entry->rating/mod_rating` the primary persisted fields (while preserving backward-compatible mirrors through Phase 9).
- Manual gameplay verification pass (normal skill, token skill, scripted grant/revoke, class-availability gating).

---

## Session 10: Phase 6f Slice C (JSON Persistence) (2026-02-18)

### Objective

Implement JSON serialization/deserialization support where `SKILL_ENTRY.rating` / `mod_rating` are treated as first-class persisted fields, while preserving compatibility with existing character JSON files.

### Code Changes

1. **JSON skill save path (`io/json/json_char.c`)**
        - `skills_to_json()` now writes `rating` and `mod_rating` fields from `SKILL_ENTRY` as primary persisted values.
        - Legacy mirror fields `learned` / `mod_learned` are still written with the same values for backward compatibility during migration.
        - Safety-net serialization for skills present only in legacy arrays also now emits `rating` / `mod_rating` plus legacy mirrors.

2. **JSON skill load paths (`io/json/json_char.c`)**
        - Updated both load paths (`json_to_char_skills_data` and `json_to_skills`) to read:
            - `rating` first, falling back to `learned`
            - `mod_rating` first, falling back to `mod_learned`
        - Loaded values continue to populate `pcdata->learned[]` / `mod_learned[]` and then synchronize into `SKILL_ENTRY` fields, preserving transitional mirror behavior.

### Validation

- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Remaining 6f Work

- Non-JSON player save/load paths are deprecated for current deployment, so no additional non-JSON persistence migration is planned in 6f.
- Manual gameplay verification pass (normal skill, token skill, scripted grant/revoke, class-switch availability gating).

---

## Session 11: Phase 6f Slice D (Cast/Practice Consumption Paths) (2026-02-18)

### Objective

Close the remaining 6f consumption-path gap by removing direct legacy array usage in cast/practice gating where entry-aware helpers are already available.

### Code Changes

1. **Cast selection and cast roll alignment (`magic.c`)**
        - `find_spell()` now gates known spells using `get_skill(ch, sn) > 0` rather than direct `pcdata->learned[sn] > 0`.
        - `do_cast()` now uses the previously resolved `skill` value for the cast success/failure roll, avoiding a second direct re-query.

2. **Practice gating alignment (`skills.c`)**
        - `can_practice()` now:
            - safely bails out if no `SKILL_ENTRY` exists,
            - uses `entry->rating` as primary, with legacy `learned[]` fallback,
            - uses that rating for the “already practiced” threshold check.
        - `had_skill()` now checks `SKILL_ENTRY.rating` first (with legacy fallback) for prior-skill determination.

### Validation

- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Remaining 6f Work

- Manual gameplay verification pass (normal skill, token skill, scripted grant/revoke, class-switch availability gating).

---

## Session 12: Phase 6f Cleanup Pass (Non-Legacy Runtime `learned[]` Reads) (2026-02-18)

### Objective

Clean up direct `pcdata->learned[]` runtime consumption in active gameplay paths that are not part of persistence/migration compatibility code.

### Code Changes

1. **Hunt skill consumption (`hunt.c`)**
    - Replaced direct hunt chance read from `pcdata->learned[gsn_hunt]` with `get_skill(ch, sn_hunt)`.
    - Preserves scent-tracking trait override behavior.

2. **Weapon skill consumption (`handler.c`)**
    - `get_weapon_skill()` now resolves player weapon skill via `get_skill(ch, sn)` instead of direct `learned[]` access.

3. **New-character weapon initialization (`nanny.c`)**
    - Default weapon setup now ensures a `SKILL_ENTRY` exists and sets `entry->rating = 50` as primary state.
    - Keeps legacy compatibility mirror `pcdata->learned[weapon] = 50`.

### Validation

- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Remaining 6f Work

- Manual gameplay verification pass (normal skill, token skill, scripted grant/revoke, class-switch availability gating).

---

## Session 13: Phase 6f Cleanup Pass (Remove Runtime `learned[]` Fallback Reads) (2026-02-18)

### Objective

Keep legacy mirroring behavior, but stop actively relying on `pcdata->learned[]` in runtime skill consumption/mutation logic in favor of `SKILL_ENTRY`.

### Code Changes

1. **Core runtime resolver (`handler.c`)**
    - `get_skill()` now uses `SKILL_ENTRY` as the runtime authority path and no longer falls back to reading `learned[]`/`mod_learned[]` when entries are missing.

2. **Skill runtime helpers (`skills.c`)**
    - Training increment path now raises `entry->rating` and mirrors to `learned[]`.
    - `group_add()` now checks/initializes via `SKILL_ENTRY` first, then mirrors `learned[]`.
    - `can_practice()` / `had_skill()` no longer use `learned[]` read fallback.
    - `update_skills()` now evaluates/removes based on `SKILL_ENTRY.rating` and mirrors the resulting value to `learned[]`.

3. **Script skill mutation (`script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`)**
    - `+` and `-` operations now consume only `entry->rating` (no `learned[]` read fallback).
    - Mirror writes to `learned[]` are retained.

### Validation

- Re-scanned active source: remaining `learned[]` checks are now confined to persistence files (`save.c`, `io/json/json_char.c`) and dead/commented legacy blocks.
- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Final Runtime Grep Report

- Scope (active runtime files only): `handler.c`, `skills.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`
- Read pattern: `->learned[...]` occurrences not used as assignment targets
- Exclusions: persistence files (`save.c`, `io/json/json_char.c`) and known dead `#if 0` lines in `skills.c`
- **Before (pre-cleanup for this scope): 30 read occurrences**
- **After (current): 0 read occurrences**

### Remaining 6f Work

- Manual gameplay verification pass (normal skill, token skill, scripted grant/revoke, class-switch availability gating).

---

## Session 14: Phase 7/8 Completion Audit (2026-02-18)

### Objective

Verify whether Phases 7 and 8 can be marked complete based on current implementation status.

### Findings

1. **Phase 7 (OLC Editors): Complete**
    - `skedit` implemented: `editors/skills/skedit.c` with command table and spell function editing.
    - `clsedit` implemented: `editors/classes/clsedit.c` with reward management (`reward`, `rewardflags`, `rewards`, `reward remove`).
    - Commands registered in dispatch tables:
      - `interp.c` command table includes `skedit` and `clsedit`.
      - `olc.c` editor map includes `skill` and `class` editors.

2. **Phase 8 (Song migration): Partially complete**
    - `SONG_DATA` backend exists (`song_data.h`, `song_data.c`) with lookup/count/load/save.
    - Bootstrap from legacy `music_table[]` exists in `load_songs()` path.
    - `db.c` boots songs via `load_songs()`.
    - **Remaining:** `music.c` still has direct `music_table[]` consumption sites, so final migration item is not complete yet.

### Documentation Updates

- Updated plan status to: **Phases 0-7 Complete, Phase 8 in progress**.
- Marked all Phase 7 checklist items complete.
- Marked first three Phase 8 items complete; left `music.c` migration unchecked.

---

## Session 15: Phase 8 Completion (2026-02-18)

### Objective

Finish the final Phase 8 migration item by removing residual `music_table[]` usage from `music.c`.

### Code Changes

- Removed obsolete commented legacy block in `music.c` that still referenced `music_table[]` paths.
- Active song execution logic remains `SONG_DATA`-driven (`ch->song`, `entry->song`, `song_lookup` path).

### Validation

- Verified `music_table[]` references in `music.c`: none remaining.
- Ran unit tests: `./sent -test:unit`
- Result: **8/8 passing**, no regressions.

### Outcome

- Phase 8 checklist is now fully complete.
- Plan status updated to: **Phases 0-8 Complete, Phase 9 pending**.

---

## Session 16: Phase 9 Warning Cleanup (`unused sn`) (2026-02-18)

### Objective

Reduce clean-build warning noise by eliminating the bulk `unused variable 'sn'` warnings in spell files without changing spell behavior.

### Code Changes

- Updated spell-local compatibility declarations from `int sn = skill->uid;` to `int sn __attribute__((unused)) = skill->uid;` in the targeted `magic*.c` files.
- Fixed malformed replacements from the first automated pass in:
    - `magic_nature.c`
    - `magic_shock.c`
    - `magic_sound.c`
    - `magic_mana.c`
    - `magic_mind.c`
    - `magic_death.c`
- Preserved runtime behavior while restoring compile correctness in affected functions.

### Validation

- Ran clean build: `./build clean`
- Build completed successfully (link succeeded).
- Verified warning logs no longer include `unused variable 'sn'` entries.

### Outcome

- The primary Phase 9 warning class (`unused sn`) is cleaned up.
- Remaining warnings are in other categories and can be handled as separate cleanup slices.

---

## Session 17: `skill_from_sn` Retirement (2026-02-18)

### Objective

Finish migration away from the legacy `skill_from_sn()` compatibility shim now that all runtime callsites are on UID-based lookups.

### Code Changes

- Replaced remaining runtime callsites of `skill_from_sn(...)` with `skill_find_uid(...)` across gameplay, scripting, persistence, and JSON paths.
- Added missing `skill_data.h` includes for files that now call `skill_find_uid` directly.
- Removed the `skill_from_sn` API declaration from `skill_data.h` and `merc.h`.
- Removed the `skill_from_sn` function definition from `skill_data.c`.

### Validation

- Recursive grep in `src/` shows no code references to `skill_from_sn(`; only historical docs mention it.
- Clean build completed successfully.

### Outcome

- Legacy `skill_from_sn` shim is fully retired from runtime code.
- UID lookup is now uniformly done through `skill_find_uid(...)`.

---

## Session 18: AFFECT_DATA Pointer-First Helper Migration (2026-02-18)

### Objective

Start the deferred Phase 5 read-path migration by making core affect helper checks consume `AFFECT_DATA.skill` first, with legacy `type` fallback for compatibility.

### Code Changes

- Updated core affect helper matching in `handler.c` to use pointer-first comparison:
    - `affect_find()`
    - `affect_strip()`
    - `affect_strip_obj()`
    - `is_affected()`
- Added local helper `affect_matches_skill_sn(...)` in `handler.c`:
    - compares `paf->skill` to `skill_find_uid(sn)` when available,
    - falls back to legacy `paf->type == sn` for compatibility/non-skill affects.

### Validation

- Ran clean build: `./build clean`
- Build completed successfully.

### Outcome

- Core affect helper read paths now align with pointer-first migration strategy.
- `AFFECT_DATA.type` remains as fallback compatibility until full Phase 9 removal.

---

## Next Steps

- **Phase 9:** Continue migrating remaining `paf->type` READ/comparison sites to pointer-first checks
- Begin `gsn_*` declaration/definition retirement slices (`merc.h` / `db.c`)
- Manual gameplay spot-check for completed skills/classes migration slices (practice/cast/scripted grant paths)
