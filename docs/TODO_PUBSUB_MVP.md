# TODO: PubSub Communications MVP Backlog

This document turns the pubsub plan into an executable MVP backlog with concrete code touchpoints.

## Goal (MVP)

Ship a production-safe first slice that migrates **global channels** (`gossip`, `ooc`) to a unified channel service and transport abstraction, with Local fallback and no behavior regressions for players.

MVP excludes `say`, `tell`, `gtell`, `yell`, `chtalk` migration, moderation commands, and report tooling.

## Core Design Decisions (Locked)

- Group-scoped channels use `GROUP_DATA->id[0]:id[1]` (`GROUP_ID` scope), not leader identity.
- Region-scoped channels use region UID (`REGION` scope), with optional multi-target routes for continent-wide channels.
- Channel handling is backend-agnostic; `do_*` commands call only channel service APIs.
- Transport backend supports `local` immediately and `redis` in the same interface.
- If Redis fails, gameplay continues via local transport (`auto` mode failover).
- Main game loop is single-threaded and must never block on transport I/O.
- Any blocking Redis listen/read path must run in dedicated worker thread(s) with bounded queue handoff.

## Phase 0: Foundation (Compile-Time Safe)

### 0.1 Add channel service skeleton

**Files**
- `src/channel_service.h` (new)
- `src/channel_service.c` (new)
- `src/CMakeLists.txt`
- `src/Makefile`

**Tasks**
- Define `CHANNEL_SCOPE` enum: `GLOBAL`, `AREA`, `REGION`, `ROOM_WV`, `DIRECT_ENTITY`, `GROUP_ID`, `CHURCH_ID`.
- Define `CHANNEL_MESSAGE` and `CHANNEL_DEFINITION` minimal structs.
- Implement `channel_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text)` with stub routing.
- Keep implementation no-op-safe (returns false with clear logging if unconfigured).

**Exit Criteria**
- Clean build in Debug and Tests.
- No runtime behavior change yet.

### 0.2 Add transport abstraction

**Files**
- `src/channel_transport.h` (new)
- `src/channel_transport.c` (new)
- `src/channel_transport_local.c` (new)

**Tasks**
- Define transport interface: publish, subscribe, unsubscribe, append_history, fetch_history, health_check, name.
- Implement LocalTransport with in-process dispatch stubs and bounded in-memory ring buffer interface.
- Add backend selector setting (`auto|redis|local|legacy_iterative`) read from game settings with default `legacy_iterative`.
- Define queue contracts for cross-thread handoff (outbound publish queue, inbound delivery queue) with explicit overflow policy.

**Exit Criteria**
- Build passes.
- Backend selection logs once at startup.

## Phase 1: Global Channel MVP Migration (`gossip`, `ooc`)

### 1.1 Wire channel definitions for global channels

**Files**
- `src/channel_service.c`
- `src/act_comm.c`

**Tasks**
- Add static definitions for `gossip` and `ooc`.
- Preserve existing toggle bits (`COMM_NOGOSSIP`, `COMM_NO_OOC`) and quiet/ignore checks.
- Preserve current player-flag formatting behavior.

**Exit Criteria**
- `do_gossip` and `do_ooc` call `channel_send(...)` for message path.
- Player-visible text remains unchanged.

### 1.2 Add LocalTransport receiver path

**Files**
- `src/channel_service.c`
- `src/channel_transport_local.c`

**Tasks**
- Deliver channel events to connected descriptors in process.
- Apply recipient checks in receiver path: `COMM_QUIET`, channel-off bit, `is_ignoring()`.
- Ensure sender self-echo formatting stays channel-appropriate.

**Exit Criteria**
- Functional parity with prior gossip/ooc behavior in local backend.

## Phase 2: Redis Backend (Optional in MVP, Recommended Immediately After)

### 2.1 Add Redis transport implementation

**Files**
- `src/channel_transport_redis.c` (new)
- `src/channel_transport.h`
- `src/io/cache/redis_cache.c` (only if shared helpers are reused)

**Tasks**
- Implement publish + history append (`PUBLISH` + `XADD`) with generated topic names.
- Run Redis subscription/listen in dedicated worker thread(s) only; main loop may only enqueue/dequeue without blocking.
- Do not couple to cache mutex assumptions in existing cache module without review.

**Exit Criteria**
- Backend `redis` starts cleanly when Redis is available.
- `auto` mode falls back to local on Redis unavailability.
- Instrumentation shows no blocking transport calls on the game loop thread.

## Phase 3: Operational Safety and Test Gates

### 3.1 Test coverage

**Files**
- `src/tests/integration/channel_pubsub_tests.c` (new)
- `src/tests/data/channel_pubsub_tests.json` (new)
- `src/tests/framework/...` dispatcher update(s)
- `src/CMakeLists.txt`
- `src/Makefile`

**Minimum test cases**
- `gossip` delivery to eligible recipients only.
- `ooc` delivery with ignore filtering.
- Channel-off bit suppresses receive.
- Quiet mode suppresses receive.
- Backend `local` path passes all above.
- Backend selection mode parse (`legacy_iterative`, `local`, `auto`, `redis`).
- Queue overflow behavior does not stall the game loop (drop/degrade policy verified).

### 3.2 Rollout gates

- Gate 1: Build Debug + Tests succeeds.
- Gate 2: `./sent -test:integration` passes new channel tests.
- Gate 3: Manual smoke in-game with 2+ clients for `gossip` and `ooc`.
- Gate 4: Enable `channel_backend=local` in staging and observe for 24h.
- Gate 5: Enable `channel_backend=auto` with Redis up/down drills.
- Gate 6: Stress-test chat burst and verify loop responsiveness (no pulse stalls attributable to channel transport).

## First Coding Slice (Start Here)

Implement these in one PR:

1. Add `channel_service.*` and `channel_transport.*` + local backend skeleton.
2. Add compile integration in both build systems.
3. Refactor only `do_gossip` and `do_ooc` send paths to call service.
4. Keep existing legacy path behind `legacy_iterative` mode for rollback.
5. Add at least 3 integration tests (basic delivery, ignore, channel-off).

## Out of Scope for This MVP PR

- `say` scripting trigger migration.
- `tell`/reply buffering and AFK behavior migration.
- `gtell` migration to `GROUP_ID` scope.
- Region/continent channel migration (`REGION` scope + multi-target route sets).
- `chtalk` migration.
- Moderation command suite (`chanmute`, `chanban`, etc.).
- Staff review queue (`rview`) and reporting UI.

## Follow-up Priority Order

1. `yell` (`AREA`, with `area_topic` override support)
2. `gtell` (`GROUP_ID`)
3. region/continent channels (`REGION`, with optional multi-target routing)
4. `chtalk` (`CHURCH_ID`)
5. `tell` (`DIRECT_ENTITY`) including AFK/offline buffering parity
6. `say` (`ROOM_WV`) with strict trigger ordering parity
