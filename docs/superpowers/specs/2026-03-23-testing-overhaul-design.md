# Testing Overhaul: Documentation Rewrite, Audit & Baseline Coverage

**Date:** 2026-03-23
**Status:** Approved
**Goal:** Transition from post-hoc testing to test-driven development by rewriting testing documentation from scratch, performing a full codebase audit, and establishing baseline test coverage across all modules.

---

## Context

Sentience MUD has a capable custom testing framework (~400+ test cases across 40-50 suites) built on a JSON-driven architecture with a C dispatcher. However:

- **Documentation is stale** — 9 docs in `docs/testing/`, several outdated (README claims 6 tests), others are transitional plans from earlier phases.
- **Coverage is uneven** — strong in some areas (string utils, WNUM parsing, scripting), absent in many others (combat math, command handlers, OLC, wilderness).
- **No TDD workflow** — tests are written after code, and the framework doesn't guide developers toward test-first practices.

The existing framework is solid and will be retained. The JSON-driven test definitions, dispatcher pattern, profile-based execution, and multi-cycle persistence testing all remain.

---

## Approach: Audit-First Pipeline

1. Full codebase audit → 2. Fresh documentation → 3. Framework improvements → 4. Write tests

The audit is the foundation that informs everything else.

---

## Phase 1: Codebase Audit

Walk every `.c` file in the codebase, grouped by module category. For each module, catalog:

| Field | Description |
|-------|-------------|
| **Currently tested** | Which functions/behaviors have existing test coverage |
| **Untested** | Specific functions and behaviors lacking tests |
| **Testability tier** | Easy (pure functions), Medium (needs MUD env/fixtures), Hard (network/I/O/Redis) |
| **Recommended test type** | Unit (JSON-driven pure function) vs Integration (needs MUD environment) |
| **Priority** | Based on code criticality and change frequency |

The audit is a working artifact (session state), not committed to the repo. It drives both documentation and test writing.

### Module Categories to Audit

1. **Utilities** — `string.c`, `bit.c`, `lookup.c`, `mem.c`, `const.c`, `tables.c`, `sha256.c`
2. **Buffer system** — `utils/buffer.c`
3. **Data systems** — `skill_data.c`, `class_data.c`, `song_data.c`, `skill_group.c`, `traits.c`, `item_types.c`, `requirements.c`
4. **Command processing** — `interp.c`, `act_comm.c`, `act_info.c`, `act_info2.c`, `act_obj.c`, `act_obj2.c`, `act_move.c`, `act_enter.c`, `act_class.c`, `act_wiz.c`
5. **Combat** — `fight.c`, `fight2.c`, `shoot.c`
6. **Magic** — `magic.c`, `magic2.c`, `magic_*.c` (24 school files)
7. **Scripting** — `scripts.c`, `script_comp.c`, `script_expand.c`, `script_ifc.c`, `script_commands.c`, `script_cmds.c`, `script_const.c`, `script_vars.c`, `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`
8. **Database/storage** — `db.c`, `db2.c`, `save.c`, `storage.c`
9. **OLC** — `olc.c`, `olc_act.c`, `olc_act2.c`, `olc_save.c`, `editor.c`, `editors/*`
10. **Game systems** — `quest.c`, `reputation.c`, `church.c`, `house.c`, `auction.c`, `mail.c`, `note.c`, `gq.c`, `trade.c`, `project.c`
11. **Wilderness** — `wilds.c`, `wilds_wildgen.c`, `wilderness_*.c`
12. **Instances** — `blueprint.c`, `dungeon.c`, `boat.c`
13. **Character/NPC** — `handler.c`, `special.c`, `mount.c`, `hunt.c`
14. **Communication** — `comm.c`, `connection.c`, `connection_tcp.c`, `connection_tls.c`, `connection_websocket.c`
15. **Protocols** — `protocol.c`, `protocol_layer.c`, `protocol_telnet.c`, `protocol_websocket.c`, `mccp.c`
16. **Account/Auth** — `account/*`, `nanny.c`, `nanny/*`
17. **Channels** — `channels/*`
18. **I/O/JSON** — `io/*`
19. **Bootstrap** — `bootstrap/*`
20. **Miscellaneous** — `weather.c`, `update.c`, `effects.c`, `events.c`, `drunk.c`, `music.c`, `treasuremap.c`, `secret.c`, `sectors_runtime.c`, `autowar.c`, `invasion.c`, `ban.c`, `staff.c`, `stats.c`, `scan.c`, `help.c`, `chat_rooms.c`, `mxp_links.c`, `pfile_migrate.c`, `log.c`

---

## Phase 2: Framework Improvements

Small, targeted changes to reduce TDD friction:

### Assertion Macros

Add to `tests/framework/test_framework.h`:

```c
#define TEST_ASSERT_TRUE(expr) ...
#define TEST_ASSERT_FALSE(expr) ...
#define TEST_ASSERT_EQ(expected, actual) ...
#define TEST_ASSERT_NEQ(expected, actual) ...
#define TEST_ASSERT_STR_EQ(expected, actual) ...
#define TEST_ASSERT_STR_NEQ(expected, actual) ...
#define TEST_ASSERT_NULL(ptr) ...
#define TEST_ASSERT_NOT_NULL(ptr) ...
#define TEST_ASSERT_INT_EQ(expected, actual) ...
#define TEST_ASSERT_INT_GT(val, threshold) ...
```

Each macro produces a consistent failure message including file, line, and the failed expression/values. Returns `TEST_FAILURE` on assertion failure.

### Simplified Test Registration

Add a registration mechanism so new test handlers can self-register without editing the central dispatcher:

```c
// In handler file:
REGISTER_TEST_HANDLER("my_new_type_*", run_my_new_type_test_case);

// Framework auto-collects these at init time
```

This reduces the steps to add a new test type from 4 (write handler, edit dispatcher, add JSON, update build) to 3 (write handler, add JSON, update build).

---

## Phase 3: Documentation Rewrite

Delete all 9 existing files in `docs/testing/`. Replace with 5 focused documents:

### 1. `README.md` — Entry Point
- What the testing framework is
- How to build with tests (`./build tests`)
- How to run tests (`./sent -test`, profiles, patterns)
- Current coverage status (accurate numbers)
- Links to other docs
- Quick-start: write your first test in 5 minutes

### 2. `TESTING_FRAMEWORK.md` — Comprehensive Reference
- Architecture: JSON-driven definitions → C dispatcher → test handlers
- Test hierarchy: suites, cases, types, dependencies
- JSON schema with full field reference
- Dispatcher routing table (test_type → handler mapping)
- Configuration: profiles, settings, timeouts
- Test utilities: assertions, fake players, JSON helpers
- Logging and output formats
- Build integration (CMake and Makefile)
- Multi-cycle and persistence testing

### 3. `WRITING_TESTS.md` — TDD Developer Guide
- The TDD cycle adapted to this framework
- Step-by-step: JSON test case → C handler (if new type) → run → implement
- Copy-paste templates for each test type:
  - Pure function unit test
  - Data integrity integration test
  - Game system integration test
  - State machine test
- How to add a new test type (with registration macro)
- How to add tests to an existing type
- Common patterns for MUD development

### 4. `COVERAGE_STATUS.md` — Living Coverage Map
- Module-by-module breakdown
- Tier ratings (Easy/Medium/Hard)
- What's tested, what's not, what's deferred
- Updated as tests are added

### 5. `TDD_WORKFLOW.md` — Process Guide
- Why TDD for MUD development
- Red-green-refactor with concrete examples
- TDD patterns: new command, new spell, new game system, new data type
- When to write unit vs integration tests
- Working with the JSON-driven framework in a TDD cycle
- Integration with the build system

---

## Phase 4: Write Tests — Easy Tier

Pure function tests (no MUD environment needed). All JSON-driven unit tests.

| Module | Target Functions | Test Type |
|--------|-----------------|-----------|
| `string.c` | Expand existing coverage to near-complete | Unit |
| `bit.c` | Flag manipulation, bitset operations | Unit |
| `lookup.c` | Table lookups, name resolution | Unit |
| `mem.c` | Allocation/free patterns | Unit |
| `requirements.c` | Validation logic | Unit |
| `const.c`, `tables.c` | Data integrity, table completeness | Unit |
| `sha256.c` | Hash correctness | Unit |
| Buffer operations | Expand existing coverage | Unit |
| Color code handling | Expand existing coverage | Unit |

---

## Phase 5: Write Tests — Medium Tier

Integration tests using the full MUD environment and mock zones.

| Module | Target Functions | Test Type |
|--------|-----------------|-----------|
| `interp.c` | Command table integrity, argument parsing | Integration |
| `handler.c` | Object/mobile linking, lookups | Integration |
| `fight.c` | Damage calculation, combat math | Integration |
| `quest.c` | Data loading, state validation | Integration |
| `reputation.c` | Data integrity | Integration |
| `olc.c` | Editor state management | Integration |
| Wilderness systems | Coordinate handling, terrain resolution | Integration |
| Blueprint/dungeon | Expand existing coverage | Integration |
| `update.c` | Tick logic (testable portions) | Integration |
| Spell data | Table integrity, function pointer validation | Integration |
| Class/skill/trait | Expand existing coverage | Integration |

---

## Phase 6: Deferred — Hard Tier

These modules require significant mocking infrastructure and will be addressed in a future effort:

- Communication layer (`comm.c`, `connection_*.c`)
- Protocol handling (`protocol_*.c`, `mccp.c`)
- Nanny/login state machine (`nanny.c`, `nanny/`)
- Redis caching (`io/cache/`)
- TLS/WebSocket protocols
- Full save/load persistence (`save.c`)
- Channel transport with Redis

---

## Deliverables

1. **Codebase audit** — Complete module-by-module catalog of tested vs. untested code
2. **Framework improvements** — Assertion macros + registration macro in test framework
3. **5 new documentation files** — Replacing 9 old files in `docs/testing/`
4. **Easy-tier tests** — Unit tests for ~9 pure-function modules
5. **Medium-tier tests** — Integration tests for ~11 game system modules
6. **Updated coverage status** — Accurate COVERAGE_STATUS.md reflecting final state
7. **Passing test suite** — All new and existing tests pass

---

## Constraints

- All test code remains behind `#ifdef BUILD_TESTS` — zero production overhead
- New tests follow the existing JSON-driven pattern
- Build files (CMakeLists.txt and Makefile) stay synchronized
- No external test framework dependencies (keep the custom framework)
- Tests must be deterministic and not depend on execution order
