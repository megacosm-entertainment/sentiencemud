# Contributing to Sentience MUD

## Getting Started

### Prerequisites

- C compiler: GCC or Clang
- Build tools: CMake 3.16+, Ninja
- Libraries: Jansson, OpenSSL, libsodium, zlog, hiredis

### Building

All build scripts run from the `src/` directory.

```bash
cd src

# One-step build (recommended)
./build                     # Configure (if needed) and build Debug
./build release             # Optimized release build
./build tests               # Debug build with test framework
./build clean release       # Clean rebuild
./build reconfig gcc        # Force reconfigure with GCC

# Two-step build (if you need more control)
./config clang              # Configure with Clang
./compile                   # Build Debug
./compile release           # Build Release

# Install (symlinks binary to project root)
./install                   # Symlink Debug build
```

Both `Makefile` and `CMakeLists.txt` exist and **must be kept in sync**. When adding or removing `.c` or `.h` files, update both.

### Running

The server loads data from relative paths and must be run from the project root:

```bash
cd /path/to/sentience       # Project root, not src/
./sent                      # Start server
./sent -test                # Run test suite
./sent -test:wnum           # Run tests matching pattern
./sent -g                   # Start with GDB attached
```

### Build Configurations

| Config | Flags | Output | Use |
|--------|-------|--------|-----|
| `debug` (default) | `-g -ggdb`, no optimization | `.build/Debug/sent` | Development |
| `release` | Optimized, no debug symbols | `.build/Release/sent` | Production |
| `reldeb` | Optimized with debug symbols | `.build/RelWithDebInfo/sent` | Profiling |

## Project Layout

```
sentience/
├── src/                            # All source code (version controlled)
│   ├── account/                    # Authentication, account management
│   ├── bootstrap/                  # First-run setup
│   ├── editors/                    # OLC editors (aedit, medit, oedit, redit, etc.)
│   ├── io/json/                    # JSON serialization for all data types
│   ├── nanny/                      # Login/connection state machine
│   ├── tests/                      # Test framework + test data
│   │   ├── data/                   #   JSON test definitions
│   │   ├── framework/              #   Core framework (test_framework.c/h, test_loader.c)
│   │   ├── integration/            #   Integration test handlers
│   │   └── unit/                   #   Unit test handlers
│   ├── docs/                       # All project documentation
│   ├── .build/                     # CMake output (gitignored)
│   ├── .deps/                      # External libraries (gitignored)
│   ├── CMakeLists.txt
│   ├── Makefile
│   └── build, config, compile, install   # Build scripts
│
├── area/                           # Zone files (JSON + legacy .are)
├── data/                           # Runtime data (races, settings, help, etc.)
│   ├── races/                      #   Race definitions (JSON)
│   ├── system/                     #   Game settings, zlog config
│   └── ...
├── accounts/                       # Player accounts (JSON, by letter)
├── characters/                     # Player characters (JSON, by letter)
└── logs/                           # Server logs
```

Only `src/` is version controlled. Everything else (data, logs, accounts, characters, area files) is runtime data and must not be committed.

## Code Conventions

Sentience inherits DikuMUD/ROM conventions established in the 1990s. Follow existing patterns rather than imposing external standards.

### Style

- **Indentation:** 4 spaces (no tabs in new code)
- **Braces:** Opening brace on same line for control structures, next line for function definitions
- **Naming:** `snake_case` for functions and variables, `UPPER_CASE` for constants and macros, `PascalCase` for type names (`CHAR_DATA`, `OBJ_DATA`, `WNUM`)
- **Line length:** No hard limit, but keep lines readable

### Function Documentation

Use Doxygen-style doc blocks for new or modified functions:

```c
/**
 * function_name - Brief one-line description
 *
 * Longer description explaining behavior, side effects, context.
 *
 * @param param1  Description of first parameter
 * @param param2  Description of second parameter
 * @return        Description of return value
 */
```

### Memory Management

Use the project's allocators rather than raw `malloc`/`free`/`strdup`:

| Project Function | Standard Equivalent | Notes |
|-----------------|--------------------|----|
| `alloc_perm(size)` | `calloc(1, size)` | For persistent data |
| `alloc_mem(size)` | `calloc(1, size)` | For general allocation |
| `free_mem(ptr, size)` | `free(ptr)` | Takes size for accounting |
| `str_dup(str)` | `strdup(str)` | Has special read-only pool handling |
| `free_string(str)` | `free(str)` | Skips read-only pool strings |

These are thin wrappers around standard allocators but maintain memory accounting and handle the legacy read-only string pool.

### Adding Commands

Add to `cmd_table[]` in `interp.c`:

```c
{ "name", do_function, POS_*, level, LOG_*, show, false }
```

Implement the handler in the appropriate `act_*.c` file or a new file (updating both build files).

### External Libraries

| Library | Purpose | Location |
|---------|---------|----------|
| Jansson | JSON parsing | `io/json/` files |
| OpenSSL | TLS, hashing | `connection_tls.c`, `handler.c` |
| libsodium | Argon2id password hashing | `account/auth_sodium.c` |
| zlog | Structured logging | `log.c` |
| hiredis | Redis client | `redis_cache.c` |

Libraries live in `src/.deps/`. Use git submodules for version-locked dependencies.

## Key Systems

### Widevnums (Area-Scoped Entity IDs)

Entities are identified by `area_uid:local_vnum` rather than globally unique vnums. This is the active migration -- understanding it is essential for working on the codebase.

**Core types:**
- `WNUM` -- Runtime reference: `{ AREA_DATA *pArea; long vnum; }`
- `WNUM_LOAD` -- Persistence reference: `{ long auid; long vnum; }` (area UID + vnum, resolved at boot)

**Key functions (all in `handler.c` unless noted):**
- `parse_widevnum(ch, arg, &wnum)` -- Parse user input to WNUM (runtime, area must be in global list)
- `parse_widevnum_load(area, arg, &wnum_load)` -- Parse during area deserialization (area not yet in global list)
- `resolve_wnum_load(&wnum_load, &wnum)` -- Convert WNUM_LOAD to WNUM after boot (`db.c`)
- `widevnum_string_room/mobile/object(index, refArea)` -- Format for display
- `wnum_match_room/obj/mob(wnum, entity)` -- Safe runtime comparison

**Rules:**
- Never compare bare `->vnum` fields without also checking the area
- Use `parse_widevnum_load()` during deserialization, never `parse_widevnum()`
- Use `widevnum_string_*()` for display, not raw `%ld` formatting

### Scripting Engine

The scripting engine dispatches triggers through 15 slot-indexed lists for performance:

- `PROG_LIST` entries store `{script_vnum, trigger_type, trigger_phrase}`
- Triggers attach to entities via `LLIST **progs` arrays (one list per slot)
- After boot, `fix_mobprogs()`/`fix_objprogs()`/etc. resolve script vnums to `SCRIPT_DATA *` pointers

See `scripts.c` for the dispatch logic and `editors/` for OLC commands.

### OLC (Online Creation)

In-game editors for all content types. Each editor lives in its own file under `editors/`:

| Editor | File | Content Type |
|--------|------|-------------|
| `aedit` | `editors/areas/aedit.c` | Areas |
| `medit` | `editors/mobiles/medit.c` | Mobiles (NPCs) |
| `oedit` | `editors/objects/oedit.c` | Objects |
| `redit` | `editors/rooms/redit.c` | Rooms |
| `tedit` | `editors/tokens/tedit.c` | Tokens |
| `bpedit` | `editors/blueprints/bpedit.c` | Blueprints |
| `bsedit` | `editors/blueprints/bsedit.c` | Blueprint sections |
| `dngedit` | `editors/dungeons/dngedit.c` | Dungeons |

### JSON Persistence

All serialization code lives in `io/json/`. Key files:

- `json_area.c` -- Area/room/mob/obj/reset serialization
- `json_char.c` -- Player character save/load
- `json_persist.c` -- Persistent mob and runtime state

Pattern: Use Jansson's `json_object_set_new()` for building, `json_get_string()`/`json_get_int()`/`json_get_bool()` helpers for reading.

### Logging

Uses zlog with named categories. Config in `data/system/zlog.conf`.

```c
#include "log.h"
log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "message");
log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "format %s %d", str, num);
```

## Testing

### Running Tests

```bash
cd src && ./build tests
cd /path/to/sentience && ./sent -test
```

Approximately 28 test failures are pre-existing (area/wnum resolution tests that need a full game environment). Your changes should not increase this number.

### Test Architecture

Tests have two parts:

1. **C test handler** in `src/tests/unit/` or `src/tests/integration/` -- implements test logic
2. **JSON test definition** in `src/tests/data/unit/` or `src/tests/data/integration/` -- defines inputs and expected outputs

All test code is guarded by `#ifdef BUILD_TESTS` for zero production overhead.

### Adding a Test

**1. Write or extend a C handler:**

```c
// src/tests/unit/my_tests.c
static test_result_t test_my_feature(test_case_t *test) {
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");

    const char *param = json_get_string(input, "parameter");
    bool expected_result = json_get_bool(expected, "success");

    bool actual = my_function_to_test(param);
    return (actual == expected_result) ? TEST_SUCCESS : TEST_FAILURE;
}
```

Register in `run_test_case()` dispatcher:
```c
if (strcmp(test->test_type, "my_feature_test") == 0)
    result = test_my_feature(test);
```

**2. Create a JSON test definition:**

```json
{
  "test_suite": "my_feature_tests",
  "description": "Tests for my feature",
  "version": "1.0",
  "requires_mud_environment": false,
  "tests": [
    {
      "name": "basic_test",
      "description": "Test basic functionality",
      "test_type": "my_feature_test",
      "input": { "parameter": "test_value" },
      "expected_output": { "success": true }
    }
  ]
}
```

**3. Update both build files** to include the new `.c` file.

### Memory Testing

```bash
# AddressSanitizer (fast, for development)
./build tests -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cd /path/to/sentience && ASAN_OPTIONS=detect_leaks=1 ./sent -test

# Valgrind (thorough)
valgrind --tool=memcheck --leak-check=full ./sent -test
```

See [docs/testing/](docs/testing/) for the full testing framework documentation.

## Branching and Commits

- **`master`** -- Stable branch. All work merges here.
- **Feature branches** -- Use `feature/<topic>` (e.g., `feature/widevnum-migration`).
- **Bug fixes** -- Use `fix/<description>` or reference the issue number.

### Commit Messages

- Write concise messages that explain *why*, not just *what*.
- Reference issue numbers where applicable.
- Keep commits focused -- one logical change per commit.

### Never Commit

- `.build/` directories or compiled binaries
- Log files, core dumps, editor/IDE metadata
- Player data (accounts, characters)
- Credentials or secrets (`.env`, keys)

## Documentation

Internal documentation lives in `src/docs/`. Follow these conventions:

| Type | Pattern | Purpose |
|------|---------|---------|
| Design plans | `PLAN_<TOPIC>.md` | Design notes, open questions, proposed architecture |
| Task tracking | `TODO_<TOPIC>.md` | Checklist of remaining work items |
| Work logs | `WORKLOG_<TOPIC>.md` | Record of work performed, decisions made, commits |
| Completed work | `done/<TOPIC>.md` | Documentation for finished projects (moved from root) |
| Topic docs | `<TOPIC>.md` | Conceptual explanations, analysis, reference material |

**Workflow for non-trivial work:**

1. Check for an existing `PLAN_*.md` -- don't duplicate planning.
2. If none exists, write one before starting implementation.
3. Record progress in `WORKLOG_*.md`.
4. Track remaining items in `TODO_*.md`.
5. Move docs to `done/` when work is complete.

## Code Quality Tools

### clang-format

A `.clang-format` configuration exists in `src/`. Use it to format new or modified code:

```bash
# Format a file in place
clang-format -i myfile.c

# Check formatting without modifying (useful in CI)
clang-format --dry-run --Werror myfile.c

# Format all files you've changed
git diff --name-only -- '*.c' '*.h' | xargs clang-format -i
```

The configuration uses K&R brace style, 4-space indentation, 120-column limit, and right-aligned pointers (`CHAR_DATA *ch`). It intentionally avoids aggressive reformatting -- comments are not reflowed and includes are not reordered.

Only format code you are modifying. Do not reformat entire files you didn't otherwise change.

### clang-tidy

Static analysis for catching bugs, undefined behavior, and style issues:

```bash
# Run on a single file (requires compile_commands.json)
clang-tidy myfile.c

# Generate compile_commands.json from CMake
cd src/.build && cmake --preset debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## License

Sentience is derived from DikuMUD (1990-1991) through Merc (1992-1993) and ROM 2.4 (1993-1998). The project is bound by all three license chains. See [LICENSE.md](LICENSE.md) for the full license texts.

### Key Requirements

These apply to all derivative works, including Sentience:

1. Copyright notices in source file headers must not be removed.
2. The original DikuMUD and Merc credits must be displayed to players (login screen or `credits` command).
3. The code may not be used for commercial purposes (the Diku license prohibits this).
4. Changes should be shared in the spirit of the original licenses: "Much time and thought has gone into this software and you are benefitting. We hope that you share your changes too."

### Contributing Your Code

By submitting code to the Sentience project, you agree that your contributions are given freely and irrevocably under the same license terms that govern the project. Your code becomes part of the collective work and may not be withdrawn.

You retain attribution for your work -- significant contributions should be noted in commit messages and, where appropriate, in file headers or documentation.

## Questions and Issues

Open an issue on the repository for questions, bug reports, or feature requests.
