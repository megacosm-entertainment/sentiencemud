# Script Engine Audit Plan

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


> Superseded by `docs/PLAN_SCRIPT_AUDIT_CONSOLIDATED.md` as the canonical
> script audit and execution roadmap.

## Goal
Perform a full end-to-end audit of the script engine for correctness, safety, maintainability, and selective backports from `src_20_dev`.

## Scope
- Runtime execution flow (`execute_script`, trigger dispatchers, loop/control opcodes)
- Entity/context propagation (`mob/obj/room/token/area/instance/dungeon`)
- Variable semantics (`varset`, ifchecks, entity-local vs caller vars)
- Trigger matching/fallback behavior (exact, wildcard, slot/type-all variants)
- Script command surface (parsing, validation, security gates, restricted commands)
- Cross-area WNUM behavior in script-facing APIs
- Test coverage and missing regression cases

## Principles
- Fix root causes, not symptom patches.
- Preserve behavior unless bug/security issue is confirmed.
- Keep `src` and tests in sync; avoid speculative refactors.
- Backport only when behavior and dependencies are understood.

## Workstreams

### 1) Trigger Dispatcher Integrity
- Review all trigger entry points:
  - `test_string_trigger`
  - `test_number_trigger`
  - `test_number_sight_trigger`
  - `test_vnumname_trigger`
- Verify owner context passed to `execute_script` in primary and wildcard fallback branches.
- Confirm token-attached script execution never leaks owner context.
- Add targeted regression tests for context-sensitive expansions (`$(self)`, room randomizers, instance commands).

### 2) Variable and Ifcheck Semantics
- Document intended semantics for:
  - `varset` / `varseton`
  - `vardefined`, `varbool`, `varnumber`, `varstring`
- Verify all ifchecks resolve variable storage consistently with the active script owner.
- Identify and classify any historical behavior that scripts may implicitly rely on.

### 3) Command/Parser Consistency
- Audit command handlers for:
  - type checks
  - null/valid guards
  - lastreturn behavior
  - argument expansion error paths
- Validate instance/dungeon-specific command requirements against runtime owner typing.

### 4) Backport Candidate Review (`src_20_dev`)
- Build a candidate matrix:
  - feature/bugfix summary
  - dependency/risk
  - behavior compatibility
  - test plan
- Prioritize low-risk, high-value fixes first (correctness > features).

### 5) Test Hardening
- Add regression tests for:
  - wildcard fallback context integrity
  - entity-type-specific random room resolvers
  - WNUM parsing in script command args
  - error-path stability (invalid entities/rooms/wnums)
- Extend integration tests before broad refactor/backport steps.

## Deliverables
- Audit findings document (issue list + severity + repro + fix recommendation)
- Patch series grouped by subsystem (dispatcher, vars, parser, commands)
- New/updated integration tests
- Backport decision matrix for `src_20_dev`

## Milestones
1. Baseline audit + issue catalog
2. High-severity correctness fixes + tests
3. Backport tranche #1 (low-risk fixes)
4. Secondary refactor opportunities (post-stabilization)

## Immediate Next Tasks
- [ ] Audit `test_vnumname_trigger` for fallback context consistency
- [ ] Add regression test coverage for wildcard fallback owner context
- [ ] Draft initial `src_20_dev` backport candidate table
- [ ] Define script-engine compatibility policy for legacy script behavior
