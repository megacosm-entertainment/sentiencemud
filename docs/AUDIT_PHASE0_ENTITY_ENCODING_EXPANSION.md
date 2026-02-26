# Phase 0 Audit Catalog: Entity Field Encoding & Expansion

**Date:** 2026-02-19  
**Scope:** Script compiler/runtime entity encoding and expansion pipeline  
**Related canonical roadmap:** `docs/PLAN_SCRIPT_AUDIT_CONSOLIDATED.md`

---

## Purpose

This is the first Phase 0 issue catalog, focused on the entity field encoding and expansion subsystem to reduce risk and unlock faster work in later phases (command consolidation, xtrigger/backport intake, editor modernization).

---

## Phase 0 Status

**Overall:** Complete

- EEF-001: Complete
- EEF-002: Complete
- EEF-003: Complete
- EEF-004: Complete
- EEF-005: Complete
- EEF-006: Complete
- EEF-007: Complete

---

## In-Scope Surfaces

- Encoding primitives and field metadata in `scripts.h`
- Entity/ifcheck lookup paths in `scripts.c`
- Entity compile path in `script_comp.c`
- Runtime expansion dispatch in `script_expand.c`
- Field table registry in `script_const.c`
- Compatibility comparisons with `src_20_dev`

---

## Issue Register

## EEF-001 — Fixed compile buffer with unbounded writes

- **Severity:** High (correctness/safety)
- **Evidence:** `compile_string()` uses stack buffer `char buf[MSL*2+1]` while `compile_substring()` appends encoded bytes via `*p++` without explicit capacity checks.
- **Where:** `script_comp.c` (`compile_string`, `compile_substring`)
- **Risk:** Long or heavily escaped script lines can overflow compile-time buffer and corrupt memory.
- **Repro strategy:** Feed an oversized script line with repeated nested `$()` and `$[]` expansions plus quoted substrings; observe memory safety tooling or crash behavior.
- **Recommendation:** Replace fixed stack assembly with capacity-tracked growable buffer (or bounded append with hard failure path).
- **Acceptance criteria:**
  - Oversized inputs fail cleanly with compile error (no overflow)
  - Normal scripts compile unchanged
  - Regression test includes oversized-line case

## EEF-002 — O(n) entity field lookup at compile time

- **Severity:** Medium (performance/maintainability)
- **Evidence:** `entity_type_lookup()` linearly scans `ENT_FIELD[]` lists by name.
- **Where:** `scripts.c` (`entity_type_lookup`), called repeatedly from `script_comp.c` (`compile_entity`)
- **Risk:** Compile-time cost scales with field count and chained expressions; slows editing and bulk compiles.
- **Repro strategy:** Profile compile of large script corpus with deep `$()` chains.
- **Recommendation:** Introduce prebuilt hash index per entity field table (name → ENT_FIELD*), preserving current tables as source of truth.
- **Acceptance criteria:**
  - Lookup path switched to O(1)-average hash lookup
  - No behavior change in field resolution
  - Fallback diagnostics preserved for invalid fields

## EEF-003 — O(n) ifcheck lookup at compile time

- **Severity:** Medium (performance)
- **Evidence:** `ifcheck_lookup()` scans `ifcheck_table[]` linearly with type-mask checks.
- **Where:** `scripts.c` (`ifcheck_lookup`), invoked across boolean expression compile in `script_comp.c`
- **Risk:** Compile-time latency grows with ifcheck catalog expansion (including backports).
- **Recommendation:** Build indexed lookup by `(name, script_type_mask)` with deterministic fallback semantics.
- **Acceptance criteria:**
  - Equal functional behavior on valid/invalid ifchecks
  - Reduced lookup overhead for large ifcheck sets

## EEF-004 — Encoding namespace pressure from byte-sized field codes

- **Severity:** Medium-High (architecture)
- **Status:** Complete (Phase 0 scope)
- **Evidence:** `entity_field_type.code` is `unsigned char`; escape bands are fixed (`ESCAPE_*` + single-letter codes).
- **Where:** `scripts.h` (`entity_field_type`, escape constants), compile/expand bytecode usage.
- **Risk:** Long-term growth pressure and fragility in field code allocation strategy.
- **Recommendation (Phase 0):**
  1. Introduce guardrails first (append-only policy + validation checks + diagnostics)
  2. Defer encoding-width migration (`uint16_t`) to a scoped compatibility project once benefits justify migration cost.
- **Acceptance criteria (Phase 0):**
  - Automated validation catches collisions/out-of-range field codes
  - Documented compatibility policy for field code evolution

Implemented in this branch by combining startup/runtime validator diagnostics with a dedicated compatibility contract in `docs/POLICY_ENTITY_FIELD_CODE_EVOLUTION.md`.

## EEF-005 — Missing automated validation for field table integrity

- **Severity:** Medium (correctness)
- **Evidence:** No explicit startup/assert pass ensuring unique names/codes per table and legal code ranges.
- **Where:** entity table declarations in `script_const.c` + runtime registration in `entity_type_info[]`
- **Risk:** Silent table mistakes can produce wrong expansion behavior or ambiguous compile outcomes.
- **Recommendation:** Add startup integrity validator:
  - duplicate field name detection per table
  - duplicate code detection per table
  - illegal code band detection
  - entity type range sanity checks vs `entity_type_info[]`
- **Acceptance criteria:** Boot/log reports clear failures; CI/build test exercises validator.

## EEF-006 — Runtime expansion complexity concentrated in large switch chains

- **Severity:** Medium (maintainability/regression risk)
- **Status:** Complete (Phase 0 scope)
- **Evidence:** `script_expand.c` relies on many large `switch(*str)` dispatch blocks.
- **Risk:** High change surface for subtle behavior regressions while adding new fields/entities.
- **Recommendation:** Keep runtime model intact for now; add targeted tests before any structural refactor.
- **Acceptance criteria:** Regression harness covers representative chained expansions before large backports.

Implemented in this branch by adding script-engine regression coverage for nested variable-name expansion and mixed variable/expression chains in `src/tests/integration/script_engine_tests.c` and `src/tests/data/integration/script_engine_tests.json`.

## EEF-007 — Metadata gap versus 2.0 field descriptors

- **Severity:** Low-Medium (usability/tooling)
- **Status:** Complete (Phase 0 scope)
- **Evidence:** Current `entity_field_type` lacks description/deprecation metadata present in `src_20_dev`.
- **Risk:** Harder script help/discovery; weak deprecation communication.
- **Recommendation:** Selectively backport metadata fields (`description`, `deprecated`) after EEF-001..005 are stabilized.
- **Acceptance criteria:** Metadata is additive and non-breaking; optional tooling can display field docs.

Implemented in this branch by extending `entity_field_type` with optional metadata, adding accessor helpers, and validating metadata/deprecation behavior in script-engine integration tests.

---

## Compatibility Constraints (Phase 0 Contract)

1. Preserve script source semantics unless fixing confirmed bugs.
2. No encoding-width migration in Phase 0 (guardrails first).
3. Field additions must be append-only within existing bytecode model during Phase 0.
4. Any changed compile failure modes must produce explicit, actionable errors.

---

## First Tranche (Execution-Ready)

## Tranche A — Safety + observability (do first)

1. **Bounded compile assembly** in `script_comp.c` (`compile_string`/`compile_substring`)  
2. **Entity table validator** at startup (integrity checks + log output)  
3. **Diagnostic report command/log hook** summarizing field-count pressure per entity table

## Tranche B — Compile-time lookup performance

4. Hash index for `entity_type_lookup`  
5. Hash index for `ifcheck_lookup`

## Tranche C — Metadata uplift (optional Phase 0 tail)

6. Add optional field metadata (`description`, `deprecated`) with no runtime semantic changes

---

## Test Additions Required

- Compiler overflow/oversized-line regression case (safety)
- Compile parity cases for common `$()` chains (behavior lock)
- Invalid field and invalid ifcheck diagnostics (error-path stability)
- Lookup parity tests before/after indexing

---

## Proposed Immediate Work Order

1. Implement EEF-001 + tests
2. Implement EEF-005 + startup checks
3. Implement EEF-002/003 lookup indexing
4. Re-baseline compile timing and update this audit with measured deltas

---

## Deferred to Post-Phase 0

- Encoding width migration to 16-bit field codes
- Deep runtime dispatch refactor of `script_expand.c`
- Broad 2.0 surface backports tied to new entity families

These remain candidates once safety/perf guardrails are in place.
