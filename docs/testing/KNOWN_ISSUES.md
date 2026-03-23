# Known Test Issues

**Last Updated:** March 23, 2026

This document tracks known test failures, skips, and environment-specific issues. Update this file when test status changes.

## Known Failures (1)

### `do_func_table_unique_names` — Duplicate `do_northeast`

- **Suite:** `constants_tables_tests`
- **Test Type:** `const_table_unique_names_test`
- **Status:** FAIL
- **Root Cause:** `do_func_table[]` in `const.c` has `do_northeast` at both index 6 and index 10. This is a real data bug — one entry should be removed or renamed.
- **Impact:** Low — both entries point to the same function, so gameplay is unaffected. The duplicate wastes a table slot.
- **Fix:** Remove the duplicate entry from `do_func_table[]` in `const.c`.

## Known Skips (13)

### Script System Segfaults (4 tests)

The MUD scripting system's `p_percent_trigger()` function segfaults in the test environment because script data is not initialized during test bootstrap. Any function that triggers a script percent-trigger will crash.

| Test | Suite | Affected Function | Trigger |
|------|-------|-------------------|---------|
| `hit_gain_npc_calculations` | `update_cycle_tests` | `hit_gain()` | TRIG_HITGAIN |
| `mana_gain_npc_calculations` | `update_cycle_tests` | `mana_gain()` | TRIG_MANAGAIN |
| `move_gain_npc_calculations` | `update_cycle_tests` | `move_gain()` | TRIG_MOVEGAIN |
| `toxin_gain_npc_calculations` | `update_cycle_tests` | `toxin_gain()` | TRIG_TOXINGAIN |

**Unblock path:** Create a mock or stub for `p_percent_trigger()` in the test environment, or initialize minimal script data during test bootstrap.

### Redis Not Available (4 tests)

Redis server is not running or not configured in the test environment.

| Test | Suite |
|------|-------|
| `redis_available` | `redis_area_cache_tests` |
| `redis_area_cache_after_boot` | `redis_area_cache_tests` |
| `redis_area_cache_format` | `redis_area_cache_tests` |
| `redis_area_warm_queue_processed` | `redis_area_cache_tests` |

**Unblock path:** Start Redis before running tests, or create a Redis mock for the test environment.

### Missing Data (3 tests)

| Test | Suite | Reason |
|------|-------|--------|
| `quest_v1_lookup_functions` | `quest_system_tests` | No v1 quest data loaded in test environment |
| `reset_serialization_limbo` | `reset_tests` | Reset data not available in test context |
| `reset_serialization_bootstrap` | `reset_tests` | Reset data not available in test context |

### Environment-Specific (2 tests)

| Test | Suite | Reason |
|------|-------|--------|
| `script_entity_high_byte_expansion` | `script_engine_tests` | Requires specific entity data not present in test bootstrap |
| `channel_compact_publish_hydrated_delivery` | `channel_pubsub_tests` | Requires full channel system initialization |

## Monitoring

When running the full test suite, the expected baseline is:

```
Total:   432
Passed:  418
Failed:  1
Errors:  0
Skipped: 13
```

Any deviation from these numbers should be investigated:
- **New failures** indicate regressions — fix before merging
- **Fewer skips** means an environment issue was resolved — update this doc
- **More skips** means something broke in test data or environment
- **Errors** (non-zero) indicate test framework issues, not test logic failures
