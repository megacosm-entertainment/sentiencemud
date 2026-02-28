# Worklog: Decomplexification

Tracks periodic progress against the high-cyclomatic-complexity list from
`docs/AUDIT_UNSAFE_FUNCTIONS.md`.

## Reporting cadence

- Add an entry after each focused decomplexification session.
- Include: touched functions, why selected, risk level, build/test status.
- Re-run a complexity scan (`pmccabe`) periodically and record deltas.

## Baseline snapshot (from audit)

Reference: `docs/AUDIT_UNSAFE_FUNCTIONS.md` (2026-02-26 snapshot)

- Functions CC > 200: 13
- Functions CC > 100: 56
- Notable targets:
  - `script_varseton` (693) in `scripts.c`
  - `do_quest` (407) in `quest.c`
  - `compile_script` (291) in `script_comp.c`
  - `do_look` (179) in `act_info.c`
  - `do_score` (108) in `act_info.c`
  - `show_char_to_char_0` (106) in `act_info.c`

## Session log

### 2026-02-27 — Session A (act_info.c safety-first extractions)

#### Scope

- `show_char_to_char_0` in `act_info.c`
- `do_score` in `act_info.c`

#### Changes

- `show_char_to_char_0`
  - Extracted quest marker logic into a helper.
  - Extracted position rendering switch into a helper.
  - Extracted aura rendering block into a helper.
- `do_score`
  - Extracted AC bar generation + output into helper functions.
  - Extracted repeated border rendering into a helper.

#### Why these targets

- Both are on the high-CC list and are player-facing display logic.
- Display-only extraction has low gameplay risk and high readability gains.

#### Validation

- Debug build passes after each refactor slice.
- Diagnostics clean except pre-existing `sprintf` overflow warning in `do_who_new`.

#### Next candidates

- Continue `do_score` with repeated padding helper extraction.
- Then move to `do_look` (`act_info.c`) for larger CC reduction.

### 2026-02-27 — Session B (periodic report + micro-reduction)

#### Scope

- Added periodic report artifact for decomplexification tracking.
- Further reduced duplication in `do_score` (`act_info.c`).

#### Changes

- Added `score_pad_to()` and replaced repeated in-function padding loops.
- Kept all output formatting semantics intact (same fixed-width alignment behavior).

#### Validation

- Debug build passes.
- Existing warning remains in `do_who_new` (`sprintf` potential overflow estimate), unchanged by this session.

#### Next candidates

- Continue `do_score` by extracting grouped row rendering helpers.
- Start first decomposition slice in `do_look` after `do_score` is stabilized.

### 2026-02-27 — Session C (shared utility extraction)

#### Scope

- Reduce repeated table-formatting patterns by introducing reusable helpers.

#### Changes

- Added `utils/tablefmt.h` + `utils/tablefmt.c`.
- New helpers:
  - `tablefmt_append_repeat()`
  - `tablefmt_pad_visible()`
  - `tablefmt_build_border()`
- Updated `act_info.c` score display helpers to use the shared module.
- Synced build systems:
  - `Makefile` includes `utils/tablefmt.c`
  - `CMakeLists.txt` includes `utils/tablefmt.c`

#### Why this matters

- We were repeatedly re-implementing visible-width padding and border loops.
- Centralizing this in `utils/` lowers repeated complexity and gives future table-style UIs a common, safe foundation.

#### Validation

- Debug build passes.
- Remaining warning in `do_who_new` (`sprintf` potential overflow) is pre-existing and unchanged.

### 2026-02-27 — Session D (table system foundation)

#### Scope

- Build out a proper table rendering utility with configurable borders,
  automatic padding, and wrapped multi-line rows.
- Migrate one production command to prove usage (`do_areas`).

#### Changes

- Expanded `utils/tablefmt` with renderer API:
  - column model (`width`, `wrap`, `align`)
  - style model (cell separators, horizontal rule tokens, padding, line endings)
  - `tablefmt_add_hr()` for border lines
  - `tablefmt_add_row()` for row rendering with wrapped multi-line cells
  - `tablefmt_style_default()` for baseline configuration
- Kept earlier low-level helpers (`append_repeat`, `pad_visible`, `build_border`) for flexible composition.
- Migrated `do_areas` output from ad-hoc `sprintf` formatting to the shared table renderer.

#### Why this matters

- Removes repeated hand-written table loops and makes future command tables
  consistent.
- Supports wrapped cells now, which was a missing capability in prior formatting paths.

#### Validation

- Debug build passes after migration.
- Existing warning in `do_who_new` remains pre-existing and unrelated to this change.

### 2026-02-27 — Session E (`do_who_new` warning cleanup)

#### Scope

- Remove the long-standing `do_who_new` format truncation warning before further table migration.

#### Changes

- Replaced unbounded/ambiguous format segments in `do_who_new` with bounded `snprintf` field widths.
- Constrained status-tag segments and name/area/race/class fields so the compiler can prove safe bounds.

#### Validation

- Debug build passes with no `do_who_new` truncation warning.

### 2026-02-27 — Session F (`do_who_new` tablefmt migration)

#### Scope

- Replace ad-hoc line assembly in `do_who_new` with `tablefmt` row rendering.

#### Changes

- Migrated per-player row output in `do_who_new` to `tablefmt_add_row()`.
- Introduced a no-border style for compatibility with existing who-output feel.
- Consolidated state/status tags into a dedicated status column string.
- Removed manual post-line spacing and many repeated `add_buf` append branches.

#### Why this matters

- Moves a high-churn display path onto shared rendering primitives.
- Reduces local formatting complexity and keeps future changes table-driven.

#### Validation

- Diagnostics clean for `act_info.c`.
- Debug build passes.

### 2026-02-27 — Session G (`do_who_new` visual parity tweak)

#### Scope

- Restore stronger visual separation in `who` output while retaining tablefmt-backed rendering.

#### Changes

- Updated `do_who_new` row composition to reintroduce bracketed segments:
  - level cell as `[IMM]`/`[###]`
  - combined race/class/area block in brackets
- Kept name and status tags as separate table columns for readability.

#### Validation

- Debug build passes.

### 2026-02-27 — Session H (`move_char` helper extraction + tablefmt docs)

#### Scope

- Start decomplexification on `move_char` (`act_move.c`, CC 159 in audit snapshot).
- Document usage patterns for the shared table renderer.

#### Changes

- `move_char` extraction in `act_move.c`:
  - `move_char_validate_travel_mode()`
  - `move_char_calculate_base_move()`
  - `move_char_apply_cost_modifiers()`
  - `move_char_check_exhaustion()`
  - `move_char_echo_departure()`
  - `move_char_echo_arrival()`
- Main `move_char` flow now delegates movement-rule and messaging blocks to focused helpers while preserving trigger and movement semantics.
- Added `docs/TABLEFMT_GUIDE.md` with practical guidance for:
  - style/column setup,
  - row and horizontal-rule rendering,
  - wrapping/alignment,
  - visible-width behavior with color codes,
  - fail-closed error handling patterns.

#### Validation

- Debug build passes after refactor (`./build`).

### 2026-02-27 — Session I (`move_char` post-entry tail extraction)

#### Scope

- Continue `move_char` decomplexification by extracting the post-room-transfer tail.

#### Changes

- Added focused helpers in `act_move.c`:
  - `move_char_handle_room_entry()`
  - `move_char_move_followers()`
  - `move_char_run_entry_triggers()`
  - `move_char_run_post_entry_checks()`
- Main `move_char` now delegates:
  - arrival side effects (`look`, traps, sunlight, cart/event handling),
  - follower propagation,
  - entry trigger/greet sequence,
  - post-entry hazards and skill/progression checks.

#### Why this matters

- Shrinks the highest-churn section of `move_char` while preserving sequencing semantics.
- Makes future hardening/testing easier because each behavior cluster has a dedicated helper boundary.

#### Validation

- Debug build passes after extraction (`./build`).

### 2026-02-27 — Session J (`move_char` destination + preflight extraction)

#### Scope

- Continue high-CC reduction in `move_char` by extracting early control-flow branches.

#### Changes

- Added/used helper boundaries in `act_move.c` for:
  - direction normalization and script-trigger mediation (`move_char_normalize_direction`),
  - early movement gates (`move_char_pre_move_checks`),
  - hidden-exit visibility gating (`move_char_can_use_hidden_exit`),
  - destination resolution including `EX_VLINK` dungeon spawn paths (`move_char_resolve_destination`),
  - top-level context/exit acquisition (`move_char_validate_context`, `move_char_get_exit`),
  - hidden-exit reveal side effect (`move_char_reveal_hidden_exit`).
- Main `move_char` now reads as a sequence of helper calls for preflight and destination setup, reducing nested branching in the primary function body.

#### Why this matters

- Moves the heaviest early branching from inline code into focused, testable helpers.
- Keeps behavior stable while improving maintainability and lowering local cognitive load.

#### Validation

- Debug build passes after each extraction slice (`./build`).

### 2026-02-27 — Session K (`move_char` mid-body extraction)

#### Scope

- Continue `move_char` reduction through the remaining mid-function movement/transfer path.

#### Changes

- Extracted movement and transfer flow into helpers:
  - `move_char_prepare_movement_cost()`
  - `move_char_can_traverse()`
  - `move_char_pay_movement_cost()`
  - `move_char_handle_pre_transfer_interruptions()`
  - `move_char_transfer_room()`
  - `move_char_get_visible_destination()`
- Replaced inline branching blocks with helper call sequence while preserving order:
  1. resolve + visibility check destination,
  2. compute/check movement cost,
  3. obstacle gate,
  4. deduct movement,
  5. departure echo + interruption cleanup,
  6. room transfer.

#### Why this matters

- Further flattens `move_char` and isolates side-effect clusters for safer future hardening and unit/integration testing.

#### Validation

- Debug build passes after each extraction (`./build`).

### 2026-02-27 — Session L (`act_move.c` high-CC closeout)

#### Scope

- Finish the high-complexity target in `act_move.c` (`move_char`).

#### Changes

- Completed decomposition of `move_char` into focused helper boundaries for:
  - context/preflight,
  - exit/destination/visibility resolution,
  - movement-cost calculation and payment,
  - pre-transfer interruption handling,
  - room transfer,
  - entry effects, followers, triggers, and post-entry checks.
- Kept `move_char` as a compact orchestration function with sequencing preserved.

#### Validation

- `pmccabe act_move.c` now reports:
  - `move_char` at CC 12 (line ~970), down from audit baseline CC 159.
- Current highest CC functions in `act_move.c` are now below high-risk threshold (top observed: `do_hide` at CC 73).
- Debug build passes (`./build`).

### 2026-02-27 — Session M (`script_varseton` highest-target pivot)

#### Scope

- Pivot to the highest remaining CC target in the audit: `script_varseton` in `scripts.c`.

#### Changes

- Extracted top primitive/string operation cluster into:
  - `script_varseton_handle_basic_ops()`
- Extracted `mobile`/`mob` resolution branch into:
  - `script_varseton_handle_mobile_type()`
- Kept `script_varseton` behavior and dispatch semantics intact while reducing main-function nesting and branch volume.

#### Validation

- Debug build passes (`./build`).
- `pmccabe scripts.c` now reports:
  - `script_varseton`: CC 582 (down from 693 baseline)
  - helper splits: `script_varseton_handle_basic_ops` CC 67, `script_varseton_handle_mobile_type` CC 47.

### 2026-02-27 — Session M (`do_hide` decomplexification)

#### Scope

- Finish remaining high-complexity target in `act_move.c` by decomposing `do_hide`.

#### Changes

- Extracted object/self hide paths into dedicated helpers:
  - `do_hide_object_in_container()`
  - `do_hide_object_on_victim()`
  - `do_hide_pick_room_hint()`
  - `do_hide_object_in_room()`
  - `do_hide_object()`
  - `do_hide_self()`
- Reduced `do_hide` to a minimal dispatcher between object-hide and self-hide flows.

#### Validation

- Debug build passes (`./build`).
- `pmccabe act_move.c` now reports:
  - `do_hide` at CC 3.

### 2026-02-27 — Session N (`script_varseton` carry/worn/content extraction)

#### Scope

- Continue reducing the highest CC target: `script_varseton` in `scripts.c`.

#### Changes

- Added shared inventory branch helper:
  - `script_varseton_handle_inventory_type()`
  - Handles both `carry` and `worn` dispatch paths via a mode flag while preserving query semantics.
- Added content branch helper:
  - `script_varseton_handle_content_type()`
- Replaced inline `carry`, `worn`, and `content` blocks in `script_varseton` with helper delegation calls.
- Removed now-unused locals from `script_varseton` introduced by previous inlining.

#### Validation

- Debug build passes (`./build`).
- `pmccabe scripts.c` now reports:
  - `script_varseton`: CC 425 (down from prior 500; baseline 693).
  - helper splits include: `script_varseton_handle_inventory_type` CC 35, `script_varseton_handle_object_type` CC 61, `script_varseton_handle_mobile_type` CC 47, `script_varseton_handle_basic_ops` CC 67.

### 2026-02-27 — Session O (`script_varseton` token/var/id helper gate)

#### Scope

- Continue decomplexifying `script_varseton` by collapsing a cluster of lookup-style branches.

#### Changes

- Added `script_varseton_handle_entity_lookup_ops()` to handle:
  - `token`
  - `DICE`
  - `variable` / `var`
  - `idmobile`
  - `idobject`
  - `idplayer`
- Replaced six inline `else if` branches in `script_varseton` with a single delegated gate.
- Removed now-unused local variables in `script_varseton` after extraction.

#### Validation

- Debug build passes (`./build`).
- `pmccabe scripts.c` now reports:
  - `script_varseton`: CC 389 (down from prior 425; baseline 693).

### 2026-02-27 — Session P (`script_varseton` mid/late branch collapse)

#### Scope

- Continue reducing `script_varseton` toward the CC < 100 target by collapsing large branch families.

#### Changes

- Added helper gates and moved corresponding inline branches:
  - `script_varseton_handle_skill_resource_ops()`
    - handles `croom`, `skill`, `skillgroup`, `skillinfo`, `song`, `spell`, `liquid`, `material`, `lockstate`.
  - `script_varseton_handle_index_ops()`
    - handles `mobindex`, `objindex`, `tokenindex|tokindex`, `blueprint|bp`, `bpsection`, `dngindex`, `shipindex`.
  - `script_varseton_handle_world_entity_ops()`
    - handles `area`/`aregion`/`wilds`/`section`/`instance`/`dungeon`/`ship`/`quest`/`class`/`trainer` family.
  - `script_varseton_handle_reputation_and_list_ops()`
    - handles `reputation` family + `race` + `moblist` + `objlist`.
  - `script_varseton_handle_room_random_ops()`
    - handles `findpath`, `randmob`, `randroom`, `dungeonrand`, `instancerand`, `sectionrand`, `specialroom`.
  - `script_varseton_handle_room_type()`
    - handles all `room` forms (widevnum, room/exit, bllist/pllist selectors).
- Replaced each moved inline `else if` branch set with a single helper gate call in `script_varseton`.
- Removed stale locals made unused by extraction.

#### Validation

- Debug build passes (`./build`).
- `pmccabe scripts.c` now reports:
  - `script_varseton`: CC 94 (down from prior 389; baseline 693).

#### Outcome

- `script_varseton` is now below the campaign threshold (`CC < 100`).
