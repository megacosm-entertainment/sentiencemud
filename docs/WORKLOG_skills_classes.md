# Worklog: Skill and Class System Backport

**Started:** 2026-02-09
**Last Updated:** 2026-02-13

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
- Compatibility API: `skill_from_sn()`, `skill_sn()`
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

Updated all invocation call sites to pass `skill_from_sn(sn)` as first arg and appropriate `INVOC_*` constant as last arg:

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
- `skill_entry_insert()` — sets `entry->skill_data = skill_from_sn(sn)` for built-in skills
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
| `effects.c` | 4 | `af.type = gsn_*` → added `af.skill = skill_from_sn(af.type)` |
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
| `save.c` | 6 | After `paf->type = sn;` → `paf->skill = skill_from_sn(sn);` |
| `db.c` | 2 | Same |
| `io/json/json_char.c` | 2 | After `paf->type = json_integer_value(...)` → `paf->skill = skill_from_sn(paf->type);` |
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

## Next Steps

- **Phase 6:** Class system — data-driven classes with multi-classing
- **Phase 7:** OLC editors for skills and classes
- Gradual migration of `paf->type` READ sites to use `paf->skill` pointer
- Test the bootstrap by booting the server to verify JSON generation
