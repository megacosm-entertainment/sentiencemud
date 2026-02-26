# Entity Field Code Evolution Policy

**Status:** Active (Phase 0)  
**Scope:** Script entity field bytecode (`ENT_FIELD.code`) and table evolution in `script_const.c` / `scripts.h`

---

## Purpose

Define safe, compatible rules for evolving entity field tables while the script engine remains on byte-sized escape encoding.

This policy is the compatibility contract referenced by Phase 0 EEF-004.

---

## Hard Rules

1. **Encoding width is fixed in Phase 0**
   - Field codes remain `unsigned char` in `ENT_FIELD.code`.
   - No 16-bit migration is allowed under this policy.

2. **Code band is reserved and enforced**
   - Valid entity field code range is `ESCAPE_EXTRA` through `0xFF`.
   - Values below `ESCAPE_EXTRA` are invalid.
   - Runtime decoding of entity field bytes must use unsigned-byte semantics.

3. **Append-only semantic evolution**
   - Do not repurpose an existing field code to a new semantic meaning.
   - Existing script bytecode must continue to decode the same semantics.

4. **Alias compatibility is explicit**
   - Multiple names may map to the same code only for alias/deprecation compatibility.
   - New alias entries should include metadata (`description`, `deprecated`) where practical.

5. **Deprecation is additive**
   - Deprecated fields remain resolvable and functional during Phase 0.
   - Deprecation communicates migration intent; it is not removal.

---

## Validation and Diagnostics

Runtime validation is performed by `script_validate_entity_tables()` at startup and in integration tests.

Current checks include:
- Entity type registry range sanity and overlap detection
- Empty field-name detection
- Field code range validation against escape bands
- Field result-type range validation
- Duplicate field-name detection per table
- Field-code reuse diagnostics (warning-level) and pressure reporting

These checks provide the required collision/out-of-range observability for Phase 0.

---

## Change Checklist

When adding or changing entity fields:

1. Confirm code remains in legal band.
2. Preserve existing semantic meaning for existing codes.
3. If introducing alias compatibility, document intent via metadata.
4. Run: `./build tests` then `cd /sentience && ./sent -test:script_engine`.
5. Update Phase 0 audit notes if policy/coverage changes.

---

## Deferred Work (Post-Phase 0)

- 16-bit field code migration
- Structural runtime dispatch refactor
- Stricter automated collision classification between alias vs semantic conflict
