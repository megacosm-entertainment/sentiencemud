# Testing Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transition from post-hoc testing to TDD by improving the framework, rewriting documentation from scratch, and establishing baseline test coverage across all modules.

**Architecture:** The existing JSON-driven test framework (suites defined in JSON, dispatched to C handlers via `test_dispatcher.c`) is retained and improved. Assertion macros reduce boilerplate. A table-based dispatcher replaces cascading if/else. Five focused docs replace nine outdated ones. New unit and integration tests fill coverage gaps across easy and medium-tier modules.

**Tech Stack:** C (GCC/Clang), Jansson (JSON), custom test framework under `tests/`, CMake + Make build system.

**Spec:** `docs/superpowers/specs/2026-03-23-testing-overhaul-design.md`

---

## Phase 1: Framework Improvements

These changes reduce friction for all subsequent test writing.

---

### Task 1: Add Assertion Macros to Test Framework

**Files:**
- Modify: `tests/framework/test_framework.h`

Assertion macros standardize failure reporting across all test handlers. Each macro logs file, line, expression, and values on failure, then returns `TEST_FAILURE`. They use `do { ... } while (0)` to be safe in all statement contexts.

- [ ] **Step 1: Add assertion macros to test_framework.h**

Add the following block at the end of `test_framework.h`, before the closing `#endif` (if any) or at the end of declarations:

```c
/*
 * Assertion macros for test handlers.
 *
 * Each macro logs a descriptive failure message and returns TEST_FAILURE.
 * Use inside functions that return test_result_t.
 */

#define TEST_ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_TRUE failed: (%s) at %s:%d", #expr, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_FALSE(expr) do { \
    if ((expr)) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_FALSE failed: (%s) was true at %s:%d", #expr, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_INT_EQ(expected, actual) do { \
    long _exp = (long)(expected); \
    long _act = (long)(actual); \
    if (_exp != _act) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_INT_EQ failed: expected %ld, got %ld at %s:%d", \
            _exp, _act, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_INT_NEQ(expected, actual) do { \
    long _exp = (long)(expected); \
    long _act = (long)(actual); \
    if (_exp == _act) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_INT_NEQ failed: both values are %ld at %s:%d", \
            _exp, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_INT_GT(val, threshold) do { \
    long _v = (long)(val); \
    long _t = (long)(threshold); \
    if (_v <= _t) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_INT_GT failed: %ld is not > %ld at %s:%d", \
            _v, _t, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_INT_GTE(val, threshold) do { \
    long _v = (long)(val); \
    long _t = (long)(threshold); \
    if (_v < _t) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_INT_GTE failed: %ld is not >= %ld at %s:%d", \
            _v, _t, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    const char *_exp = (expected); \
    const char *_act = (actual); \
    if ((_exp == NULL && _act != NULL) || (_exp != NULL && _act == NULL) || \
        (_exp != NULL && _act != NULL && strcmp(_exp, _act) != 0)) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_STR_EQ failed: expected \"%s\", got \"%s\" at %s:%d", \
            _exp ? _exp : "(null)", _act ? _act : "(null)", __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_STR_NEQ(expected, actual) do { \
    const char *_exp = (expected); \
    const char *_act = (actual); \
    if ((_exp == NULL && _act == NULL) || \
        (_exp != NULL && _act != NULL && strcmp(_exp, _act) == 0)) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_STR_NEQ failed: both are \"%s\" at %s:%d", \
            _exp ? _exp : "(null)", __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_NULL failed: (%s) was not NULL at %s:%d", #ptr, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, \
            "ASSERT_NOT_NULL failed: (%s) was NULL at %s:%d", #ptr, __FILE__, __LINE__); \
        return TEST_FAILURE; \
    } \
} while (0)
```

- [ ] **Step 2: Build with tests to verify compilation**

Run: `cd /sentience/src && ./build clean tests`
Expected: Build succeeds with no errors. The macros are only included in test-enabled builds via `#ifdef BUILD_TESTS`.

- [ ] **Step 3: Run existing tests to verify no regressions**

Run: `cd /sentience && ./sent -test:profile:unit_only`
Expected: All existing unit tests pass. The macros don't affect existing code — they're additive.

- [ ] **Step 4: Commit**

```bash
git add tests/framework/test_framework.h
git commit -m "Add assertion macros to test framework

Standardized TEST_ASSERT_* macros for consistent failure reporting:
TRUE, FALSE, INT_EQ, INT_NEQ, INT_GT, INT_GTE, STR_EQ, STR_NEQ,
NULL, NOT_NULL. Each logs file:line and values on failure.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Refactor Dispatcher to Table-Based Lookup

**Files:**
- Modify: `tests/framework/test_dispatcher.c`
- Modify: `tests/framework/test_modules.h`

Replace the cascading `if/else if/else if...` chain in `run_test_case()` with a table of `{ pattern, handler, match_mode }` entries. Adding a new test type becomes a one-line table entry instead of editing the dispatch chain.

- [ ] **Step 1: Read the current dispatcher code**

Read `tests/framework/test_dispatcher.c` to understand the full `run_test_case()` function and all includes/helpers. Also read `tests/framework/test_modules.h` for the current handler declarations.

- [ ] **Step 2: Add handler table type to test_modules.h**

Add the following at the top of `test_modules.h`, after existing includes but before handler declarations:

```c
/* Match modes for the handler dispatch table. */
typedef enum {
    MATCH_EXACT,    /* strcmp: test_type must match pattern exactly */
    MATCH_PREFIX    /* strstr: test_type must contain pattern as substring */
} test_match_mode_t;

/* Entry in the handler dispatch table. */
typedef struct {
    const char *pattern;
    test_result_t (*handler)(test_case_t *);
    test_match_mode_t match_mode;
} test_handler_entry_t;
```

- [ ] **Step 3: Replace the dispatch chain in test_dispatcher.c**

In `run_test_case()`, replace the entire block of `if/else if` test_type comparisons with a table lookup. Keep all surrounding code (timing, logging, result printing) intact.

The handler table, placed as a static const at file scope in `test_dispatcher.c`:

```c
/*
 * Handler dispatch table.
 *
 * Order matters: first match wins. Place exact matches before prefix matches
 * to avoid ambiguity. Add new handlers as single-line entries.
 */
static const test_handler_entry_t handler_table[] = {
    /* Unit test handlers */
    { "pure_function_test",              run_pure_function_test_case,       MATCH_EXACT  },
    { "buffer_function_test",            run_buffer_function_test_case,     MATCH_EXACT  },

    /* Integration test handlers — exact matches first */
    { "reset_cross_area_creation",       run_reset_test_case,               MATCH_EXACT  },
    { "reset_serialization",             run_reset_test_case,               MATCH_EXACT  },
    { "reset_legacy_vnum",               run_reset_test_case,               MATCH_EXACT  },
    { "shop_stock_cross_area_creation",  run_shop_stock_test_case,          MATCH_EXACT  },
    { "shop_stock_serialization",        run_shop_stock_test_case,          MATCH_EXACT  },
    { "shop_stock_legacy_vnum",          run_shop_stock_test_case,          MATCH_EXACT  },
    { "shop_stock_reference_integrity",  run_shop_stock_test_case,          MATCH_EXACT  },
    { "damage_class_lookup_test",        run_lookup_table_test_case,        MATCH_EXACT  },
    { "reserved_lookup_test",            run_wnum_test_case,                MATCH_EXACT  },
    { "reserved_wnum_format_test",       run_wnum_test_case,                MATCH_EXACT  },
    { "reserved_compat_test",            run_wnum_test_case,                MATCH_EXACT  },

    /* Integration test handlers — prefix/substring matches */
    { "string_editor_",                  run_string_editor_test_case,       MATCH_PREFIX },
    { "church_",                         run_church_test_case,              MATCH_PREFIX },
    { "instance_",                       run_instance_test_case,            MATCH_PREFIX },
    { "blueprint_",                      run_instance_test_case,            MATCH_PREFIX },
    { "dungeon_",                        run_instance_test_case,            MATCH_PREFIX },
    { "ship_",                           run_instance_test_case,            MATCH_PREFIX },
    { "wnum_json_",                      run_instance_test_case,            MATCH_PREFIX },
    { "persist_directory",               run_instance_test_case,            MATCH_PREFIX },
    { "chat_room_",                      run_chat_room_test_case,           MATCH_PREFIX },
    { "skill_group_",                    run_skill_group_test_case,         MATCH_PREFIX },
    { "skill_",                          run_skill_data_test_case,          MATCH_PREFIX },
    { "spell_fun_",                      run_skill_data_test_case,          MATCH_PREFIX },
    { "class_",                          run_class_data_test_case,          MATCH_PREFIX },
    { "item_type_",                      run_item_type_test_case,           MATCH_PREFIX },
    { "song_",                           run_song_data_test_case,           MATCH_PREFIX },
    { "trait_",                          run_trait_system_test_case,        MATCH_PREFIX },
    { "script_engine_",                  run_script_engine_test_case,       MATCH_PREFIX },
    { "channel_",                        run_channel_pubsub_test_case,      MATCH_PREFIX },
    { "combat_",                         run_combat_telemetry_test_case,    MATCH_PREFIX },
    { "_lookup_test",                    run_lookup_table_test_case,        MATCH_PREFIX },
    { "flag_table_",                     run_lookup_table_test_case,        MATCH_PREFIX },

    /* Sentinel — must be last */
    { NULL, NULL, MATCH_EXACT }
};
```

The dispatch logic inside `run_test_case()` replaces the `if/else if` chain:

```c
    test_result_t result = TEST_SKIP;

    if (test->test_type) {
        bool matched = false;

        for (int i = 0; handler_table[i].pattern != NULL; i++) {
            bool is_match = false;

            if (handler_table[i].match_mode == MATCH_EXACT) {
                is_match = (strcmp(test->test_type, handler_table[i].pattern) == 0);
            } else {
                is_match = (strstr(test->test_type, handler_table[i].pattern) != NULL);
            }

            if (is_match) {
                result = handler_table[i].handler(test);
                matched = true;
                break;
            }
        }

        if (!matched) {
            /* Fallback: unmatched types go to wnum handler (legacy behavior) */
            result = run_wnum_test_case(test);
        }
    } else if (test->execute) {
        result = test->execute(test);
    }
```

- [ ] **Step 4: Build and run all tests**

Run: `cd /sentience/src && ./build clean tests`
Then: `cd /sentience && ./sent -test:profile:full`
Expected: All tests pass. The table produces identical dispatch behavior to the old if/else chain.

- [ ] **Step 5: Commit**

```bash
git add tests/framework/test_dispatcher.c tests/framework/test_modules.h
git commit -m "Refactor test dispatcher to table-based lookup

Replace cascading if/else chain with a static handler_table[]. Adding a
new test type is now a one-line table entry. MATCH_EXACT for strcmp,
MATCH_PREFIX for strstr. Exact matches before prefix matches to avoid
ambiguity. Falls back to wnum handler for unmatched types (legacy).

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Phase 2: Documentation Rewrite

Delete all 9 existing docs in `docs/testing/` and replace with 5 focused documents. Each document is written fresh based on the actual codebase, not the old docs.

---

### Task 3: Remove Old Documentation

**Files:**
- Delete: all 9 files in `docs/testing/`

- [ ] **Step 1: Delete old documentation files**

```bash
cd /sentience/src
rm docs/testing/BRANCH_PROTECTION_CHECKLIST.md
rm docs/testing/CI_BOOTSTRAP_ROOT_WORKFLOW.md
rm docs/testing/MVP_CHANNEL_TEST_REGIMEN.md
rm docs/testing/PLAN_FULL_COVERAGE_2026-02-19.md
rm docs/testing/README.md
rm docs/testing/TESTING_ARCHITECTURE_REFACTOR.md
rm docs/testing/TESTING_FRAMEWORK.md
rm docs/testing/TESTING_QUICK_REFERENCE.md
rm docs/testing/TESTING_ROADMAP.md
```

- [ ] **Step 2: Commit the deletion**

```bash
git add -u docs/testing/
git commit -m "Remove outdated testing documentation

Clearing 9 stale docs to make way for fresh, accurate documentation
aligned with current framework state and TDD workflow.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Write docs/testing/README.md

**Files:**
- Create: `docs/testing/README.md`

This is the entry point. A developer landing here should be able to build, run tests, and know where to go next within 2 minutes.

- [ ] **Step 1: Investigate current test counts**

Run: `cd /sentience/src && ls tests/data/unit/*.json | wc -l && ls tests/data/integration/*.json | wc -l`
Also check: `cd /sentience && ./sent -test:summary` (if binary is built with tests)

Record the exact number of test suites and test cases for accurate documentation.

- [ ] **Step 2: Write README.md**

Create `docs/testing/README.md` covering:

1. **Header** — "Sentience MUD Testing Framework"
2. **Quick Start** — Build with tests (`./build tests`), run all tests (`./sent -test`), run unit only (`./sent -test:profile:unit_only`), run by pattern (`./sent -test:wnum`)
3. **Framework Overview** — JSON-driven test definitions dispatched to C handlers. Zero production overhead (`#ifdef BUILD_TESTS`). ~N test suites, ~M test cases across unit and integration tiers.
4. **Test Profiles** — Table of available profiles (quick, full, unit_only, bootstrap_ci, serialization_cycle, etc.) with descriptions and commands
5. **Documentation Index** — Links to the other 4 docs with one-line descriptions:
   - `TESTING_FRAMEWORK.md` — Complete architecture and reference
   - `WRITING_TESTS.md` — How to write tests (TDD-oriented)
   - `TDD_WORKFLOW.md` — The red-green-refactor process for MUD development
   - `COVERAGE_STATUS.md` — What's tested and what's not
6. **Key Commands** — Table of `./sent -test:*` variations (registry, summary, config, pattern, profile)

Use accurate counts from Step 1. Keep it concise — this is a landing page, not a reference manual.

- [ ] **Step 3: Commit**

```bash
git add docs/testing/README.md
git commit -m "Add new testing README with accurate framework overview

Fresh entry-point doc with build/run commands, profile table,
documentation index, and accurate test counts.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Write docs/testing/TESTING_FRAMEWORK.md

**Files:**
- Create: `docs/testing/TESTING_FRAMEWORK.md`

The comprehensive reference document. A developer should be able to understand the entire framework by reading this one doc.

- [ ] **Step 1: Investigate framework details**

Read these files to gather accurate information:
- `tests/framework/test_framework.h` — structs, enums, function declarations
- `tests/framework/test_framework.c` — init/cleanup, logging, configuration
- `tests/framework/test_dispatcher.c` — handler table (from Task 2 refactor)
- `tests/framework/test_loader.c` — JSON loading logic
- `tests/framework/test_modules.h` — handler declarations, dispatch table types
- `tests/framework/test_utils.h` and `test_utils.c` — fake player utilities
- `tests/data/test_config.json` — profiles and settings
- `test_integration.c` — entry point, path resolution

- [ ] **Step 2: Write TESTING_FRAMEWORK.md**

Create `docs/testing/TESTING_FRAMEWORK.md` with these sections:

1. **Architecture Overview** — Diagram showing: JSON test files → test_loader.c → test_suite_t linked list → test_dispatcher.c handler_table → C handler functions → test_result_t
2. **Test Hierarchy** — Explain test_suite_t, test_case_t, test_config_t, test_profile_t with struct field descriptions
3. **JSON Test Schema** — Complete field reference for test suite JSON files. Include `type`, `test_suite`, `description`, `version`, `requires_mud_environment`, `test_level`, `dependencies`, `timeout_seconds`, `tests[]` array with all test_case fields (`name`, `description`, `test_type`, `input`, `expected_output`, `dependencies`, `timeout_seconds`, `verbose_output`)
4. **Handler Dispatch Table** — Full table showing every `test_type` pattern → handler function mapping. Reference the `handler_table[]` in `test_dispatcher.c`
5. **Configuration** — How `test_config.json` works: profiles, settings (default_timeout_seconds, stop_on_first_failure, run_unit_tests_first, verbose options, log options), environment variables (SENTIENCE_TEST_DATA_DIR, SENTIENCE_TEST_CONFIG, SENTIENCE_TEST_LOG_FILE, SENTIENCE_TEST_LOG_FORMAT)
6. **Assertion Macros** — Reference table of all TEST_ASSERT_* macros from Task 1 with usage examples
7. **Test Utilities** — `test_json_get_string/int/bool`, `test_utils_create_fake_player`, `test_utils_destroy_fake_player`, `test_environment_ready`, `load_json_file`
8. **Logging** — File logging (text and JSON formats), console output, verbosity settings
9. **Build Integration** — CMake (`./build tests`, `BUILD_TESTS=ON`, `ENABLE_COVERAGE=ON`) and Makefile (`make BUILD_TESTS=1`) commands. Conditional compilation via `#ifdef BUILD_TESTS`
10. **Path Resolution** — 4-point fallback for test data dir, config path resolution, bootstrap root support
11. **Multi-Cycle Testing** — `run_test_cycles.sh` usage for persistence/serialization verification across game restarts
12. **Result Codes** — TEST_SUCCESS, TEST_FAILURE, TEST_ERROR, TEST_SKIP with descriptions

- [ ] **Step 3: Commit**

```bash
git add docs/testing/TESTING_FRAMEWORK.md
git commit -m "Add comprehensive testing framework reference

Complete architecture, JSON schema, handler dispatch table, config,
assertions, utilities, logging, build integration, and path resolution.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 6: Write docs/testing/WRITING_TESTS.md

**Files:**
- Create: `docs/testing/WRITING_TESTS.md`

TDD-oriented guide. A developer reads this to learn how to add a new test, starting with the test (not the implementation).

- [ ] **Step 1: Gather example patterns**

Read these for concrete patterns to include as templates:
- `tests/data/unit/core_string_unit_tests.json` — unit test JSON pattern
- `tests/data/integration/combat_telemetry_tests.json` — integration test JSON pattern
- `tests/unit/pure_function_tests.c` — unit handler pattern (function dispatch → test_cases iteration → compare)
- `tests/integration/trait_system_tests.c` — integration handler pattern (type dispatch → MUD state access)
- `tests/integration/lookup_table_tests.c` — another integration pattern
- `tests/framework/test_dispatcher.c` — the handler_table for registration

- [ ] **Step 2: Write WRITING_TESTS.md**

Create `docs/testing/WRITING_TESTS.md` with these sections:

1. **The TDD Cycle for Sentience** — Write test JSON first → build → see it fail (handler missing or assertion fails) → write/modify C handler → build → see it pass → commit
2. **Adding a Test to an Existing Type** — Simplest case: add a test case to an existing JSON suite. Step-by-step with a concrete example (e.g., add a new `is_number` test case to `core_string_unit_tests.json`). No C code changes needed.
3. **Adding a New Test Type** — Full walkthrough:
   - Step A: Create JSON test suite file in `tests/data/unit/` or `tests/data/integration/`
   - Step B: Create C handler file in `tests/unit/` or `tests/integration/`
   - Step C: Add handler declaration to `tests/framework/test_modules.h`
   - Step D: Add entry to `handler_table[]` in `tests/framework/test_dispatcher.c`
   - Step E: Add source file to both `CMakeLists.txt` and `Makefile` (BUILD_TESTS blocks)
   - Step F: Add suite name to appropriate profile(s) in `tests/data/test_config.json`
   - Step G: Build and run
4. **Copy-Paste Templates** — Complete, working templates for:
   - **Unit test JSON** (pure function with test_cases array)
   - **Unit test C handler** (extract function name, iterate test_cases, compare results)
   - **Integration test JSON** (requires_mud_environment: true, fixture references)
   - **Integration test C handler** (type dispatcher, MUD state access, assertion macros)
5. **Using Assertion Macros** — Examples of each TEST_ASSERT_* macro in context
6. **Common Patterns** — Testing a lookup function, testing data integrity, testing a state machine, testing with fake players
7. **Gotchas** — Must include `#ifdef BUILD_TESTS` / `#endif` guards in all test C files. Must update both CMakeLists.txt AND Makefile. Test names must be unique across all suites. JSON must be valid (use `python3 -m json.tool` to validate).

- [ ] **Step 3: Commit**

```bash
git add docs/testing/WRITING_TESTS.md
git commit -m "Add TDD-oriented test writing guide

Step-by-step guide for adding tests to existing types and creating new
test types. Includes copy-paste templates and common patterns.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 7: Write docs/testing/TDD_WORKFLOW.md

**Files:**
- Create: `docs/testing/TDD_WORKFLOW.md`

Process guide for adopting TDD in MUD development. Not a framework reference — a workflow guide.

- [ ] **Step 1: Write TDD_WORKFLOW.md**

Create `docs/testing/TDD_WORKFLOW.md` with these sections:

1. **Why TDD for MUD Development** — MUD systems are highly interconnected; changes in one area can break others. TDD provides regression safety, forces clear API design, and enables confident refactoring. Specific to this codebase: JSON test definitions make it easy to define expected behavior before writing C code.
2. **The Red-Green-Refactor Cycle** — Adapted to this framework:
   - **Red:** Write a JSON test definition with input and expected_output. Write the C handler if needed. Build. Run the test. It fails (red).
   - **Green:** Write the minimal C implementation to make the test pass. Build. Run the test. It passes (green).
   - **Refactor:** Clean up the implementation. Run tests again. Still green.
3. **TDD Patterns for Common MUD Tasks:**
   - **New Command** — Define expected behavior in JSON (command name, arguments, expected effects). Write integration test handler that creates a fake player, calls the command, checks state changes. Implement the command in `act_*.c`.
   - **New Spell** — Define spell parameters in JSON (name, school, damage range, effect). Write test that validates spell function pointer, damage calculation, and affect application. Implement in `magic_*.c`.
   - **New Game System** — Define data model tests (loading, lookup, integrity). Define behavior tests (state transitions, interactions). Implement the system module.
   - **New Data Type** — Define JSON serialization roundtrip test. Test loading, lookup by various keys, integrity constraints. Implement the data type handlers.
4. **When to Write Unit vs Integration Tests** — Unit: pure functions, no global state, no MUD environment needed. Integration: requires loaded areas, game state, or mock zones. Rule of thumb: if the function takes only primitive types or const pointers, it's a unit test candidate.
5. **Working with JSON Test Definitions** — The JSON files ARE the specification. Write them first as a design exercise. The input/expected_output structure forces you to think about the API before implementing it.
6. **Integration with the Build System** — `./build tests` → `./sent -test:your_suite` cycle. Use `./sent -test:profile:quick` for rapid feedback during development.

- [ ] **Step 2: Commit**

```bash
git add docs/testing/TDD_WORKFLOW.md
git commit -m "Add TDD workflow guide for MUD development

Red-green-refactor cycle adapted to JSON-driven framework. Includes
patterns for commands, spells, game systems, and data types.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 8: Write Initial docs/testing/COVERAGE_STATUS.md

**Files:**
- Create: `docs/testing/COVERAGE_STATUS.md`

Living document showing what's tested and what's not. This will be updated as tests are added in later tasks.

- [ ] **Step 1: Audit current test coverage**

Investigate what's currently tested by reading:
- All JSON files in `tests/data/unit/` — list each suite and what functions it tests
- All JSON files in `tests/data/integration/` — list each suite and what systems it tests
- All handler files in `tests/unit/` and `tests/integration/` — what functions are called

Cross-reference with all `.c` files in `src/` to identify untested modules. Group by module category from the spec.

- [ ] **Step 2: Write COVERAGE_STATUS.md**

Create `docs/testing/COVERAGE_STATUS.md` with:

1. **Summary** — Total suites, total test cases, date last updated
2. **Module Coverage Table** — One row per module category:

| Module | Files | Tier | Test Suites | Status |
|--------|-------|------|-------------|--------|
| String Utilities | string.c | Easy | core_string, string_predicates, string_formatting, string_transform, string_edit_helpers | ✅ Good |
| WNUM Parsing | (in handler.c) | Easy | wnum_parsing, widevnum_* | ✅ Good |
| Buffer System | utils/buffer.c | Easy | buffer_core, buffer_permutation | ✅ Good |
| Script Engine | scripts.c, script_*.c | Medium | script_engine_tests | ✅ Good |
| Skill Data | skill_data.c | Medium | skill_data | ✅ Partial |
| Class Data | class_data.c | Medium | class_data | ✅ Partial |
| ... | ... | ... | ... | ... |
| Combat | fight.c, fight2.c | Medium | combat_telemetry | ⚠️ Partial |
| Command Processing | interp.c, act_*.c | Medium | (none) | ❌ None |
| Handler | handler.c | Medium | (none) | ❌ None |
| Quest System | quest.c | Medium | (none) | ❌ None |
| Communication | comm.c, connection_*.c | Hard | (none) | 🔒 Deferred |
| ... | ... | ... | ... | ... |

3. **Tier Legend** — Easy (pure functions, unit testable), Medium (needs MUD environment), Hard (network/I/O/Redis, deferred)
4. **Status Legend** — ✅ Good (>80% of key functions), ⚠️ Partial (some coverage), ❌ None (no tests), 🔒 Deferred (hard tier, future work)

Populate accurately from Step 1 findings.

- [ ] **Step 3: Commit**

```bash
git add docs/testing/COVERAGE_STATUS.md
git commit -m "Add coverage status document

Module-by-module breakdown of test coverage with tier ratings.
Living document to be updated as tests are added.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Phase 3: Easy Tier Tests (Unit — Pure Functions)

Each task adds JSON test definitions and, where needed, a new C test handler. The test JSON is the specification — write it first, then verify the handler supports it.

**Pattern for all easy-tier tasks:**
1. Investigate the source file to identify testable functions
2. Write JSON test suite file in `tests/data/unit/`
3. Extend the pure_function_tests.c handler OR create a new handler
4. Add handler entry to `handler_table[]` if new handler
5. Update CMakeLists.txt and Makefile if new .c file
6. Add suite to test_config.json profiles
7. Build and run

---

### Task 9: SHA256 Unit Tests

**Files:**
- Create: `tests/data/unit/sha256_unit_tests.json`
- Modify: `tests/unit/pure_function_tests.c` (add sha256 handler branch)
- Modify: `tests/data/test_config.json` (add to unit_only profile)

SHA256 is the simplest first test to write. Known input → known output. No MUD state needed.

- [ ] **Step 1: Investigate sha256.c exports**

Read `sha256.h` and `sha256.c` to identify the public API. Expected functions: a hash function that takes input bytes and produces a hex digest string. Record the exact function signature(s).

- [ ] **Step 2: Write the failing test JSON**

Create `tests/data/unit/sha256_unit_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "sha256_unit_tests",
  "description": "Unit tests for SHA256 hash function correctness",
  "version": "1.0",
  "requires_mud_environment": false,
  "test_level": "unit",
  "tests": [
    {
      "name": "sha256_known_vectors",
      "description": "Validate SHA256 against NIST known-answer test vectors",
      "test_type": "pure_function_test",
      "input": {
        "function": "sha256",
        "test_cases": [
          {
            "input_string": "",
            "expected_hash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
          },
          {
            "input_string": "abc",
            "expected_hash": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
          },
          {
            "input_string": "hello world",
            "expected_hash": "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"
          }
        ]
      },
      "expected_output": {
        "all_cases_pass": true
      }
    }
  ]
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:sha256_unit_tests`
Expected: FAIL or ERROR — the pure_function_test handler doesn't know how to dispatch "sha256" yet.

- [ ] **Step 4: Add sha256 handler to pure_function_tests.c**

In `pure_function_tests.c`, inside `run_pure_function_test_case()`, add a new branch for the "sha256" function name. The handler should:

1. Iterate `test_cases` array
2. For each case, extract `input_string` and `expected_hash`
3. Call the SHA256 function from `sha256.h` on the input string
4. Compare the result to the expected hash
5. Return TEST_FAILURE on mismatch with a log message showing input, expected, and actual

Use the existing handler pattern (e.g., the `is_number` handler) as a template. Use `TEST_ASSERT_STR_EQ` for the comparison.

- [ ] **Step 5: Add suite to test_config.json**

In `tests/data/test_config.json`, add `"sha256_unit_tests"` to:
- The `unit_only_suites` array
- The `full_test_suites` array
- The `unit_only` profile's suites array

- [ ] **Step 6: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:sha256_unit_tests`
Expected: All tests PASS.

- [ ] **Step 7: Commit**

```bash
git add tests/data/unit/sha256_unit_tests.json tests/unit/pure_function_tests.c tests/data/test_config.json
git commit -m "Add SHA256 unit tests with known-answer vectors

Tests empty string, 'abc', and 'hello world' against NIST SHA256 digests.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 10: Bit Operations Unit Tests

**Files:**
- Create: `tests/data/unit/bit_operations_unit_tests.json`
- Modify: `tests/unit/pure_function_tests.c` (add bit function handlers)
- Modify: `tests/data/test_config.json`

- [ ] **Step 1: Investigate bit.c exports**

Read `bit.c` to identify pure functions suitable for unit testing. Look for functions that manipulate flag bitfields, convert between flag names and values, format flag strings, etc. Also check if any bit functions are declared in `merc.h` or a dedicated header. Record exact function signatures.

Common patterns to look for: `IS_SET`, `SET_BIT`, `REMOVE_BIT` macros, and any functions like `flag_string()`, `bit_name()`, `fwrite_flag()`, `fread_flag()`, or bitfield utility functions.

- [ ] **Step 2: Write test JSON**

Create `tests/data/unit/bit_operations_unit_tests.json` with test cases covering the bit manipulation functions found in Step 1. Follow the pure_function_test pattern:

```json
{
  "type": "test_suite",
  "test_suite": "bit_operations_unit_tests",
  "description": "Unit tests for bitfield manipulation and flag string functions",
  "version": "1.0",
  "requires_mud_environment": false,
  "test_level": "unit",
  "tests": [
    {
      "name": "bit_function_name_cases",
      "description": "Validate [function] for [behavior]",
      "test_type": "pure_function_test",
      "input": {
        "function": "[function_name_from_step_1]",
        "test_cases": []
      },
      "expected_output": { "all_cases_pass": true }
    }
  ]
}
```

Populate `test_cases` based on the functions discovered in Step 1. For each function, test: normal cases, edge cases (zero, max values), and boundary conditions.

- [ ] **Step 3: Add handler branches to pure_function_tests.c**

For each bit function tested, add a handler branch in `run_pure_function_test_case()`. Follow the existing pattern: extract inputs from JSON, call the function, compare to expected output, return TEST_FAILURE on mismatch.

- [ ] **Step 4: Update test_config.json**

Add `"bit_operations_unit_tests"` to the `unit_only_suites`, `full_test_suites`, and `unit_only` profile.

- [ ] **Step 5: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:bit_operations_unit_tests`
Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/data/unit/bit_operations_unit_tests.json tests/unit/pure_function_tests.c tests/data/test_config.json
git commit -m "Add bit operations unit tests

Tests for bitfield manipulation and flag string conversion functions.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 11: Lookup Function Unit Tests

**Files:**
- Create: `tests/data/unit/lookup_function_unit_tests.json`
- Modify: `tests/unit/pure_function_tests.c`
- Modify: `tests/data/test_config.json`

- [ ] **Step 1: Investigate lookup.c exports**

Read `lookup.c` and any related headers to identify pure lookup functions. These may include functions like `position_lookup()`, `sex_lookup()`, `size_lookup()`, `race_lookup()`, `liq_lookup()`, or other table-based lookups that take a string name and return an integer index.

Note: some lookup functions may already be tested in `lookup_table_tests.c` (integration). Only add unit tests for functions that can run WITHOUT the MUD environment (i.e., that use static const tables, not runtime-loaded data).

- [ ] **Step 2: Write test JSON**

Create `tests/data/unit/lookup_function_unit_tests.json` targeting pure lookup functions. Test valid lookups (known names → expected indices), invalid lookups (unknown names → -1 or similar), and edge cases (empty string, NULL-safe behavior if applicable).

- [ ] **Step 3: Add handler branches to pure_function_tests.c**

- [ ] **Step 4: Update test_config.json**

- [ ] **Step 5: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:lookup_function_unit_tests`
Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/data/unit/lookup_function_unit_tests.json tests/unit/pure_function_tests.c tests/data/test_config.json
git commit -m "Add lookup function unit tests

Tests for static table lookups (position, sex, size, etc.).

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 12: Memory Utility Tests

**Files:**
- Create: `tests/data/unit/memory_util_unit_tests.json`
- Create: `tests/unit/memory_util_tests.c`
- Modify: `tests/framework/test_modules.h` (add declaration)
- Modify: `tests/framework/test_dispatcher.c` (add table entry)
- Modify: `CMakeLists.txt` (add source file)
- Modify: `Makefile` (add source file)
- Modify: `tests/data/test_config.json`

Memory utilities warrant their own handler since they test allocation/free patterns that differ from string functions.

- [ ] **Step 1: Investigate mem.c exports**

Read `mem.c` and `recycle.h`/`merc.h` for memory-related function declarations. Look for: `alloc_mem()`, `free_mem()`, `alloc_perm()`, `new_buf()`, `free_buf()`, `buf_string()`, and any memory pool functions. Identify which functions can be safely tested in isolation (allocate, use, free without needing game state).

- [ ] **Step 2: Write test JSON**

Create `tests/data/unit/memory_util_unit_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "memory_util_unit_tests",
  "description": "Unit tests for memory allocation and management utilities",
  "version": "1.0",
  "requires_mud_environment": false,
  "test_level": "unit",
  "tests": [
    {
      "name": "alloc_free_basic",
      "description": "Validate basic alloc_mem/free_mem cycle",
      "test_type": "memory_util_test",
      "input": {
        "scenario": "alloc_free_basic",
        "alloc_size": 256
      },
      "expected_output": { "success": true }
    },
    {
      "name": "alloc_zero_size",
      "description": "Validate alloc_mem with zero size",
      "test_type": "memory_util_test",
      "input": {
        "scenario": "alloc_zero_size"
      },
      "expected_output": { "success": true }
    }
  ]
}
```

Add more test cases based on the functions discovered in Step 1.

- [ ] **Step 3: Create the C handler file**

Create `tests/unit/memory_util_tests.c`:

```c
#ifdef BUILD_TESTS

#include <string.h>
#include "../../merc.h"
#include "../framework/test_framework.h"

test_result_t run_memory_util_test_case(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory util test missing configuration");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        return TEST_ERROR;
    }

    /* Dispatch by scenario — add cases based on Step 1 findings */
    if (strcmp(scenario, "alloc_free_basic") == 0) {
        int alloc_size = test_json_get_int(input, "alloc_size");
        if (alloc_size <= 0) alloc_size = 256;

        void *ptr = alloc_mem(alloc_size);
        TEST_ASSERT_NOT_NULL(ptr);

        memset(ptr, 0xAB, alloc_size);
        free_mem(ptr, alloc_size);

        return TEST_SUCCESS;
    }

    /* Add more scenarios from Step 1 investigation */

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown memory util scenario: %s", scenario);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */
```

- [ ] **Step 4: Register the handler**

Add to `tests/framework/test_modules.h`:
```c
test_result_t run_memory_util_test_case(test_case_t *test);
```

Add to `handler_table[]` in `tests/framework/test_dispatcher.c`:
```c
    { "memory_util_test",                run_memory_util_test_case,         MATCH_EXACT  },
```

- [ ] **Step 5: Update build files**

Add `tests/unit/memory_util_tests.c` to both the CMakeLists.txt and Makefile `BUILD_TESTS` conditional blocks.

- [ ] **Step 6: Update test_config.json**

Add `"memory_util_unit_tests"` to `unit_only_suites`, `full_test_suites`, and `unit_only` profile.

- [ ] **Step 7: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:memory_util_unit_tests`
Expected: All tests PASS.

- [ ] **Step 8: Commit**

```bash
git add tests/data/unit/memory_util_unit_tests.json tests/unit/memory_util_tests.c \
       tests/framework/test_modules.h tests/framework/test_dispatcher.c \
       CMakeLists.txt Makefile tests/data/test_config.json
git commit -m "Add memory utility unit tests

Tests alloc_mem/free_mem cycles and edge cases.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 13: Constants and Tables Data Integrity Tests

**Files:**
- Create: `tests/data/integration/constants_tables_tests.json`
- Create: `tests/integration/constants_tables_tests.c`
- Modify: `tests/framework/test_modules.h`
- Modify: `tests/framework/test_dispatcher.c`
- Modify: `CMakeLists.txt`, `Makefile`, `tests/data/test_config.json`

These are integration tests because the tables are populated at runtime during MUD boot.

- [ ] **Step 1: Investigate const.c and tables.c**

Read `const.c` and `tables.c` to identify:
- Static const arrays (position_table, sex_table, size_table, etc.)
- Function tables (skill_table, spell_table, group_table)
- Any validation that should hold (e.g., tables are non-empty, names are unique, function pointers are non-NULL)

Also read `tables.h` for type definitions and any existing lookup functions.

- [ ] **Step 2: Write test JSON**

Create `tests/data/integration/constants_tables_tests.json` testing:
- Table non-emptiness (each table has at least 1 entry)
- Table termination (last entry sentinel is correct)
- Name uniqueness within each table
- Known entries exist (e.g., "standing" in position_table, "male" in sex_table)
- Function pointer non-NULL for entries that require them

Use `test_type` prefix `"const_table_"` for these tests.

- [ ] **Step 3: Create C handler**

Create `tests/integration/constants_tables_tests.c` with a handler that dispatches by `test->test_type`:
- `const_table_nonempty_test` — Verify specified table has > 0 entries
- `const_table_entry_exists_test` — Verify a named entry exists in specified table
- `const_table_unique_names_test` — Verify no duplicate names in specified table
- `const_table_sentinel_test` — Verify table ends with NULL sentinel

Use assertion macros throughout.

- [ ] **Step 4: Register handler, update build files**

Add declaration to `test_modules.h`. Add `"const_table_"` prefix entry to `handler_table[]`. Add source to CMakeLists.txt and Makefile. Add suite to test_config.json.

- [ ] **Step 5: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:constants_tables_tests`
Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/data/integration/constants_tables_tests.json tests/integration/constants_tables_tests.c \
       tests/framework/test_modules.h tests/framework/test_dispatcher.c \
       CMakeLists.txt Makefile tests/data/test_config.json
git commit -m "Add constants and tables data integrity tests

Validates table non-emptiness, entry existence, name uniqueness, and
sentinel termination for game constant tables.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 14: Expand String Utility Coverage

**Files:**
- Modify: `tests/data/unit/core_string_unit_tests.json` (or create new suite files)
- Modify: `tests/unit/pure_function_tests.c`

- [ ] **Step 1: Audit string.c coverage gaps**

Read `string.c` and identify exported functions. Cross-reference with existing test suites (`core_string_unit_tests`, `string_predicates_unit_tests`, `string_formatting_unit_tests`, `string_transform_unit_tests`, `string_edit_helpers_unit_tests`). List functions that have NO test coverage.

- [ ] **Step 2: Write tests for uncovered functions**

For each uncovered function, add test cases to the most appropriate existing suite or create a new suite if the function doesn't fit existing categories. Follow the pure_function_test pattern with `input.function` and `input.test_cases`.

- [ ] **Step 3: Add handler branches if needed**

If any newly tested functions aren't already dispatched in `pure_function_tests.c`, add handler branches.

- [ ] **Step 4: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:profile:unit_only`
Expected: All tests PASS, including new test cases.

- [ ] **Step 5: Commit**

```bash
git add tests/data/unit/ tests/unit/pure_function_tests.c
git commit -m "Expand string utility test coverage

Add tests for previously uncovered string functions.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 15: Expand Buffer and Color Code Coverage

**Files:**
- Modify: existing buffer and color test JSON suites
- Modify: `tests/unit/buffer_function_tests.c` and/or `tests/unit/pure_function_tests.c`

- [ ] **Step 1: Audit buffer and color coverage gaps**

For buffers: read `utils/buffer.c` and `utils/buffer.h`, cross-reference with `buffer_core_unit_tests.json` and `buffer_permutation_unit_tests.json`. Identify untested functions or scenarios.

For colors: read color-related functions in `string.c` or wherever color utilities live. Cross-reference with `color_code_helpers_unit_tests.json`, `color_article_helpers_unit_tests.json`, `color_trunc_pad_helpers_unit_tests.json`.

- [ ] **Step 2: Add test cases for gaps**

Add new test cases to existing JSON suites for uncovered functions and edge cases.

- [ ] **Step 3: Add handler branches if needed**

- [ ] **Step 4: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:profile:unit_only`
Expected: All tests PASS.

- [ ] **Step 5: Commit**

```bash
git add tests/data/unit/ tests/unit/
git commit -m "Expand buffer and color code test coverage

Add tests for previously uncovered buffer operations and color functions.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 16: Requirements System Unit Tests

**Files:**
- Create: `tests/data/unit/requirements_unit_tests.json` or `tests/data/integration/requirements_tests.json`
- Modify: `tests/unit/pure_function_tests.c` (if pure functions found) or create `tests/integration/requirements_tests.c`
- Modify: build files and test_config.json as needed

- [ ] **Step 1: Investigate requirements.c and requirements.h**

Read both files. Identify functions: likely `check_requirement()`, `meets_requirements()`, requirement parsing functions. Determine if any are pure functions (unit testable) or all require MUD state (integration).

- [ ] **Step 2: Write test JSON**

Create test suite file targeting the functions found. If pure functions exist, use unit test pattern. If MUD state is needed, use integration pattern with `requires_mud_environment: true`.

- [ ] **Step 3: Implement handler**

Either extend `pure_function_tests.c` or create a new integration handler following the standard pattern.

- [ ] **Step 4: Register, build, run**

Register in handler table if new handler. Update build files if new .c file. Update test_config.json.

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:requirements`
Expected: All tests PASS.

- [ ] **Step 5: Commit**

```bash
git add tests/ CMakeLists.txt Makefile
git commit -m "Add requirements system tests

Tests for requirement checking and validation functions.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Phase 4: Medium Tier Tests (Integration — MUD Environment)

These tests require the full MUD environment (areas loaded, game state initialized). Each creates a new integration test handler.

**Pattern for all medium-tier tasks:**
1. Investigate the source module for testable functions
2. Write JSON test suite in `tests/data/integration/`
3. Create C handler in `tests/integration/`
4. Register in handler table
5. Update CMakeLists.txt, Makefile, test_config.json
6. Build and run

---

### Task 17: Command Table Integrity Tests

**Files:**
- Create: `tests/data/integration/command_table_tests.json`
- Create: `tests/integration/command_table_tests.c`
- Modify: `tests/framework/test_modules.h`, `tests/framework/test_dispatcher.c`
- Modify: `CMakeLists.txt`, `Makefile`, `tests/data/test_config.json`

- [ ] **Step 1: Investigate interp.c and interp.h**

Read `interp.c` and `interp.h` for the command table structure. Identify: `cmd_table[]` array, `CMD_DATA` struct, `do_function` pointer type, command lookup logic, and any validation functions. Determine what properties a valid command entry must have (non-NULL name, non-NULL function pointer, valid position, valid level).

- [ ] **Step 2: Write test JSON**

Create `tests/data/integration/command_table_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "command_table_tests",
  "description": "Command table integrity and lookup validation",
  "version": "1.0",
  "requires_mud_environment": true,
  "test_level": "integration",
  "tests": [
    {
      "name": "command_table_nonempty",
      "description": "Verify command table is populated after boot",
      "test_type": "command_table_count_test",
      "input": { "minimum_expected": 50 },
      "expected_output": { "count_positive": true }
    },
    {
      "name": "command_table_integrity",
      "description": "All command entries have valid name, function pointer, and position",
      "test_type": "command_table_integrity_test",
      "input": {},
      "expected_output": { "all_valid": true }
    },
    {
      "name": "command_lookup_known",
      "description": "Known commands resolve correctly via interpreter lookup",
      "test_type": "command_table_lookup_test",
      "input": {
        "test_cases": [
          { "name": "look", "should_exist": true },
          { "name": "get", "should_exist": true },
          { "name": "say", "should_exist": true },
          { "name": "kill", "should_exist": true },
          { "name": "score", "should_exist": true },
          { "name": "xyzzynonexistent", "should_exist": false }
        ]
      },
      "expected_output": { "all_lookups_correct": true }
    },
    {
      "name": "command_table_unique_names",
      "description": "No duplicate command names in the table",
      "test_type": "command_table_unique_test",
      "input": {},
      "expected_output": { "all_unique": true }
    }
  ]
}
```

- [ ] **Step 3: Create C handler**

Create `tests/integration/command_table_tests.c` with:

```c
#ifdef BUILD_TESTS

#include <string.h>
#include "../../merc.h"
#include "../../interp.h"
#include "../framework/test_framework.h"

static test_result_t test_command_table_count(test_case_t *test);
static test_result_t test_command_table_integrity(test_case_t *test);
static test_result_t test_command_table_lookup(test_case_t *test);
static test_result_t test_command_table_unique(test_case_t *test);

test_result_t run_command_table_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    if (strcmp(test->test_type, "command_table_count_test") == 0)
        return test_command_table_count(test);
    if (strcmp(test->test_type, "command_table_integrity_test") == 0)
        return test_command_table_integrity(test);
    if (strcmp(test->test_type, "command_table_lookup_test") == 0)
        return test_command_table_lookup(test);
    if (strcmp(test->test_type, "command_table_unique_test") == 0)
        return test_command_table_unique(test);

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown command table test type: %s", test->test_type);
    return TEST_ERROR;
}

/* Implement each test function following the established pattern:
   - Extract input from test->config
   - Access game data (command table)
   - Validate using TEST_ASSERT_* macros
   - Return TEST_SUCCESS / TEST_FAILURE */

#endif
```

Implement each static function using the investigation from Step 1 to access the command table correctly. Use `TEST_ASSERT_NOT_NULL`, `TEST_ASSERT_INT_GT`, `TEST_ASSERT_STR_EQ` macros.

- [ ] **Step 4: Register handler**

Add `run_command_table_test_case` to `test_modules.h` and add `{ "command_table_", run_command_table_test_case, MATCH_PREFIX }` to `handler_table[]`.

- [ ] **Step 5: Update build files and config**

Add `tests/integration/command_table_tests.c` to CMakeLists.txt and Makefile. Add `"command_table_tests"` to `full_test_suites` and integration profiles in test_config.json.

- [ ] **Step 6: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:command_table_tests`
Expected: All tests PASS.

- [ ] **Step 7: Commit**

```bash
git add tests/data/integration/command_table_tests.json tests/integration/command_table_tests.c \
       tests/framework/test_modules.h tests/framework/test_dispatcher.c \
       CMakeLists.txt Makefile tests/data/test_config.json
git commit -m "Add command table integrity tests

Validates command table population, entry integrity, known command
lookups, and name uniqueness.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 18: Handler Function Tests

**Files:**
- Create: `tests/data/integration/handler_function_tests.json`
- Create: `tests/integration/handler_function_tests.c`
- Modify: `tests/framework/test_modules.h`, `tests/framework/test_dispatcher.c`
- Modify: `CMakeLists.txt`, `Makefile`, `tests/data/test_config.json`

- [ ] **Step 1: Investigate handler.c**

Read `handler.c` (14,447 lines). Focus on functions that can be tested with a fake player and the existing game state: `get_char_room()`, `get_obj_carry()`, `get_obj_wear()`, `affect_to_char()`, `affect_remove()`, `affect_strip()`, `char_from_room()`, `char_to_room()`, `obj_from_char()`, `obj_to_char()`, `count_users()`, and any pure utility functions.

Identify which functions are safe to call in a test context (won't crash without full game loop) and which require specific setup.

- [ ] **Step 2: Write test JSON**

Create `tests/data/integration/handler_function_tests.json` with test_types like:
- `handler_get_char_test` — Test character lookup functions
- `handler_affect_test` — Test affect add/remove/strip
- `handler_room_transfer_test` — Test char_from_room/char_to_room

Use the bootstrap test area and fake player.

- [ ] **Step 3: Create C handler**

Create `tests/integration/handler_function_tests.c`. Use `test_utils_create_fake_player()` where needed. Place the fake player in a test room, test handler functions, clean up.

- [ ] **Step 4: Register, update build files, run, commit**

Follow the standard pattern: add to handler table, build files, test_config.json. Build, run, verify, commit.

---

### Task 19: Combat Math Tests

**Files:**
- Create: `tests/data/integration/combat_math_tests.json`
- Create: `tests/integration/combat_math_tests.c`
- Modify: `tests/framework/test_modules.h`, `tests/framework/test_dispatcher.c`
- Modify: `CMakeLists.txt`, `Makefile`, `tests/data/test_config.json`

- [ ] **Step 1: Investigate fight.c and fight2.c**

Read both files. Identify testable functions:
- Damage calculation functions (raw damage → modified damage)
- Hit/miss probability calculations
- Damage verb selection (`dam_message()` or similar)
- AC/resistance calculations
- Any pure math functions that don't depend on complex game state

Note: `combat_telemetry_tests.c` already tests NPC/PC damage branch reduction. Focus on functions NOT already covered by combat_telemetry.

- [ ] **Step 2: Write test JSON**

Create `tests/data/integration/combat_math_tests.json` with test_types like:
- `combat_math_damage_verb_test` — Verify correct damage verb for damage ranges
- `combat_math_hit_calc_test` — Verify hit chance calculations
- `combat_math_ac_test` — Verify armor class calculations

Use parametric test cases with known inputs and expected outputs.

- [ ] **Step 3: Create C handler**

Create `tests/integration/combat_math_tests.c`. For damage verb tests, call the function with various damage values and verify the returned string. For calculations, verify math against expected formulas.

- [ ] **Step 4: Register, update build files, run, commit**

Follow the standard pattern.

---

### Task 20: Quest System Tests

**Files:**
- Create: `tests/data/integration/quest_system_tests.json`
- Create: `tests/integration/quest_system_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate quest.c**

Read `quest.c` (8,030 lines). Identify testable aspects:
- Quest data loading and count validation
- Quest lookup by name/ID
- Quest state machine transitions (if accessible)
- Quest requirement checking
- Quest reward calculation

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

Follow the standard integration test pattern established in Tasks 17-19.

---

### Task 21: Reputation System Tests

**Files:**
- Create: `tests/data/integration/reputation_system_tests.json`
- Create: `tests/integration/reputation_system_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate reputation.c**

Read `reputation.c`. Test: reputation data loading, reputation lookup, reputation level calculations, reputation name-to-value conversions.

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

---

### Task 22: OLC Framework Tests

**Files:**
- Create: `tests/data/integration/olc_framework_tests.json`
- Create: `tests/integration/olc_framework_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate olc.c, olc.h, olc_act.c**

Read OLC files. Test: editor state management (current editor, edit mode transitions), OLC command table integrity, editor lookup functions.

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

---

### Task 23: Wilderness System Tests

**Files:**
- Create: `tests/data/integration/wilderness_system_tests.json`
- Create: `tests/integration/wilderness_system_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate wilds.c and wilderness_*.c**

Read wilderness files. Test: coordinate handling, terrain resolution, wilderness map data integrity, virtual link resolution. Note existing `wilderness_resolver_unit_tests` coverage to avoid duplication.

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

---

### Task 24: Blueprint and Dungeon Expansion

**Files:**
- Modify: existing `tests/data/integration/instance_persist_tests.json` or `blueprint_area_scope_tests.json`
- Modify: `tests/integration/instance_tests.c` (if adding test types)

- [ ] **Step 1: Audit existing coverage**

Read existing blueprint/instance/dungeon test suites and handler. Identify gaps: blueprint creation/validation not covered, dungeon floor generation, room template application.

- [ ] **Step 2: Add test cases for gaps**

Expand existing JSON suites with new test cases. Add new handler branches in `instance_tests.c` if new test types are needed.

- [ ] **Step 3: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:instance_persist && ./sent -test:blueprint_area_scope`
Expected: All tests PASS.

- [ ] **Step 4: Commit**

---

### Task 25: Update Cycle Tests

**Files:**
- Create: `tests/data/integration/update_cycle_tests.json`
- Create: `tests/integration/update_cycle_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate update.c**

Read `update.c` (4,772 lines). Identify testable tick functions: weather updates, character healing/regeneration calculations, object decay timers, area reset timing. Focus on calculation functions that can be tested without running the full game loop.

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

---

### Task 26: Spell Data Integrity Tests

**Files:**
- Create: `tests/data/integration/spell_data_integrity_tests.json`
- Create: `tests/integration/spell_data_tests.c`
- Modify: standard framework files, build files, config

- [ ] **Step 1: Investigate magic.c and magic.h**

Read magic system files. Test: spell table population (non-empty), spell lookup by name, spell function pointer validity (non-NULL for spells that should have them), spell school categorization, spell level ranges.

- [ ] **Step 2: Write test JSON, create handler, register, build, run, commit**

---

### Task 27: Expand Class/Skill/Trait Coverage

**Files:**
- Modify: existing `tests/data/integration/skill_data_tests.json`, `class_data_tests.json`, `trait_system_tests.json`
- Modify: `tests/integration/skill_data_tests.c`, `class_data_tests.c`, `trait_system_tests.c`

- [ ] **Step 1: Audit existing coverage for each**

Read each existing test suite and handler. Identify untested aspects:
- Skills: group membership validation, prerequisite chains, level requirements
- Classes: attribute modifier validation, class-specific ability checks
- Traits: trait interaction rules, trait effect calculations

- [ ] **Step 2: Add test cases for gaps**

Expand JSON suites and add handler branches as needed.

- [ ] **Step 3: Build and run**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:skill_data && ./sent -test:class_data && ./sent -test:trait_system`
Expected: All tests PASS.

- [ ] **Step 4: Commit**

---

## Phase 5: Finalize

---

### Task 28: Update Coverage Status and Full Test Suite Verification

**Files:**
- Modify: `docs/testing/COVERAGE_STATUS.md`
- Modify: `docs/testing/README.md` (update test counts)

- [ ] **Step 1: Run full test suite**

Run: `cd /sentience/src && ./build clean tests && cd /sentience && ./sent -test:profile:full`
Record: total tests, passed, failed, errors, skipped.

- [ ] **Step 2: Update COVERAGE_STATUS.md**

Update the module coverage table with accurate status for each module. Update suite counts and test counts. Mark newly tested modules as ✅ or ⚠️ based on coverage depth.

- [ ] **Step 3: Update README.md**

Update the framework overview section with accurate test suite and test case counts.

- [ ] **Step 4: Final commit**

```bash
git add docs/testing/COVERAGE_STATUS.md docs/testing/README.md
git commit -m "Update coverage status with final test counts

Reflects all new test suites added during testing overhaul.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```
