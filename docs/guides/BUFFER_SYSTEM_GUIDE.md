# BUFFER System Guide

## Purpose

The BUFFER subsystem is Sentience's dynamic string builder used broadly across command, formatting, and output paths. This guide describes the current architecture, API contract, failure semantics, and the old-vs-new migration trade-offs.

## Locations

- Implementation: `src/utils/buffer.c`
- Public API: `src/utils/buffer.h`
- Core tests: `src/tests/unit/buffer_function_cases_core.c`
- Permutation tests: `src/tests/unit/buffer_function_cases_permutations.c`
- Suite definitions:
  - `src/tests/data/unit/buffer_core_unit_tests.json`
  - `src/tests/data/unit/buffer_permutation_unit_tests.json`

## Data Model

`buf_type` tracks:

- `string`: allocated character storage
- `size`: allocated capacity (bytes)
- `len`: tracked current string length (bytes, excluding NUL)
- `state`: lifecycle/safety state (`BUFFER_SAFE`, `BUFFER_OVERFLOW`, `BUFFER_FREED`)

The tracked `len` avoids repeated full-string scans on append-heavy paths.

## Growth and Limits

- Buffers grow through tiered capacities via `grow_buf()`.
- Hard upper bound is enforced by `MAX_BUF_TOTAL`.
- Exceeding representable bounds or growth tiers sets `BUFFER_OVERFLOW` and fails mutation calls.
- Overflowed buffers are fail-closed for mutation until cleared.

## Public API Contract

### Constructors / lifecycle

- `new_buf()`
  - Creates a safe, empty buffer at or above `BASE_BUF` capacity.
- `new_buf_size(int size)`
  - Floors requests below `BASE_BUF`.
  - Fails hard (process exit) if requested/derived size cannot be represented by configured tiers.
- `free_buf(BUFFER *buffer)`
  - Invalidates and recycles the handle.
  - Post-free use is considered misuse; guarded APIs fail safely.

### Mutation functions

- `add_buf_char(BUFFER *buffer, char ch)`
  - Returns `false` on invalid/freed/overflowed buffer or growth failure.
  - Returns `true` on successful append.
- `add_buf(BUFFER *buffer, const char *string)`
  - Returns `false` on invalid/freed/overflowed buffer or growth failure.
  - `string == NULL` and empty string are successful no-ops.
- `bprintf(BUFFER *buffer, const char *fmt, ...)`
  - Returns `false` on invalid/freed/overflowed buffer, `fmt == NULL`, formatting failure, or growth failure.

### Reset and accessors

- `clear_buf(BUFFER *buffer)`
  - Resets valid buffers to empty safe state.
  - No-op on invalid/freed handles.
- `buf_string(BUFFER *buffer)`
  - Returns internal string for valid buffer.
  - Returns a stable empty string fallback for invalid/freed/null handle.
- `buf_len(BUFFER *buffer)` / `buf_capacity(BUFFER *buffer)` / `buf_remaining(BUFFER *buffer)`
  - Return `0` for invalid/freed/null handles.

## Old vs New Approach

### Legacy model (old)

- BUFFER implementation lived inside `mem.c`.
- Core append paths depended on `strlen`, `strcpy`, and `strcat` behavior.
- Growth and copy paths were less centralized.
- Fewer explicit misuse guards.

### Hardened model (new)

- BUFFER extracted into dedicated module (`utils/buffer.*`).
- Length-aware append logic using tracked `len`.
- Growth centralized in `grow_buf()` with cap and overflow guards.
- Defensive null/invalid/freed-handle behavior standardized across public API.
- Unit coverage split into core and permutation suites for maintainability.

## Pros / Cons and Trade-offs

### What we gained

- Stronger safety invariants under malformed input and misuse.
- More predictable fail-closed behavior for mutation paths.
- Better maintainability from module isolation and focused tests.
- Better append-path efficiency from avoiding repeated string scans.

### What changed or was lost

- Some permissive legacy call patterns now fail explicitly.
- Slightly larger code and test surface area.
- Additional guard logging can surface caller misuse that was previously silent.

## Testing Expectations

Core suite validates:

- default construction and base-floor behavior
- append/format success paths
- overflow behavior and recovery via `clear_buf`
- null-argument and freed-handle guards
- limit-boundary behavior and capacity invariants

Permutation suite validates:

- operation sequence invariants across mixed append/clear combinations

Run unit tests:

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test:unit
```

## Migration Notes for Callers

- Treat `false` returns from mutation APIs as required control-flow branches.
- Do not assume post-free handles can be reused.
- Prefer explicit `clear_buf()` for reuse instead of free/recreate loops where appropriate.
- Use accessor helpers (`buf_len`, `buf_remaining`) rather than recomputing from `buf_string`.
