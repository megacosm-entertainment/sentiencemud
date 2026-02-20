# Consolidated Script Engine Audit & Execution Plan

## Status

- **Date**: 2026-02-19
- **Owner**: Script/engine workstream
- **Intent**: Single canonical audit + implementation roadmap
- **Sequencing note (2026-02-20)**: Pause additional script-helper micro-hardening
  outside critical crash/safety fixes until Phase 2 command consolidation is
  complete. Resume hardening immediately after Phase 2 using current regression
  baselines (`script_engine` focused + bootstrap profile), to avoid duplicate
  edits on code paths scheduled for consolidation.
- **Supersedes (planning authority)**:
  - `docs/PLAN_SCRIPT_ENGINE_AUDIT.md`
  - `docs/PLAN_SCRIPT_COMMANDS.md`
  - Script-related roadmap sections in `docs/SCRIPT_SYSTEM_ANALYSIS.md`
  - Script editor modernization planning in `docs/SCRIPT_EDITOR_FRAMEWORK.md`

### Execution update (2026-02-20)

- **Phase 2 / Tranche A complete**: var-family command dispatch (`varset`, `varseton`,
  `varclear`, `varclearon`, `varcopy`, `varsave`, `varsaveon`) is now routed through
  shared `scriptcmd_var*` handlers in command tables across mob/object/room/token
  script spaces.
- **Validation**: debug build + focused `script_engine` regression suite passing.
- **Dead Lua module removal approved and executed**:
  - `script_lua.c` removed from active `src/` tree (it was not part of CMake/Make source lists).
  - obsolete `SCRIPT_LUA` script flag define removed from active `scripts.h`.
  - no active `src/` references to `script_loadlua` / `script_freelua` / `execute_lua_script`.
- **Next up**: begin Phase 2 / Tranche B (`transfer`/`goto`/`force`/`echo` families) via
  shared resolver/helper extraction, preserving behavior parity.

### Tranche B kickoff note (2026-02-20)

- Implemented first Tranche B step by centralizing `goto` dispatch to shared
  `scriptcmd_goto` for mob/object/token command tables.
- Preserved existing per-context behavior within shared implementation
  (movement target resolution and movement semantics by host type).
- Validation remains green (`script_engine` focused suite).

### Phase 1 kickoff note (2026-02-20)

- Added trigger-context regression coverage for Phase 1 entry points:
  - string trigger wildcard fallback + mixed-owner guard behavior
  - number trigger wildcard fallback + mixed-owner guard behavior
- Implemented in `src/tests/integration/script_engine_tests.c` and
  `src/tests/data/integration/script_engine_tests.json`.
- Validation target: `./sent -test:script_engine`.

### Phase 1 tranche update (2026-02-20)

- Added additional trigger-context regressions for:
  - sight-trigger slot-mismatch guard (`test_number_sight_trigger`)
  - `p_direction_trigger` wrapper dispatch execution path
  - `p_greet_trigger` wrapper dispatch execution path
- Extended focused `script_engine` suite to 18 tests, all passing.

---

## 1) Objectives

This plan consolidates all script-engine work into one path with four primary outcomes:

1. **Correctness & safety** of trigger dispatch, variable resolution, and command parsing
2. **Efficiency** (runtime and maintenance), especially command-surface deduplication
3. **Script editor usability** for builders (better show/error flow, less friction)
4. **Selective backports from `src_20_dev`** where compatibility/risk is acceptable

A key delivery constraint is completing command/ifcheck consolidation needed to finish and stabilize **widevnum migration behavior** in the scripting layer.

---

## 2) Baseline Findings (Consolidated)

### Confirmed in current branch (`src/`)

- Script command logic is still split across `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, and `script_tpcmds.c` with substantial overlap.
- Shared `scriptcmd_*` consolidation exists but is incomplete.
- Script ifchecks and command stubs remain partially modernized (class/trait/song/skill gaps documented in prior plans).
- Event command surface exists (`scriptcmd_event`), but there is no dedicated **event-prog (`eprog`) script space** yet.

### Confirmed in dev branch (`src_20_dev/`)

- Custom trigger system (“xtriggers”) exists with user-facing management hooks and dynamic trigger entries.
- Trigger model includes custom trigger identity beyond static built-in tables.
- Additional command consolidation and extended scripting features exist (requires selective intake).

---

## 3) Decision Policy

### Backport acceptance rules

A feature from `src_20_dev` is accepted only if all are true:

1. **Correctness gain** or clear usability gain
2. **No hidden dependency explosion** (or dependencies are explicitly included in tranche)
3. **Widevnum compatibility** is preserved or improved
4. **Regression tests** can be added for new/changed behavior
5. **Operational safety** is maintained (no unbounded script power without controls)

### Compatibility policy

- Prefer preserving current script behavior unless behavior is provably buggy, unsafe, or blocking migration.
- If behavior changes, provide migration notes and guardrails.

---

## 4) Target Architecture (Pragmatic)

### A. Command surface consolidation

- Move duplicate `do_mp*`/`do_op*`/`do_rp*`/`do_tp*` logic into shared `scriptcmd_*` handlers.
- Keep type-specific files primarily as registration/compat wrappers.
- Standardize:
  - argument expansion error handling
  - `lastreturn` semantics
  - null/type guards
  - room/entity resolver helpers

### B. Trigger system modernization

- Maintain fast runtime dispatch (type-int + slot bucketing).
- Add a dynamic custom-trigger registry (xtrigger-style) only with:
  - stable persistence format
  - usage tracking
  - scriptability controls
  - explicit slot-pressure monitoring

### C. Variable/ifcheck modernization

- Normalize variable semantics across owner contexts.
- Fill known ifcheck and command stubs for class/race/skill/song/trait systems.
- Add missing checks/commands only after parser/runtime parity rules are defined.

### D. Script editor usability

- Unify script editors on common OLC processing path.
- Improve script show/diagnostics with compile/runtime error visibility.
- Add standalone show/inspection commands where useful for staff workflows.

### E. Event scripting expansion (`eprogs`)

- Add as a **separate scoped workstream** after command-core consolidation.
- First deliverable is a design + compatibility proposal, then a thin proof of concept.

---

## 5) Phased Execution Plan

## Phase 0 — Audit Baseline & Compatibility Contract

**Goal**: freeze terminology and expected behavior before heavy code movement.

**Current Phase 0 starting point**: `docs/AUDIT_PHASE0_ENTITY_ENCODING_EXPANSION.md`
prioritizes entity encoding/expansion safety and compile-time lookup efficiency
as the first major tranche.

- Build issue catalog for:
  - trigger fallback/context integrity
  - variable ownership resolution
  - command parser inconsistencies
  - widevnum argument handling in script APIs
- Define compatibility matrix for legacy scripts.

**Exit criteria**:
- Single issue tracker doc with severity/repro/owner
- Script compatibility policy approved

## Phase 1 — Correctness First (No broad refactor yet)

**Goal**: fix high-risk behavioral bugs before architecture changes.

- Audit and test:
  - `test_string_trigger`
  - `test_number_trigger`
  - `test_number_sight_trigger`
  - `test_vnumname_trigger`
- Ensure wildcard fallback preserves correct owner/context.
- Add targeted regression tests for expansion context and random-room/entity edge cases.

**Exit criteria**:
- Reproducible trigger-context test coverage in `src/tests`
- No known high-severity context leakage bugs

## Phase 2 — Command Consolidation (Widevnum-critical)

**Goal**: reduce duplication and centralize behavior required for final widevnum stabilization.

- Tranche A: migrate trivial wrappers (varset/varclear/varcopy families) to shared handlers.
- Tranche B: migrate room-resolution heavy commands (transfer/goto/force/echo families) via shared resolver helpers.
- Tranche C: migrate large duplicated switch-based commands (alter/addaffect families) into shared core.
- Keep command registration in all relevant prog tables while reducing implementation spread.

**Exit criteria**:
- Major duplicate command implementations removed
- Behavior parity verified by script integration tests
- Widevnum-sensitive command parsing centralized

## Phase 3 — Script Ifcheck/Command Gaps (Class/Race/Skill/Song/Trait)

**Goal**: complete known functional gaps captured in earlier planning.

- Implement/add/fix ifchecks:
  - `hasclass`, `isclass`, `classlevel`, `classcount`
  - `hastrait`, `traitint`, `traitstring`
  - `hasskill`, `hassong`
  - resolve legacy `class` ifcheck behavior (alias/deprecate)
- Fill command stubs:
  - `setclass`, `setrace`, resolve/deprecate `setsubclass`
- Add new commands where approved:
  - `grantclass`, `revokeclass`, `grantsong`, `revokesong`, `settrait`
- Modernize `grantskill`/`revokeskill` APIs toward data-pointer based flow where feasible.

**Exit criteria**:
- No documented script stubs left for approved commands
- All new commands registered across intended prog spaces

## Phase 4 — Selective `src_20_dev` Backport Tranche

**Goal**: import high-value features with bounded risk.

### Priority 1
- Additional command consolidation patterns already proven in 2.0
- Trigger metadata shape improvements needed by custom trigger management

### Priority 2
- xtrigger/custom-trigger subsystem (with safety gates)

### Priority 3
- Additional variable types and low-risk ifchecks

**Exit criteria**:
- Backport matrix completed with Accepted/Deferred/Rejected decisions
- Accepted items landed with tests + migration notes

## Phase 5 — Script Editor UX Modernization

**Goal**: make script authoring/debugging easier for builders and staff.

- Move script editors to common OLC command processing framework.
- Improve show output and error visibility.
- Surface compile/runtime script diagnostics in editor workflow.
- Add standalone inspection commands (`*show`/error viewers) if retained by design.

**Exit criteria**:
- Reduced editor boilerplate
- Script errors visible without log-file spelunking

## Phase 6 — Event Script Space (`eprogs`) Design + Pilot

**Goal**: determine whether event system warrants first-class script type.

- Produce design for `eprog` trigger/dispatch model.
- Define command-space policy (reuse shared core; avoid new duplication).
- Implement minimal pilot if approved.

**Exit criteria**:
- Approved design doc + go/no-go decision
- If go: pilot merged behind controlled exposure

---

## 6) Backport Candidate Matrix (Initial)

| Candidate | Value | Risk | Initial Decision |
|---|---|---|---|
| Command consolidation patterns from 2.0 | High | Low-Med | **Adopt early** |
| Dynamic custom triggers (xtriggers) | High | Med | **Adopt with guardrails** |
| Trigger permission model (`progs`-style capability model) | High | Med | **Adopt if migration cost is bounded** |
| Extra variable types from 2.0 | Med-High | Med | **Selective adopt** |
| Broad ifcheck bulk import | Med | Med-High | **Selective adopt** |
| Large signature/ABI-wide changes without immediate payoff | Low-Med | High | **Defer** |

---

## 7) Efficiency Work (Parallel, low risk)

These can run in parallel where isolated:

- Reduce avoidable script runtime allocations in hot paths.
- Add early short-circuit checks for entities with no relevant programs.
- Keep compile-time lookup improvements as secondary unless profiling shows need.

---

## 8) Test Strategy

- Expand integration tests first for behavior-sensitive scripting paths.
- Add regression fixtures for:
  - wildcard fallback owner correctness
  - widevnum argument parsing and cross-area references
  - command parity across mob/obj/room/token contexts
  - custom-trigger lifecycle (if xtrigger tranche lands)

Definition of done for each phase includes test coverage for changed surfaces.

---

## 9) Immediate Next Actions (Start Here)

1. Execute entity encoding/expansion tranche from `docs/AUDIT_PHASE0_ENTITY_ENCODING_EXPANSION.md` (EEF-001 then EEF-005).
2. Continue issue catalog expansion with trigger-context hotspots (`test_vnumname_trigger` first).
3. Land first command-consolidation tranche (trivial wrappers) to establish shared helper patterns.
4. Write backport decision table for xtriggers + variable types with explicit dependencies.
5. Draft `eprog` design brief (scope, trigger model, command policy) before implementation.

---

## 10) Notes for Widevnum Completion

- All script-facing command argument parsing must converge on shared WNUM-aware resolvers.
- Avoid re-introducing per-prog-type parser forks.
- Backports that touch trigger/command metadata must be evaluated for WNUM compatibility before merge.
