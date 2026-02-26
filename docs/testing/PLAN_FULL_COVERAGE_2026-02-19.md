# Full Coverage Test Plan (Session Snapshot)

Date: 2026-02-19
Owner: current testing effort on `feature/widevnum-migration`

## Purpose

Capture a practical, staged plan to reach broad automated coverage while preserving current CI behavior and bootstrap assumptions.

## Constraints and Rules

- Keep using the existing JSON-driven framework in `src/tests/`.
- Maintain module-based dispatch in `src/tests/framework/test_dispatcher.c`.
- New in-game fixture entities must be added only to `bootstrap_data/area/tests.json`.
- Cross-area interaction with existing zones (for realistic integration scenarios) is allowed.
- Keep `CMakeLists.txt` and `Makefile` synchronized for any added/removed test source files.
- Avoid expanding scope into unrelated refactors while building coverage.

## Current Status (Done)

- Centralized dispatch in `test_dispatcher.c` with domain module handlers.
- Pure-function tests moved out of `wnum_tests.c` into `tests/unit/pure_function_tests.c`.
- Added broad string helper coverage (transforms, formatting, edit helpers).
- Added non-pure editor-state tests and shared setup utilities:
	- `tests/framework/test_utils.h`
	- `tests/framework/test_utils.c`
- Added/updated test data suites under `tests/data/unit` and updated profile wiring.
- Stabilized string editor tests (mutable buffers for `string_add`, correct `@` terminator semantics).

## Known Remaining Issue

- Unit suite currently has one known failure in WNUM parsing expectations (`parse_widevnum("2#1")` behavior mismatch).

## Coverage Strategy

### Phase 1 — Stabilize Baseline (short-term)

1. Resolve the outstanding WNUM unit expectation mismatch.
2. Ensure all new string suites run cleanly under unit profile.
3. Confirm build parity for tests on both CMake and Make paths.

Exit criteria:
- Unit profile is fully green in local run.
- No test regressions introduced in bootstrap/CI profiles.

### Phase 2 — Core Utility Coverage Expansion

Priorities:
- Iterators/list traversal helpers.
- Name/argument parsing functions not yet covered.
- Common predicate/lookup helpers used by command paths.
- Additional string edge cases (length limits, malformed input, embedded control codes).

Exit criteria:
- Core utility modules have targeted unit suites with deterministic pass/fail output.

### Phase 3 — Command-Adjacent Integration Coverage

Priorities:
- Object lookup/selection flows in realistic room/inventory states.
- Editor command integration flows that rely on descriptor state.
- Bootstrap fixture validation cases using `tests.json` entities plus existing zones.

Exit criteria:
- Integration suites validate command-adjacent behavior with reproducible setup.

### Phase 4 — CI Hardening and Coverage Tracking

1. Introduce clear per-profile pass criteria (`bootstrap_ci`, `ci`, `development`).
2. Add optional coverage-instrumented runs (`./build coverage`) for trend tracking.
3. Add a simple rolling coverage ledger (functions/modules covered vs pending).

Exit criteria:
- Coverage progress is measurable and reviewed as part of ongoing test work.

## Tomorrow’s Starting Queue

1. Fix and lock down the remaining WNUM expectation failure.
2. Run unit profile and verify all string suites + WNUM pass.
3. Add next utility test batch (iterator + argument parsing helpers).
4. Update this plan with completed items and new blockers.

## Guardrails

- Keep tests deterministic (no external timing/network dependence unless explicitly modeled).
- Reuse `test_utils` for descriptor/character setup instead of per-suite ad hoc setup.
- Prefer small, focused suites over large monolithic JSON files.
- When adding runtime fixture entities, document intent in test JSON descriptions.

## Notes for Deferred Cleanup

- Legacy text-format deprecation (`smash_tilde` removal and legacy readers) is intentionally deferred.
- Complete test coverage milestones first, then revisit migration/removal with confidence.
