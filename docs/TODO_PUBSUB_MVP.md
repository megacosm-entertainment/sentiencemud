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

## MVP Status (Current)

The original MVP target (global channels only) is complete and expanded:

- Completed channel migrations to ChannelService:
	- `gossip` (`GLOBAL`)
	- `ooc` (`GLOBAL`)
	- `flame` (`GLOBAL`)
	- `helper` (role-restricted global)
	- `music` (`GLOBAL`)
	- `immtalk` (immortal-only global)
	- `yell` (`AREA`)
- Transport status:
	- `local`, `redis`, `auto`, and `legacy_iterative` backend modes are wired.
	- `auto` failover/recovery behavior has startup grace + retry promotion logic.
	- Runtime Redis re-enable/rewarm controls exist via immortal `cachestats` subcommands.
- Stage 3 complete: `tell`/`reply` routed through `channel_service_send_directed` with DIRECT_ENTITY scope.
	- Recipient fields (`recipient_uid/id0/id1`) added to transport pipeline (CHANNEL_MESSAGE, LOCAL_EVENT, REDIS_OUTBOUND_EVENT).
	- Delivery-side ignore check in `channel_deliver_tell_legacy` for defensive depth.
	- 19/19 channel tests pass (282 total, 254 pass, no regressions).

## Post-MVP Next Stages

### Stage 1: Scope-Aware Routing & Subscription Core ✓ COMPLETE (2026-02-22)

Goal: move from broad ingest+local filtering toward proper scope-targeted topic routing.

Tasks:
- ✓ Canonical topic builders for all supported scopes (`GLOBAL`, `AREA`, `REGION`, `GROUP_ID`, `CHURCH_ID`, `DIRECT_ENTITY`, `ROOM_WV`).
- ✓ Server-level subscription manager (`channel_service_sync_subscriptions`) with refcounted topic registry; runs every pulse.
- ✓ `auto` backend transitions rebuild subscriptions on failover/recovery.
- ✓ Recipient-level checks (`ignore`, channel flags, quiet, penalties) as final delivery gate.

Exit criteria:
- ✓ No per-player Redis subscriptions; bounded server-side topic set.
- ✓ Scoped channels only publish/subscribe to required topic families.

### Stage 2: Contextual Channel Migration ✓ COMPLETE (2026-02-22)

Goal: complete non-room channel migration that depends on scoped routing.

Tasks:
- ✓ `gtell` (`GROUP_ID`) — wired through channel service; topic built per group ID; legacy delivery handles membership check.
- ✓ `chtalk` (`CHURCH_ID`) — wired through channel service in `church.c`; topic built per church UID.
- Region/continent channels (`REGION`) — deferred; no named region channels in current game design. Infrastructure (topic builder, sync) is in place.

Exit criteria:
- ✓ `gtell` and `chtalk` route through channel service on local/auto/redis backends.
- ✓ Subscription sync updates on player movement and membership transitions (pulse-driven).

### Stage 3: Direct Messaging Parity ✓ COMPLETE (2026-02-22)

Goal: migrate direct channels while preserving current UX parity.

Tasks:
- ✓ Migrate `tell`/`reply` to `DIRECT_ENTITY` topics.
- ✓ Preserve AFK buffering, offline/linkdead handling, reply pointer semantics, and ignore/visibility rules.
- ✓ `CHANNEL_MESSAGE` extended with `recipient_uid/id0/id1` fields.
- ✓ `channel_service_send_directed()` API added.
- ✓ `channel_deliver_tell_legacy()` with delivery-side ignore check.
- ✓ `do_tell` routed through `channel_service_send_directed`; sender echo and reply pointer set synchronously, recipient delivery via transport.
- ✓ 2 new tests: `channel_tell_directed_delivery`, `channel_tell_ignore_filter` (19/19 channel tests pass).

Exit criteria:
- ✓ Behavioral parity with existing `tell` path in all edge cases.

### Stage 4: Room Speech Migration

Goal: migrate `say`-family channels to `ROOM_WV` without breaking scripts.

**Design notes (2026-02-22):** Three room types require distinct topic strategies:
- Regular rooms: `rt:room:v:<vnum>`
- Wilderness virtual rooms (`room->wilds != NULL`): `rt:room:wv:<wilds_uid>:<x>:<y>`
- Instanced clone rooms (`room->id[0|1] != 0`): `rt:room:inst:<source_vnum>:<id0>:<id1>`

`do_say` has additional complexity: mid-sentence punctuation split (one say → two messages),
randomized verb form (50/50 "says" vs "'...' says N"), and in-place TRIG_SPEECH trigger fanout.
These must be resolved in `do_say` before calling channel service.

Tasks:
- Add `channel_build_room_scope_topic()` handling all three room types.
- Migrate `say`/`whisper`/`sayto` room-scoped message flow.
- Preserve trigger ordering (`display first`, then `TRIG_SPEECH` entity trigger fanout).
- Add low-retention room history policy (`MAXLEN`/age-based trim).

Exit criteria:
- Script trigger behavior matches existing semantics.
- Room history bounded by channel policy.

### Stage 5: Moderation & Reporting Layer

Goal: add enforceable, auditable, per-channel moderation on top of stabilized transport.

Tasks:
- Implement channel penalties (`warn/mute/ban`) with character/account scope.
- Add moderator command surface (`chanwarn`, `chanmute`, `chanban`, penalty list/remove/history).
- Add incident references and review queue integration.

Exit criteria:
- Publish-time enforcement works for both character and account penalties.
- Staff has searchable audit trail tied to incidents.

### Stage 6: Subscription Lifecycle & History Correctness (Next)

Goal: ensure scoped topics are created only when needed, retained briefly for continuity, and dropped when no longer relevant, while preserving correct history semantics for movement/membership/direct channels.

#### 6.1 Topic lifecycle policy (refcount + grace TTL)

Design:
- Keep server-owned refcounted topic subscriptions (already in place) and add delayed cleanup per topic when refcount drops to zero.
- Apply scope-aware grace windows before unsubscribe to absorb churn:
	- `ROOM_WV`: 10 minutes
	- `GROUP_ID`, `CHURCH_ID`, `DIRECT_ENTITY`: 30 minutes
	- `AREA`, `REGION`: 10 minutes (or config override)
- If a topic becomes active again during grace, cancel pending unsubscribe.

Why:
- Avoids rapid subscribe/unsubscribe thrash during movement, group invite churn, reconnects.
- Preserves short-term continuity for late-arriving messages/history fetch.

Exit criteria:
- No immediate unsubscribe on transient refcount drops.
- Topic set remains bounded and converges after inactivity.

#### 6.2 Previous-subscription tracking for context-aware history

Design:
- Track each active entity's previous effective topic set in channel service.
- On location/group/church changes, compute `{removed, added}` delta from previous set.
- Fetch recent history for newly added topics.
- Optionally fetch small trailing history for recently removed room/group topics for continuity UX (configurable).

Why:
- Movement and membership transitions otherwise lose context or duplicate replay.

Exit criteria:
- Movement/group membership transitions produce deterministic history hydration with no duplicate floods.

#### 6.3 Membership-driven subscription updates

Design:
- Group/channel scope transitions must react to both movement and membership events:
	- join/leave group
	- group disband
	- church join/leave
	- direct entity online/offline transitions
- Ensure subscription sync is triggered by event hooks where available, with pulse reconciliation as fallback safety net.

Exit criteria:
- Group/church scope subscriptions update immediately on membership changes and remain correct after pulse reconciliation.

#### 6.4 Direct entity (`tell`) history model

Problem:
- Topic history for `DIRECT_ENTITY` alone is awkward for conversation retrieval since relevant messages are those where user is sender or receiver.

Design target:
- Keep delivery topics per entity (`rt:entity:<id1>:<id2>`) for fanout.
- Add conversation history indexing keyed by participant pair (canonicalized uid pair), e.g. `hist:dm:<low_uid>-<high_uid>`.
- On directed send, write one history record with sender/recipient metadata.
- For replay/query, fetch records where entity is participant (pair index + optional recent-peer index).

Exit criteria:
- Tell history retrieval returns both sent and received messages with correct ordering.
- No dependence on recipient being currently subscribed to reconstruct conversation history.

#### 6.5 Channel policy surface (cedit + defaults)

Add policy fields (or equivalent runtime config) to control:
- history max length
- history max age
- subscription grace TTL
- hydration replay count/window

Current note:
- `history_max_len` and `history_max_age_seconds` already exist in channel definitions; wire runtime enforcement and add grace/hydration policy fields next.

## Immediate Implementation Slice (In Progress)

Stages 1–3 remain complete. Stage 4 has now started with foundational routing work:

Completed in current slice:
1. `CHANNEL_SCOPE_ROOM_WV` topic building in `channel_service.c`:
	- regular room: `rt:room:v:<vnum>`
	- wilderness room: `rt:room:wv:<wilds_uid>:<x>:<y>`
	- instance/clone room: `rt:room:inst:<source_vnum>:<id0>:<id1>`
2. Subscription sync now includes `ROOM_WV` scope for NPC context reconciliation.
3. Channel defaults now include `say`, `whisper`, and `sayto` as `ROOM_WV` definitions.
4. `cedit` now exposes and persists `topic_pattern` editing (`topic <pattern|clear>`), so scoped/topic overrides are manageable in-editor.
5. Integration coverage added for regular + wilderness room subscription topics.

Next implementation slice (active target):
1. Migrate `do_say` send path to `channel_service_send(..., "say", ...)` while preserving exact output/trigger ordering.
2. Add legacy room-delivery dispatcher path keyed by `say` for transport receive handling.
3. Migrate `do_sayto` and `do_whisper` to room-scoped channel service paths (or documented intentional direct-local path where required by trigger semantics).
4. Add parity tests for room speech behavior (format variants + trigger ordering + ignore/quiet compatibility where applicable).

Stage 5 moderation remains in-progress; complete account-scope parity and incident-reference linking after room speech migration stabilizes.

### Next Concrete Slice (Subscription/History)

1. Implement topic grace-TTL unsubscribe scheduler in `channel_service_sync_subscriptions` flow.
2. Add per-entity previous-topic snapshot and delta-based history hydration hooks.
3. Add membership event triggers (group/church join/leave) to force sync pass, keeping pulse sync as fallback.
4. Implement directed-message conversation history index keyed by participant pair.
5. Add integration tests for:
	- room move churn (topic add/remove with grace)
	- group membership add/remove subscription delta
	- tell history includes sent+received paths.

### Stage 7: Dynamic Channels (Dungeon/Instance-Attached)

Goal: support channels created from runtime context (dungeon/instance/blueprint attachments) with strict access boundaries and automatic lifecycle management.

#### 7.1 Source of truth and composition

Design:
- Keep global channel definitions in `channels.json` as reusable templates.
- Attach channel template IDs to content indexes:
	- `DUNGEON_INDEX_DATA.channel_defs`
	- `BLUEPRINT.channel_defs`
- At runtime, derive effective dynamic channels from template + context:
	- scope key (dungeon uid / instance uid / room instance key)
	- optional display alias (future)

Rules:
- No ad-hoc mutation of template definitions at runtime.
- Runtime context only supplies scope identifiers and access checks.

#### 7.2 Access control semantics (hard requirements)

Dungeon-scoped channel:
- A player may publish/receive only while currently in that dungeon context.
- Leaving dungeon removes eligibility immediately (with subscription grace window only for transport churn, not visibility).

Instance-scoped channel:
- A player may publish/receive only while in that instance context.

Blueprint-attached instance channels:
- Membership is determined by resolved instance section/owner policy (existing instance ownership/group checks where applicable).

Security invariant:
- Delivery gate always revalidates context membership, even if subscription cache is stale.

#### 7.3 Topic naming and routing

Design target:
- Keep stable topic families, context-qualified:
	- `rt:dungeon:<uid1>:<uid2>:<channel_id>`
	- `rt:instance:<uid1>:<uid2>:<channel_id>`
- Use deterministic builders in channel service so all publishers/subscribers converge on identical keys.

Notes:
- Existing scope builders already emit dungeon/instance topics; extend to include channel namespace where needed for multi-channel-per-context isolation.

#### 7.4 Runtime lifecycle

Create:
- Channel becomes active when at least one eligible entity is present and subscription refcount > 0.

Retain:
- Apply Stage 6 grace TTL when refcount drops to 0.

Remove:
- Unsubscribe after grace expiry.
- History retention follows per-channel policy (`history_max_len`, `history_max_age_seconds`).

#### 7.5 History behavior for dynamic channels

Dungeon/instance history:
- Hydrate when entering context from previous-subscription delta.
- Do not expose history once actor is no longer context-eligible, except optional local replay cache already fetched.

Room-derived dynamic channels:
- Same delta hydration policy as Stage 6 room movement.

#### 7.6 Editor and tooling surface

Required authoring features:
- `dedit` / dungeon index editor: manage `channel_defs` list.
- `bpedit` / blueprint editor: manage `channel_defs` list.
- Validation command: ensure attached channel IDs exist in registry.

Optional (later):
- Context-local overrides (format/modifier) layered over template with explicit precedence.

#### 7.7 Test gates

Add integration tests for:
- enter dungeon => subscription and delivery enabled
- leave dungeon => delivery denied immediately; subscription drops after grace
- group change inside dungeon => group-scoped dynamic channels update correctly
- two parallel dungeon instances with same index do not cross-deliver
- blueprint-attached channel defs are loaded/saved round-trip in area JSON
