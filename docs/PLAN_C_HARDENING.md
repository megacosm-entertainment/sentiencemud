# Plan: C Hardening — The Stenberg Approach

**Created:** 2026-02-26
**Status:** Planning
**Inspiration:** Daniel Stenberg's cURL blog series on writing safer C
**Companion doc:** [AUDIT_UNSAFE_FUNCTIONS.md](AUDIT_UNSAFE_FUNCTIONS.md) — per-file breakdown

---

## Motivation

Daniel Stenberg has written extensively about the practices cURL uses to ship safe, reliable C code despite the language's sharp edges. These blog posts form the basis of this plan:

- **Writing C for curl** (2025-04-07) — Banned functions, coding standards, dynamic buffer discipline
- **Detecting malicious Unicode** (2025-05-16) — CI checks for confusable Unicode in source files
- **Decomplexification** (2025-05-29) — Cyclomatic complexity tracking, CI enforcement at CC≤100
- **Keeping tabs on curl's memory use** (2025-07-08) — Per-test allocation limits, struct size monitoring
- **More views on curl vulnerabilities** (2025-07-10) — Vulnerability patterns, fuzzing, static analysis
- **Parsing integers in C** (2025-11-13) — Banning atoi/strtol, strict custom parser
- **No strcpy either** (2025-12-29) — Banning strcpy, replacing with length-aware memcpy wrappers

The Sentience codebase has 25 years of heritage from the DikuMUD/MERC/ROM lineage. While it was never internet-facing in the way cURL is, active development means these patterns now carry real risk — buffer overflows from player input, silent integer parsing failures in OLC, and unmaintainable functions with cyclomatic complexity scores in the hundreds.

---

## Current State Audit Summary

Full per-file audit: [AUDIT_UNSAFE_FUNCTIONS.md](AUDIT_UNSAFE_FUNCTIONS.md)

### Unsafe Function Usage

| Category | Banned/Unsafe | Safe Alternative | Adoption % |
|----------|-------------:|----------------:|----------:|
| String formatting | 4,347 (`sprintf`) | 985 (`snprintf`) + 17 (`vsnprintf`) | 18.7% |
| String copy | 336 (`strcpy`) + 219 (`strncpy`) | 250 (`strlcpy`) | 31.1% |
| String concat | 1,147 (`strcat`) + 48 (`strncat`) | 48 (`strlcat`) | 3.9% |
| Integer parsing | 1,009 (`atoi`) + 169 (`atol`) + 20 (`sscanf`) | 8 (`strtol`) + 4 (`strtoul`) | 1.0% |
| Memory (raw vs managed) | 458 (malloc/calloc/realloc/free) | 3,660 (str_dup/free_string/alloc_perm) | 88.9% |

**Total banned function calls: ~7,300**

### Buffer System Adoption

The existing BUFFER system (`new_buf`/`add_buf`/`bprintf`) accounts for 2,094 calls — already widely used, but the buffer itself is built on unsafe primitives.

### Cyclomatic Complexity

| Metric | Value |
|--------|------:|
| Total functions analyzed | 5,752 |
| Average CC | 11.4 |
| Functions with CC > 100 | 56 |
| Functions with CC > 50 | 192 |
| Worst function | `script_varseton` at CC=693 |

cURL's threshold is CC ≤ 100. We have 56 functions above that line.

### Top 10 Most Complex Functions

| # | Function | CC | File |
|---|----------|---:|------|
| 1 | `script_varseton` | 693 | scripts.c |
| 2 | `do_quest` | 407 | quest.c |
| 3 | `compile_script` | 291 | script_comp.c |
| 4 | `fread_char` | 236 | save.c |
| 5 | `DECL_OPC_FUN` | 235 | scripts.c |
| 6 | `damage_new` | 219 | fight.c |
| 7 | `one_hit` | 210 | fight.c |
| 8 | `test_number_trigger` | 201 | scripts.c |
| 9 | `SCRIPT_CMD` (tpcmds) | 196 | script_tpcmds.c |
| 10 | `char_update` | 191 | update.c |

### What's Already Good

- **str_dup / free_string** (1,866 / 1,635 calls) — well-established string ownership pattern
- **alloc_perm** (159 calls) — custom persistent allocator
- **BUFFER system** with `bprintf` — exists and works, just needs hardening
- **strlcpy/strlcat** used in newer modules (channels, editors)
- **snprintf** used in newer code (985 calls)
- No `gets` calls anywhere

---

## Network Attack Surface

Sentience supports three connection protocols, each with different security implications:

| Protocol | File | Status | Risk Level |
|----------|------|--------|------------|
| Plain Telnet | connection_tcp.c | Production | Low (expected cleartext) |
| TLS (OpenSSL) | connection_tls.c | Production | Medium (TLS parsing, cert handling) |
| WebSocket | connection_websocket.c | Preliminary | High (HTTP upgrade, frame parsing, web-facing) |

The connection abstraction layer itself (`connection.c`, `connection_tcp.c`, `connection_tls.c`, `connection_websocket.c`) is **clean** — zero unsafe function calls in the transport layer.

However, the code that **processes data from these connections** is not:

| File | Role | Unsafe Calls | Key Risk |
|------|------|-------------:|----------|
| comm.c | I/O dispatch, input processing | 136 | 49 `strcpy` on data from network sockets |
| protocol.c | Telnet/MCCP negotiation | 79 | 50 `sprintf`, 19 `strcat` building protocol responses |
| nanny.c | Login, character creation | 174 | 152 `sprintf` on user-supplied names/passwords |

**Why this matters more with TLS and WebSockets:**

- **Telnet-only era:** The attack surface was limited to players on a private MUD port. Exploitation required a telnet client and MUD knowledge.
- **TLS:** Opens the door to automated scanning tools that probe TLS services. Any buffer overflow in `comm.c` that processes post-TLS-handshake data is now reachable by bots.
- **WebSocket:** Exposes the game to the entire web. Browser-based clients mean the server may face HTTP-layer attacks, malformed WebSocket frames, and cross-origin abuse. Any `strcpy` in the input processing path becomes a web-accessible vulnerability.

**Priority implication:** `comm.c`, `protocol.c`, and `nanny.c` should be migrated in Phase 3 Tier 1 (highest risk), ahead of game logic files. The connection layer files themselves need no changes — they're already safe.

---

## Phase 1: Harden the BUFFER System

**Priority:** Foundational — everything else builds on this
**Effort:** 1-2 days

### Implementation Status (2026-02-26)

Phase 1 is now implemented in the current tree:

Reference guide: [BUFFER System Guide](guides/BUFFER_SYSTEM_GUIDE.md)

- BUFFER code extracted into a dedicated module: `src/utils/buffer.c` / `src/utils/buffer.h`
- `buf_type` tracks `len` to avoid repeated `strlen()` on append
- `add_buf`, `add_buf_char`, and `bprintf` use length-aware append logic
- hard cap enforcement via `MAX_BUF_TOTAL` and `BUFFER_OVERFLOW` state
- additional guards for `size_t`/`INT_MAX` overflow boundaries
- null-argument/invalid-buffer guards on public BUFFER API entry points
- capacity introspection helpers available: `buf_len`, `buf_capacity`, `buf_remaining`
- unit coverage added and split for maintainability:
    - `buffer_core_unit_tests`
    - `buffer_permutation_unit_tests`

Remaining hardening phases (2-5) are still pending.

### Buffer Follow-ups Still Open

Phase 1 core safety behavior is implemented, but two high-value BUFFER follow-ups remain:

1. **Caller-side failure handling audit**
    - Many call sites invoke `add_buf` / `add_buf_char` / `bprintf` without checking return values.
    - Hardening benefit is limited unless callers propagate or handle failure paths.
    - Initial pass completed (2026-02-26):
      - `src/editors/areas/aedit.c` (`aedit_regions list` path)
      - `src/editors/blueprints/bpedit.c` (`section list` + `channel list` paths)
    - These paths now fail closed with explicit user-facing truncation errors when buffer mutation fails.
     - Additional pass completed (2026-02-26):
         - `src/editors/commands/cmdedit.c` (`do_cmdlist` render path)
         - `src/editors/dungeons/dngedit.c` (`dngedit_channel list` and `dngedit_floors list` paths)
     - Builder propagation pass completed (2026-02-26):
         - converted `dngedit_buffer_floors`, `dngedit_buffer_levels`, and `dngedit_buffer_special_exits` to return `bool`
         - wired call-site handling in `dngedit_show_floors_tab`, `dngedit_show_levels_tab`, `dngedit_show_special_tab`, and `dngedit_floors list`
     - Tab-output pass completed (2026-02-26):
         - guarded direct `add_buf` usage in `dngedit_show_general_tab`, `dngedit_show_special_tab`, `dngedit_show_variables_tab`, and `dngedit_show_notes_tab`
     - Local list-render pass completed (2026-02-26):
         - hardened weighted floor list rendering for `levels weight ... list`
         - hardened grouped weighted floor list rendering for `levels group ... weight ... list`
         - hardened `special room list` output path
         - replaced duplicated `special exit list` renderer with `dngedit_buffer_special_exits()` reuse
     - Readability consolidation pass completed (2026-02-26):
         - replaced repeated weighted `from/to` exit list render blocks with `dngedit_render_weighted_exit_list()`
         - replaced repeated weighted floor list render blocks with `dngedit_render_weighted_floor_list()`
         - replaced repeated special room render blocks with `dngedit_render_special_room_list()` and `dngedit_buffer_special_rooms_tab()`
         - replaced channel list append scaffolding with `dngedit_render_channel_list()`
         - removed remaining `append_ok` state plumbing in `dngedit.c` while preserving fail-closed semantics
     - Blueprint section tab pass completed (2026-02-26):
         - guarded direct `add_buf` usage in `bsedit_show_general_tab`, `bsedit_show_links_tab`, `bsedit_show_maze_tab`, and `bsedit_show_notes_tab`
     - Blueprint tab pass completed (2026-02-26):
         - guarded direct `add_buf` usage in `bpedit_show_general_tab`, `bpedit_show_sections_tab`, `bpedit_show_layout_tab`, `bpedit_show_variables_tab`, and `bpedit_show_notes_tab`

#### Caller-side hardening style rule (maintainability + readability)

When hardening BUFFER call sites, prefer this order:

1. **Harden first**: ensure all mutation calls fail closed and surface a clear user-facing error when output exceeds buffer limits.
2. **Consolidate second**: if a list/table renderer appears in multiple command paths, extract a local helper and keep message text unchanged.
3. **Minimize per-call noise**: avoid long `append_ok` chains in command handlers when a helper can own append, cleanup, and paging flow.
4. **Keep scope local**: refactor one editor/file at a time to avoid broad churn.

This keeps security guarantees intact while improving long-term maintainability and reviewability.

2. **Guard-log signal quality**
    - Invalid-buffer guards now surface misuse, but repeated call-site misuse can become noisy.
    - Add focused caller fixes before introducing any broader log-rate policy.

### Old vs New BUFFER Approach (Trade-off Record)

| Dimension | Old Approach (`mem.c` legacy) | New Approach (`utils/buffer.c`) |
|----------|-------------------------------|----------------------------------|
| Structure | BUFFER logic embedded in broader memory recycler file | Dedicated BUFFER module with clear API boundary |
| Length handling | Repeated `strlen()` scans during append | Tracked `len` field, O(1) append bookkeeping |
| String ops | `strcpy` / `strcat` internals | `memcpy` + bounded `vsnprintf` |
| Growth path | Manual allocate-copy-free flow | Centralized `grow_buf()` with `realloc` |
| Bounds policy | Soft tiering, limited global enforcement | Explicit cap (`MAX_BUF_TOTAL`) + overflow state |
| Failure semantics | Mixed legacy behavior by call path | Consistent fail-closed behavior for overflow/invalid args |
| Test coverage | Minimal direct BUFFER contract testing | Dedicated core + permutation suites, including negative paths |

#### Pros/Cons

**Old approach pros**
- Familiar legacy behavior with very low short-term churn.
- Fewer abstractions to navigate while debugging in a single file.

**Old approach cons**
- Banned/unsafe string primitives in core paths.
- Repeated `strlen()` overhead and weaker invariants.
- Harder to reason about failures and boundary behavior.
- BUFFER concerns mixed with unrelated recycler code.

**New approach pros**
- Better safety posture: overflow guards, cap enforcement, invalid-argument guards.
- More predictable contracts: mutation calls fail cleanly on invalid/overflowed buffers.
- Better maintainability: isolated module, focused tests, clear invariants.
- Better performance characteristics on append-heavy paths via length tracking.

**New approach cons**
- Slightly higher code surface area (module split + additional tests).
- Stricter behavior may expose latent caller misuse that previously went unnoticed.
- Extra guard logging can increase noise if callers frequently violate contracts.

#### What We Gained

- A test-backed BUFFER safety contract (normal, overflow, null-arg, and freed-handle misuse paths).
- Cleaner architectural separation for future Phase 2 helper adoption.
- Lower risk of accidental overflow/undefined behavior in string-building hot paths.

#### What We Lost / Changed

- Tolerance for some permissive legacy usage patterns (invalid/misused handles now fail instead of "best effort").
- A small amount of simplicity from having everything co-located in `mem.c`.
- Potentially quieter logs in misuse scenarios (now intentionally surfaced for hardening).

The BUFFER (`buf_type`) is the project's dynamic string builder, used ~2,094 times. It needs to be the safe foundation before we migrate unsafe calls to use it.

### Current Problems

1. **No length tracking** — `add_buf()` calls `strlen()` on every append (O(n))
2. **Uses banned functions internally** — `strcpy` and `strcat` inside `add_buf()` and `bprintf()`
3. **`add_buf_char` is wasteful** — creates a 2-byte temp string, calls full `add_buf`
4. **Growth path is manual** — `malloc` + `strcpy` + `free` instead of `realloc` or `memcpy`
5. **No hard maximum** — unbounded growth could exhaust memory

### Changes

**Add `len` field to `buf_type`:**
```c
struct buf_type
{
    BUFFER *    next;
    bool        valid;
    int16_t     state;
    size_t      size;    /* allocated capacity */
    size_t      len;     /* current string length */
    char *      string;
};
```

**Rewrite core operations using `len`:**
- `add_buf()` — use `memcpy(buf->string + buf->len, str, slen)` instead of `strcat`
- `add_buf_char()` — direct `buf->string[buf->len++] = ch; buf->string[buf->len] = '\0';`
- `bprintf()` — format at `buf->string + buf->len` via `vsnprintf`, no strlen
- Growth path — use `realloc()` or `memcpy` with known length, not `strcpy`
- `clear_buf()` — set `buf->len = 0`, zero the first byte
- Add `buf_len()` accessor for callers that need the length

**Add hard maximum:**
- `MAX_BUF_TOTAL` cap (e.g., 1MB or 2MB) to prevent runaway growth
- Return `false` / set `BUFFER_OVERFLOW` if exceeded

**Add `buf_remaining()` for callers that need to know capacity.**

---

## Phase 2: Safe String & Integer Helpers

**Priority:** High — needed before mass migration
**Effort:** 2-3 days

### Safe String Library (`safe_string.h` / `safe_string.c`)

Create project-wide replacements for banned functions, following cURL's model of wrapping length information with the operation:

```c
/**
 * Safe string copy with guaranteed NUL termination.
 * Returns number of characters copied (excluding NUL), or -1 on truncation.
 */
int sent_strlcpy(char *dst, const char *src, size_t dstsize);

/**
 * Safe string concatenation with guaranteed NUL termination.
 * Returns total length that would have been needed, or -1 on truncation.
 */
int sent_strlcat(char *dst, const char *src, size_t dstsize);

/**
 * Safe snprintf wrapper. Returns characters written (excluding NUL),
 * or -1 on truncation. Never returns >= dstsize.
 */
int sent_snprintf(char *dst, size_t dstsize, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
```

### Strict Integer Parser (`sent_parse_int` / `sent_parse_long`)

Following cURL's `curlx_str_number()` philosophy — strict by default:

```c
typedef enum {
    PARSE_OK = 0,
    PARSE_ERR_EMPTY,       /* empty or NULL string */
    PARSE_ERR_OVERFLOW,    /* value exceeds max */
    PARSE_ERR_UNDERFLOW,   /* value below min (if signed) */
    PARSE_ERR_INVALID,     /* non-digit characters */
    PARSE_ERR_TRAILING,    /* trailing non-digit characters */
} parse_result_t;

/**
 * Strict integer parser. No leading whitespace, no sign prefix,
 * no trailing garbage. Returns PARSE_OK on success.
 */
parse_result_t sent_parse_int(const char *str, int *out, int min, int max);
parse_result_t sent_parse_long(const char *str, long *out, long min, long max);
```

Why strict? Because `atoi("12abc")` silently returns 12, and `atoi("")` returns 0. In OLC editors, this means builders can enter garbage and get silent wrong values. A strict parser surfaces those errors immediately.

### Build System Updates

- Add `safe_string.c` to both `Makefile` and `CMakeLists.txt`
- `#include "safe_string.h"` from `merc.h` for project-wide availability

---

## Phase 3: Systematic Migration

**Priority:** Medium — bulk of the work
**Effort:** 4-6 weeks (can be done incrementally, file-by-file)

### Migration Order (by risk and impact)

#### Tier 1: Highest Risk (Week 1-2)

| Target | Count | Files | Strategy |
|--------|------:|-------|----------|
| `sprintf` in comm.c | 68 | comm.c | `snprintf` — network-facing code |
| `strcpy` in comm.c | 49 | comm.c | `sent_strlcpy` — network-facing code |
| `strcat` in bit.c | 476 | bit.c | Rewrite to use BUFFER or `sent_strlcat` |
| `sprintf` in act_info.c | 502 | act_info.c | `snprintf` or `bprintf` — player-facing output |
| `strcat` in act_info.c | 184 | act_info.c | `sent_strlcat` or BUFFER |

#### Tier 2: OLC Editors (Week 2-3)

| Target | Count | Strategy |
|--------|------:|----------|
| `atoi` in OLC editors | ~445 | `sent_parse_int` with error feedback |
| `atol` in OLC editors | ~70 | `sent_parse_long` with error feedback |
| `sprintf` in OLC editors | ~800 | `snprintf` or `bprintf` |

OLC is builder-facing code where bad input is common. Strict parsing will catch typos and give useful error messages instead of silently accepting garbage.

#### Tier 3: Game Systems (Week 3-4)

| Target | Count | Strategy |
|--------|------:|----------|
| `sprintf` in act_wiz.c | 457 | `snprintf` — immortal commands |
| `sprintf` in act_obj.c | 175 | `snprintf` — object handling |
| `sprintf` in fight.c | ~100 | `snprintf` — combat output |
| `atoi` in game logic | ~500 | `sent_parse_int` |

#### Tier 4: Scripting Engine (Week 4-6)

| Target | Count | Strategy |
|--------|------:|----------|
| `sprintf` in script_*.c | ~400 | `snprintf` or BUFFER |
| `atoi` in scripts.c | ~60 | `sent_parse_int` |
| `strcpy` in scripts | varies | `sent_strlcpy` |

The scripting engine is the most complex code and requires the most care.

### Migration Rules

1. **Never change logic** — only replace the string/int function, keep behavior identical
2. **One file at a time** — complete a file, build, test, commit
3. **Truncation is visible** — if `sent_snprintf` truncates, log it rather than silently losing data
4. **atoi→sent_parse_int requires error handling** — decide per-callsite what to do on parse failure (default value? error message to player? skip the operation?)

---

## Phase 4: Complexity Reduction

**Priority:** Medium-Low — improves maintainability, not security
**Effort:** Ongoing

### Approach

Follow cURL's model: set a CC threshold, enforce it in CI, reduce over time.

**Initial target:** CC ≤ 200 (pragmatic starting point given current state)
**Medium-term target:** CC ≤ 150
**Long-term target:** CC ≤ 100

### High-Value Decomposition Targets

| Function | CC | Why it matters |
|----------|---:|----------------|
| `script_varseton` | 693 | Largest function, likely contains bugs |
| `do_quest` | 407 | Player-facing quest system |
| `compile_script` | 291 | Script compiler — correctness critical |
| `fread_char` | 236 | Character loading — data integrity |
| `damage_new` | 219 | Combat system — balance sensitive |
| `interpret` | 184 | Command dispatcher — every player hits this |

### Decomposition Strategies

- **Switch-case dispatch tables** — `script_varseton` is likely a massive switch; convert to a function pointer table
- **Extract sub-handlers** — `do_quest` probably handles multiple subcommands; each becomes its own function
- **State machine refactors** — `fread_char` and `compile_script` can use explicit state machines
- **Table-driven parsing** — OLC editors with large switch statements → data tables

### CI Enforcement

Add a build step that runs `pmccabe` and fails if any function exceeds the threshold:

```bash
#!/bin/bash
# scripts/check_complexity.sh
MAX_CC=200
WORST=$(pmccabe src/*.c src/**/*.c 2>/dev/null | sort -rn | head -1 | awk '{print $1}')
if [ "$WORST" -gt "$MAX_CC" ]; then
    echo "FAIL: Function exceeds CC threshold of $MAX_CC"
    pmccabe src/*.c src/**/*.c 2>/dev/null | sort -rn | awk -v max=$MAX_CC '$1 > max'
    exit 1
fi
```

---

## Phase 5: Static Analysis & CI

**Priority:** Low (can start anytime, grows with other phases)
**Effort:** 1-2 days initial setup, then ongoing

### Banned Function Scanner

Add a CI step that greps for banned functions and fails on any new introduction:

```bash
#!/bin/bash
# scripts/check_banned_functions.sh
BANNED="sprintf\(|strcpy\(|strcat\(|gets\(|atoi\(|atol\(|strtok\(|strncpy\("
HITS=$(grep -rn --include='*.c' --include='*.h' \
    --exclude-dir=.deps --exclude-dir=.build \
    -cE "$BANNED" src/ | awk -F: '{sum+=$2} END {print sum}')
echo "Banned function calls: $HITS"
# Initially just report; later enforce no-increase policy
```

### Static Analysis Tools

- **cppcheck** — catches buffer overflows, null derefs, resource leaks
- **clang-tidy** — modernization checks, readability, performance
- **clang --analyze** — path-sensitive analysis (already have Clang in build system)

### Unicode Checks

Per the malicious Unicode blog, add a CI check that all `.c` and `.h` files are ASCII-only:

```bash
# Reject non-ASCII in source files
grep -rPn '[^\x00-\x7F]' --include='*.c' --include='*.h' src/
```

### Memory Monitoring

Per the memory blog, add test-time allocation tracking:
- Track peak concurrent allocations per test case
- Track total allocation count per test case
- Flag tests that exceed thresholds
- Monitor struct sizes for key structures (CHAR_DATA, OBJ_DATA, ROOM_INDEX_DATA)

---

## Phase Summary & Timeline

| Phase | What | Risk Reduction | Effort |
|-------|------|---------------|--------|
| 1. Buffer hardening | Fix BUFFER internals, add len tracking | Foundation | 1-2 days |
| 2. Safe helpers | `sent_strlcpy`, `sent_snprintf`, `sent_parse_int` | Tooling | 2-3 days |
| 3. Systematic migration | Replace all banned functions file-by-file | Bulk safety | 4-6 weeks |
| 4. Complexity reduction | Decompose CC>100 functions | Maintainability | Ongoing |
| 5. CI enforcement | Scanners, static analysis, complexity gates | Prevention | 1-2 days + ongoing |

Phases 1 and 2 are prerequisites. Phase 3 is the bulk work and can be done incrementally alongside normal development. Phases 4 and 5 are ongoing improvements.

---

## References

- [Daniel Stenberg - Writing C for curl](https://daniel.haxx.se/blog/2025/04/07/writing-c-for-curl/)
- [Daniel Stenberg - Detecting malicious Unicode](https://daniel.haxx.se/blog/2025/05/16/detecting-malicious-unicode/)
- [Daniel Stenberg - Decomplexification](https://daniel.haxx.se/blog/2025/05/29/decomplexification/)
- [Daniel Stenberg - Keeping tabs on curl's memory use](https://daniel.haxx.se/blog/2025/07/08/keeping-tabs-on-curls-memory-use/)
- [Daniel Stenberg - More views on curl vulnerabilities](https://daniel.haxx.se/blog/2025/07/10/more-views-on-curl-vulnerabilities/)
- [Daniel Stenberg - Parsing integers in C](https://daniel.haxx.se/blog/2025/11/13/parsing-integers-in-c/)
- [Daniel Stenberg - No strcpy either](https://daniel.haxx.se/blog/2025/12/29/no-strcpy-either/)
