# CI Bootstrap Root Workflow

This document captures the current bootstrap + test workflow for isolated roots (e.g. CI runners), and the path-resolution strategy used to avoid hardcoded `/sentience` assumptions at runtime.

## Goal

Run bootstrap and tests against a temporary root (for example `/tmp/sent_bootstrap_run`) without requiring writes to the primary game tree.

## Current Strategy

- Keep compile-time path macros in `merc.h` (e.g. `AREA_DIR`, `AREA_LIST`, `PERSIST_FILE`) as canonical defaults.
- Resolve runtime paths at I/O call sites via `resolve_game_path(...)`.
- Use `--data-root=...` / `--bootstrap-root=...` to switch runtime root.

This avoids invasive macro rewrites while making runtime behavior root-aware.

## Implemented Path-Resolution Coverage

### Test framework pathing
- `src/test_integration.c`
  - Test data dir resolution now checks:
    1. `SENTIENCE_TEST_DATA_DIR`
    2. `data/tests`
    3. `src/tests/data`
    4. `/sentience/src/tests/data`
  - Test config resolution now checks:
    1. `SENTIENCE_TEST_CONFIG`
    2. `<resolved_test_data_dir>/test_config.json`

### Bootstrap CI fixture support
- `src/bootstrap/bootstrap_files.c`
  - Added `create_ci_test_data_files()` to copy test JSON data into `data/tests`.
  - `ensure_directory_structure()` now creates `data/tests`, `data/tests/unit`, `data/tests/integration`.
- `src/bootstrap/bootstrap.c`
  - CI fixture mode now runs both:
    - `create_ci_test_fixture_areas()`
    - `create_ci_test_data_files()`

### Area loading/saving and boot area list
- `src/db.c`
  - Area list and area directory loading use `resolve_game_path(AREA_LIST/AREA_DIR, ...)`.
- `src/io/json/json_area.c`
  - `json_area_load()` and `json_area_save()` now resolve `AREA_DIR` via `resolve_game_path`.

### Persistence and instance loading
- `src/io/json/json_persist.c`
  - Persist JSON/dat paths are resolved through helper wrappers that call `resolve_game_path`.
- `src/db.c`
  - `persist_save()` and `persist_load()` resolve `PERSIST_FILE`.
  - Persist JSON directory fallback probe resolves `PERSIST_JSON_OBJECTS`.
  - `load_instances()` resolves both `INSTANCES_FILE_JSON` and `INSTANCES_FILE` (including archive rename targets).

## Bootstrap/Test Commands (Isolated Root)

From `src/`:

```bash
# Build with tests
./build tests

# Bootstrap isolated root with CI fixtures + test data
/sentience/src/.build/Debug/sent \
  -bootstrap \
  --bootstrap-auto \
  --bootstrap-ci-fixtures \
  --bootstrap-root=/tmp/sent_bootstrap_run \
  --bootstrap-username=bootstrapci \
  --bootstrap-email=bootstrapci@example.com \
  --bootstrap-password=bootstrap123

# Preferred CI-root profile run
/sentience/src/.build/Debug/sent \
  -test:profile:bootstrap_ci \
  --data-root=/tmp/sent_bootstrap_run
```

## Test Profiles

- `bootstrap_ci` profile in `src/tests/data/test_config.json` is intended for isolated bootstrap roots.
- It targets suites that are currently stable with bootstrap fixture data.

## Known Gaps / Next Migration Targets

Additional `_DIR`/path-macro call sites still exist and should be migrated to `resolve_game_path` over time, prioritizing:

1. Boot-critical file I/O (`db.c`, `save.c`, `io/json/*` loaders)
2. Runtime persistence and migration paths
3. Any direct `fopen/access/stat/mkdir` calls touching game-data paths

## Why Not Replace `merc.h` Macros Directly?

Replacing the macros globally would be high-risk and invasive. Current approach keeps backward compatibility and uses opt-in runtime root handling where I/O actually happens.

A future refactor can introduce a centralized path API layer if desired, but call-site migration already provides practical CI isolation.

## GitHub Actions Direction

For a future workflow:

1. Build test binary (`./build tests`).
2. Bootstrap into a temporary root (`--bootstrap-root=$RUNNER_TEMP/...`).
3. Run `-test:profile:bootstrap_ci --data-root=...`.
4. Optionally run broader profiles as migration coverage improves.

## Implemented Workflow

Workflow file: `.github/workflows/ci-bootstrap-tests.yml`

- Trigger: pull requests targeting:
  - `master`
  - `v2_testport`
  - `legacy_server`
  - `olc_blueprints`
  - `feature/widevnum-migration`
- Trigger: pushes to protected branches:
  - `master`
  - `v2_testport`
  - `legacy_server`
  - `olc_blueprints`
  - `feature/widevnum-migration`
- Also supports manual `workflow_dispatch`.
- Uses container image: `ghcr.io/megacosm-entertainment/sentience_trixie:latest`.
- Steps:
  1. Checkout
  2. Build dependencies (`.deps/build_deps.sh`)
  3. Build tests (`./build tests`)
  4. Bootstrap isolated root under `$RUNNER_TEMP/sent_bootstrap_root`
  5. Run `-test:profile:bootstrap_ci --data-root=$RUNNER_TEMP/sent_bootstrap_root`

This gives PR-time regression coverage without touching production runtime data.

## Required Checks (Branch Protection)

To block merges unless bootstrap + tests pass, configure branch protection on each protected branch and add this required status check:

- `CI Bootstrap Tests / Bootstrap-root regression tests`

Recommended branch protection settings:

1. Require a pull request before merging
2. Require status checks to pass before merging
3. Select `CI Bootstrap Tests / Bootstrap-root regression tests`
4. Optionally require branches to be up to date before merging

With this, merges are blocked automatically when bootstrap or `bootstrap_ci` tests fail.

## Discord Notifications

### Merge/direct push notifications

Workflow: `.github/workflows/merges.yml`

- Sends merge/direct-push notifications for protected branches.
- Webhook selection order:
  1. Branch-specific (`GHA_LEGACY_DISCORD_WEBHOOK` for `legacy_server`, `GHA_20_DISCORD_WEBHOOK` for `v2_testport`)
  2. Fallback generic `GHA_MERGES_DISCORD_WEBHOOK`

### CI bootstrap test result notifications

Workflow: `.github/workflows/ci-bootstrap-discord.yml`

- Triggered on completion of `CI Bootstrap Tests` (`workflow_run`).
- Posts pass/fail status and run link to:
  - `GHA_CI_TESTS_DISCORD_WEBHOOK`
