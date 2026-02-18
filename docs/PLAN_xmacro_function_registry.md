# Plan: X-Macro Registry for Spell & Command Function Registration

## Problem

Adding a new spell or command function currently requires editing 2-3 files:

**For spells:**
1. `magic.h` — add `DECLARE_SPELL_FUN(spell_foo);` forward declaration
2. `skill_data.c` — add `{ "spell_foo", spell_foo }` to `spell_func_table[]`
3. (Optional) `const.c` — add entry to legacy `skill_table[]`

**For commands:**
1. `interp.h` — add `DECLARE_DO_FUN(do_foo);` forward declaration
2. `tables.c` — add `{ "do_foo", do_foo }` to `do_func_table[]`
3. `interp.c` — add entry to `cmd_table[]` (bootstrap only)

The lookup tables (`spell_func_table`, `do_func_table`) exist because C has no reflection — JSON files store function names as strings, so a string-to-function-pointer mapping is required at boot. The header declarations are needed because the tables reference functions defined in other translation units.

### Current Drift

Because these lists are maintained manually in parallel, they've drifted:
- ~20 spell functions declared in `magic.h` but missing from `spell_func_table[]` — these cannot be resolved from JSON skill data
- ~94 command functions declared in `interp.h` but missing from `do_func_table[]` — these are invisible to `cmdedit` and JSON serialization
- Duplicate entries in `do_func_table[]` (e.g., directional commands listed twice)
- Inconsistent formatting across the declaration files

## Proposed Solution: X-Macro Include Files

The X-macro pattern lets you define each function name **once** in a registry file, then `#include` it in multiple contexts with different macro definitions to generate both forward declarations and lookup table entries from the same list.

### New Files

**`spell_registry.inc`** — one `SPELL_ENTRY(name)` per spell function, sorted alphabetically (~175 entries). `spell_null` excluded as a special sentinel.

**`do_func_registry.inc`** — one `DO_FUNC_ENTRY(name)` per command function, sorted alphabetically (~570 entries), deduplicated.

### How It Works

In `magic.h` (generates forward declarations):
```c
#define SPELL_ENTRY(fun) DECLARE_SPELL_FUN(fun);
#include "spell_registry.inc"
#undef SPELL_ENTRY
```

In `skill_data.c` (generates lookup table entries):
```c
static const SPELL_FUNC_ENTRY spell_func_table[] = {
    { "spell_null", spell_null },
#define SPELL_ENTRY(fun) { #fun, fun },
#include "spell_registry.inc"
#undef SPELL_ENTRY
    { NULL, NULL }
};
```

The `#fun` stringification means the table entry name always matches the actual function — no chance of typos causing a mismatch.

Same pattern applies for `do_func_registry.inc` in `interp.h` and `tables.c`.

### After This Change

**Adding a new spell:** write the implementation, add one line to `spell_registry.inc`. Done.
**Adding a new command:** write the implementation, add one line to `do_func_registry.inc`. Done.

## Files Modified

| File | Action |
|------|--------|
| `spell_registry.inc` | **NEW** — ~175 entries |
| `do_func_registry.inc` | **NEW** — ~570 entries |
| `magic.h` | Replace ~173 `DECLARE_SPELL_FUN` lines with 3-line include block |
| `skill_data.c` | Replace ~152 `spell_func_table` entries with 3-line include block |
| `interp.h` | Replace ~560 `DECLARE_DO_FUN` lines with 3-line include block |
| `tables.c` | Replace ~502 `do_func_table` entries with 3-line include block |
| `merc.h` | Remove ~12 bare `void do_*()` declarations (moved to registry) |

**Not changed:** `cmd_table[]` in `interp.c` (bootstrap-only, has per-command metadata), `CMakeLists.txt` / `Makefile` (`.inc` files are included by the preprocessor, not compiled separately).

## Implementation Phases

**Phase 1: Spells** (smaller, lower risk)
1. Build sorted union of `magic.h` declarations + `spell_func_table` entries into `spell_registry.inc`
2. Replace `magic.h` declaration block with include
3. Replace `spell_func_table` entries in `skill_data.c` with include
4. Build and test

**Phase 2: Commands** (larger)
1. Build sorted, deduplicated union of `interp.h` + `do_func_table` + `merc.h` bare declarations into `do_func_registry.inc`
2. Replace `interp.h` declaration block with include
3. Replace `do_func_table` entries in `tables.c` with include
4. Remove bare declarations from `merc.h`
5. Build and test

## Alternatives Considered

| Approach | Pros | Cons |
|----------|------|------|
| **X-Macro (chosen)** | Pure C preprocessor, no deps, single source of truth | Still requires one manual line per function |
| **Code generation script** | Truly automatic (parse source for functions) | New build dependency, harder to debug, fragile parsing |
| **`__attribute__((constructor))`** | Per-file self-registration, no central list | Non-portable, init order issues, harder to debug |
| **Linker sections** | Elegant (Linux kernel style) | Complex, non-portable, overkill for this scale |
| **Status quo** | No work required | Ongoing drift, 3 files to edit per function |

## Net Impact

- ~660 net line reduction (add ~740 registry lines, remove ~1400 duplicated lines)
- Eliminates an entire class of bug (declaration/table drift)
- Fixes ~94 missing command function registrations and ~20 missing spell registrations
- No build system changes required
