# Test Coverage Status

**Last Updated:** March 23, 2026  
**Total Test Suites:** 67 (27 unit + 34 integration + 6 framework)  
**Total Test Cases:** 432 (418 passing, 1 failing, 13 skipped)  
**Test Source Files:** 36 (4 framework + 5 unit + 27 integration)  
**JSON Test Definitions:** 67 files  
**Total Source Modules:** 136  

## Test Run Summary

```
=== Test Results ===
Total:   432
Passed:  418
Failed:  1
Errors:  0
Skipped: 13
==================
```

**Known Failure:** `buffer_core_and_failure_paths` — pre-existing assertion issue in buffer uniqueness check.

**Known Skips (13):** Script system functions that trigger `p_percent_trigger()` which segfaults in the test environment (no script data initialized). Affected: `hit_gain()`, `mana_gain()`, `move_gain()`, `toxin_gain()`, and a few trait/quest tests with missing data.

## Module Coverage Table

| Module | Source Files | Tier | Test Suites | Tests | Status |
|--------|-------------|------|-------------|-------|--------|
| **String Processing** | string.c | Easy | core_string, string_edit_helpers, string_editor_state, string_formatting, string_predicates, string_transform, color_* helpers, string_editor | ~65 | ✅ Good |
| **Lookup/Tables** | lookup.c, tables.c | Easy | lookup_helpers, lookup_function, lookup_table | ~15 | ✅ Good |
| **Bit Operations** | bit.c | Easy | bitset_helpers, bit_operations | ~10 | ✅ Good |
| **SHA256 Crypto** | sha256.c | Easy | sha256_unit_tests | 3 | ✅ Good |
| **Constants/Tables** | const.c | Easy | constants_tables | 21 | ✅ Good |
| **Memory Management** | mem.c | Easy | memory_util | 12 | ✅ Good |
| **Requirements** | requirements.c | Easy | requirements_pure_function | ~15 | ✅ Good |
| **Buffer Operations** | (various) | Easy | buffer_core, buffer_permutation, buffer_function, color_string_manipulation | ~30 | ✅ Good |
| **Argument Processing** | (various) | Easy | argument_helpers, pronoun_helpers | ~8 | ✅ Good |
| **Class System** | class_data.c | Medium | class_data_tests | 11 | ✅ Good |
| **Skill System** | skill_data.c, skill_group.c, skills.c | Medium | skill_data, skill_group | 12 | ✅ Good |
| **Song System** | song_data.c | Medium | song_data_tests | 3 | ✅ Good |
| **Item Types** | item_types.c, item_type_mem.c | Medium | item_type_tests | 5 | ✅ Good |
| **Trait System** | traits.c | Medium | trait_system_tests | 6 | ✅ Good |
| **Script Engine** | scripts.c, script_*.c | Medium | script_engine_tests | ~50 | ✅ Good |
| **Chat Rooms** | chat_rooms.c | Medium | chat_rooms, chat_rooms_json | ~10 | ✅ Good |
| **Church System** | church.c | Medium | church, church_json | ~10 | ✅ Good |
| **Blueprint System** | blueprint.c | Medium | blueprint_area_scope, blueprint_data_integrity | ~16 | ✅ Good |
| **Dungeon System** | dungeon.c | Medium | dungeon_data_integrity | ~8 | ✅ Good |
| **Channel/PubSub** | (various) | Medium | channel_pubsub_tests | ~25 | ✅ Good |
| **Area Loading** | db.c, db2.c | Medium | area_loading, json_area, system_area | ~12 | ✅ Good |
| **Reset System** | (various) | Medium | reset, reset_cross_area | ~6 | ✅ Good |
| **Shop System** | (various) | Medium | shop_stock, shop_stock_cross_area | ~6 | ✅ Good |
| **WNUM System** | (various) | Medium | wnum, wnum_parsing, reserved_wnum, widevnum_*, wnum_* helpers | ~30 | ✅ Good |
| **Wilderness** | wilds.c, wilderness_*.c | Medium | wilderness_resolver, wilderness_system | ~8 | ✅ Good |
| **Database Integrity** | (various) | Medium | database_integrity_tests | ~4 | ✅ Good |
| **Instance System** | (various) | Medium | instance, instance_persist | ~6 | ✅ Good |
| **Combat Telemetry** | (various) | Medium | combat_telemetry_tests | ~5 | ✅ Good |
| **Bootstrap** | (various) | Medium | bootstrap_fixture_tests | ~4 | ✅ Good |
| **Redis Caching** | (various) | Medium | redis_area_cache_tests | ~4 | ✅ Good |
| **Command Table** | interp.c | Medium | command_table_tests | 4 | ✅ Good |
| **Handler/Core** | handler.c | Medium | handler_function_tests | 10 | ✅ Good |
| **Combat Math** | fight.c, fight2.c | Medium | combat_math_tests | 2 | ✅ Good |
| **Quest System** | quest.c, gq.c | Medium | quest_system_tests | 13 | ✅ Good |
| **Reputation** | reputation.c | Medium | reputation_system_tests | 8 | ✅ Good |
| **OLC Framework** | olc.c, olc_*.c, editor.c | Medium | olc_framework_tests | 6 | ✅ Good |
| **Spell Data** | magic.c | Medium | spell_data_integrity | 6 | ✅ Good |
| **Update Cycle** | update.c | Medium | update_cycle_tests | 6 | ⚠️ Partial |
| **Sector System** | sectors_runtime.c | Medium | (via constants) | 2 | ⚠️ Partial |
| **Effects** | effects.c | Medium | - | 0 | ❌ None |
| **Events** | events.c, event_types.c | Medium | - | 0 | ❌ None |
| **Magic System** | magic_*.c (24 files) | Medium | - | 0 | ❌ None |
| **Commands** | act_*.c (10 files) | Medium | - | 0 | ❌ None |
| **Miscellaneous** | alias.c, auction.c, autowar.c, ban.c, boat.c, drunk.c, help.c, house.c, hunt.c, invasion.c, mail.c, mount.c, music.c, note.c, project.c, rview.c, scan.c, secret.c, shoot.c, special.c, staff.c, stats.c, storage.c, trade.c, treasuremap.c, weather.c | Medium-Hard | - | 0 | ❌ None |
| **Communication** | comm.c | Hard | - | 0 | 🔒 Deferred |
| **Connection Management** | connection*.c | Hard | - | 0 | 🔒 Deferred |
| **Protocol Layers** | protocol*.c | Hard | - | 0 | 🔒 Deferred |
| **Network Security** | tls.c, mccp.c | Hard | - | 0 | 🔒 Deferred |
| **Player Login** | nanny.c | Hard | - | 0 | 🔒 Deferred |
| **Data Persistence** | save.c | Hard | - | 0 | 🔒 Deferred |
| **Logging** | log.c | Hard | - | 0 | 🔒 Deferred |
| **MXP Links** | mxp_links.c | Hard | - | 0 | 🔒 Deferred |

## Tier Legend

- **Easy** = Pure functions, unit testable, no external dependencies
- **Medium** = Needs MUD environment, game state, or data files
- **Hard** = Network/I/O/external dependencies, requires mocking infrastructure

## Status Legend

- ✅ **Good** = Substantial coverage of key functions and use cases
- ⚠️ **Partial** = Some coverage but significant gaps remain (e.g., tests skip due to segfaults)
- ❌ **None** = No tests exist for this module
- 🔒 **Deferred** = Hard tier module, testing deferred to future phases

## Coverage Summary

**Easy Tier:** 9/9 modules tested (100%) ✅
- All easy-tier modules now have test coverage
- SHA256, memory management, constants, requirements all added in Phase 3

**Medium Tier:** 29/39 modules tested (74%)
- Strong coverage for data systems (classes, skills, songs, items, traits, spells)
- Strong coverage for world systems (areas, blueprints, dungeons, wilderness, quests, reputation)
- Strong coverage for framework systems (commands, handlers, OLC, channels, scripts)
- Partial coverage for update cycle (script system segfaults block gain function tests)
- Missing coverage for magic spell implementations, command actions, effects, events

**Hard Tier:** 0/8 modules tested (0%)
- All network, I/O, and external dependency modules deferred
- Requires connection mocking infrastructure

**Overall:** 38/56 categorized modules have test coverage (68%)

## Bugs Found by Tests

During the testing overhaul, the test suite discovered several real bugs:

1. **Duplicate `do_northeast`** in `do_func_table[]` (const.c indices 6 and 10) — detected by constant table uniqueness tests
2. **`nocolour()` double-brace bug** in db2.c — escape sequence `{{` was copying both braces instead of collapsing to one
3. **`snprintf` buffer size** in sha256.c — buffer was 65 instead of 3 for hex byte formatting
4. **14 commands with NULL function pointers** in command table — detected by command table integrity tests

## Framework Improvements Made

- **10 TEST_ASSERT_* macros** added to `test_framework.h` for consistent assertion patterns
- **Table-based dispatcher** replaced 61-line if/else chain with `handler_table[]` (40+ entries)
- **JSON-driven test suites** — all test data externalized for easy modification
- **Unique prefix convention** — each test module uses a short unique prefix to avoid dispatcher collision

## Next Steps

Priority focus areas for future test development:

1. **Script System Mocking** — Create mock for `p_percent_trigger()` to unblock gain function tests in update cycle, and enable testing functions that interact with the script system
2. **Magic Spell Implementations** — Test individual spell functions in magic_*.c files (24 spell category files)
3. **Command Actions** — Test player commands in act_*.c files with appropriate game state setup
4. **Effects & Events** — Test effect application and event dispatch systems
5. **Hard Tier Infrastructure** — Build connection mocking for network protocol tests