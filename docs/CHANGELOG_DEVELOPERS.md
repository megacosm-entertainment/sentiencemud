# Developer Changelog

Scope: `df6c9d2c3c0c1e5c93172ce2da08868a2c9aa0e4` → `88602021f775e53d514521366df2948fdffb37ea` (current HEAD)

Range size:
- 1438 commits
- 1020 files changed
- 535,501 insertions / 162,455 deletions

This changelog is intentionally diff-driven (not commit-message-driven) and highlights architecture-level additions, notable bug-fix streams, and removals.

Compatibility note: this range was implemented with an explicit bias toward backward compatibility, staged migration, and bridge layers to avoid abrupt runtime/content breakage.

## Foundational Migrations (New Systems)

- Pronoun/body-type identity model and associated login/creation flow updates.
- Account system modernization (auth centralization, preferences, penalties, unlock/migration helpers).
- Storage/persistence modernization via broad JSON IO adoption.
- Widevnum migration work across loaders, runtime helpers, tests, and cross-area resolution surfaces.
- Race/class/skill/song/trait evolution into structured data modules and bootstrap seed flows.
- New event subsystem (`evtedit` + runtime `event`) with scheduling, phases, brackets, and script wiring.
- Quest system evolution with substantial runtime/backend changes.
- Wilderness subsystem expansion into explicit state/storage/mod/link/map/terrain modules.

## Compatibility Strategy Highlights

- Prefer additive subsystem introduction before legacy retirement.
- Keep command/runtime surfaces stable while internals are refactored.
- Use helper layers, bootstrap seeds, and broader test coverage to prevent migration regressions.
- Remove legacy files only after replacement paths are operational.

## Additions

### Data-Driven Persistence and IO
- New `io/json/*` suite covering account, character, area, settings, chat, notes, staff, projects, corpses, reserved values, socials, sectors, and common/persist helpers.
- New `io/common.*` and expanded persistence boundaries for cleaner serialization flow.

### OLC and Editor Platform Split
- Broad editor modularization under `src/editors/*` with shared framework (`editors/common.c`, `editors/common.h`, `editors/common/*`).
- Added dedicated editor modules for events, channels, socials, races, classes, skills, reputation, sectors, traits, dungeons, wilderness, projects, and more.

### Event Runtime + Editor System
- Event definition/editor/runtime surface introduced and expanded (`event_types.*`, `events.c`, `editors/events/evtedit.c`).
- Bracket specs, aggregation modes, phase plans, scheduling, and script control/read surface integrated.

### Channel Service Architecture
- Added channel subsystem modules:
    - `channels/channel_registry.*`
    - `channels/channel_service.*`
    - `channels/channel_policy.*`
    - `channels/channel_filter.*`
    - `channels/channel_review.*`
    - `channels/channel_moderation.*`
    - `channels/channel_transport*`
- Supports local and Redis-backed transport behavior.

### Connection / Protocol Modernization
- Added abstract connection layer (`connection.*`) with TCP/TLS/WebSocket implementations.
- Added protocol-layer split (`protocol_layer.*`, `protocol_telnet.c`, `protocol_websocket.c`).

### Authentication, Secrets, Logging
- Added account auth subsystem (`account/auth*`, `account/otp.c`, `account/penalty*`, `account/preferences*`, `account/unlock*`).
- Added centralized secrets subsystem (`secret.*`).
- Added new structured logging subsystem (`log.*`) and associated docs.

### Runtime Data Systems
- Added class/skill/song/trait/item-type data modules:
    - `class_data.*`, `skill_data.*`, `skill_group.*`, `song_data.*`, `traits.*`, `item_types.*`, `item_type_mem.c`
- Added wilderness state/storage/mod layers (`wilderness_state.*`, `wilderness_storage.*`, `wilderness_mods.*`, `wilderness_vlinks.*`, `wilderness_wmap.*`, `wilderness_wterr.*`).

### Build, Bootstrap, and Tests
- Added CMake-driven script tooling (`build`, `config`, `compile`, `install`) and top-level CMake definitions.
- Added bootstrap subsystem and seed data under `bootstrap/bootstrap_data/*`.
- Added JSON-driven test framework (`tests/framework/*`, `tests/data/*`, `tests/integration/*`, `tests/unit/*`).

## Bug Fix Streams

### Persistence and Duplication
- Account/character duplication/corruption edge cases were addressed across auth/load/save paths.
- Object duplication and ghost-object classes of bugs were addressed alongside persistence changes.

### Runtime Safety / Stability
- Crash-prone paths in update/cache/runtime handling were hardened.
- TLS/SIGPIPE and connection stability issues were reduced.

### Editor/Validation Hardening
- Event editor validation was expanded (brackets/progress/scheduling semantics).
- Shared editor abstractions improved consistency and reduced command-surface drift.

### Performance-Oriented Work
- Caching and inventory/reconnect hotspots were refactored and optimized.
- Async/Redis cache infrastructure was added (`io/cache/async_cache.*`, `io/cache/redis_cache.*`).

## Removals

Deleted files in this range include:
- `healer.c`
- `locker.c`
- `imc.c`, `imc.h`, `imccfg.h`
- `html.c`
- `msgqueue.c`
- `script_lua.c`
- `social.c`
- `olc_edit_rsg.c`
- `olc_mpcode.c`
- `buildnumber.mak`

## Developer Examples

### Build and run tests
```bash
cd /sentience/src
./build tests
cd /sentience
./sent -test
```

### Fast local build/install loop
```bash
cd /sentience/src
./build
./install
```

### Verify command function registration
```bash
cd /sentience/src
grep -n "do_evtedit" tables.c
grep -n "do_cmdedit" tables.c
```

## Further Reading

- [Build System](BUILD_SYSTEM.md)
- [Bootstrap](BOOTSTRAP.md)
- [Integration Test Framework](INTEGRATION_TEST_FRAMEWORK.md)
- [Testing Framework](testing/TESTING_FRAMEWORK.md)
- [Event Editor Runtime](EVENT_EDITOR_RUNTIME.md)
- [Channels Admin Guide](channels/CHANNELS_ADMIN_GUIDE.md)
- [Script Editor Framework](SCRIPT_EDITOR_FRAMEWORK.md)
- [Event Runtime Worklog](WORKLOG_EVENT_EDITOR_RUNTIME.md)
