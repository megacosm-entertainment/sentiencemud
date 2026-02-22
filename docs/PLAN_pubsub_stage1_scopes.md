# PLAN: PubSub Stage 1 — Scope Routing & Subscription Core

This document defines the immediate implementation plan now that MVP channel migration is complete.

## Objectives

1. Introduce canonical topic generation per channel scope.
2. Introduce server-side dynamic subscription management (topic refcount model).
3. Preserve current player-visible behavior while enabling future scoped channels.

## Current Baseline

- ChannelService currently handles: `gossip`, `ooc`, `flame`, `helper`, `music`, `immtalk`, `yell`.
- Existing transport backends: `legacy_iterative`, `local`, `auto`, `redis`.
- Auto backend supports fallback and retry promotion.

## Stage 1 Deliverables

### D1. Scope-Aware Topic Builder

- Map channel IDs to scope definitions in ChannelService.
- Generate publish topic per message:
  - GLOBAL → `rt:<channel_id>`
  - AREA (`yell`) → `rt:area:<area_uid>`
- Return safe failure for missing context (e.g., area-scoped send without room/area).

### D2. Subscription Manager Skeleton

- Keep a refcounted list of active server topic subscriptions.
- Recompute desired topics periodically from current server state:
  - Always-on global channel topics.
  - Area topics for currently occupied areas.
- Reconcile with active subscriptions using `channel_transport_subscribe/unsubscribe`.

### D3. Lifecycle Integration

- Initialize and prime subscriptions in `channel_service_init()`.
- Re-sync subscriptions on pulse (throttled, e.g., once per second).
- Unsubscribe on `channel_service_shutdown()`.

## Stage 1 Status Snapshot (Current)

This section tracks what is complete right now versus what remains to close Stage 1.

### Completed

- Scope-aware topic generation is active for:
  - `GLOBAL` channels (`rt:<channel_id>`)
  - `AREA` channels (`rt:area:<area_topic|area_uid>`)
  - `REGION` builder support (`rt:region:<region_topic|region_uid>`, with area fallback)
- Server-side subscription reconciliation is implemented:
  - Desired topic set recomputed periodically
  - Subscribe/unsubscribe deltas applied through transport API
  - Topic registry bounded by service max topic count
- Lifecycle wiring is complete:
  - Initial sync at `channel_service_init()`
  - Pulse-driven refresh in `channel_service_pulse()`
  - Unsubscribe cleanup in `channel_service_shutdown()`
- Area/region topic overrides are editable and persisted:
  - Area editor command: `topic <value|clear>`
  - Region editor command: `regions topic <#|default> <value|clear>`
  - JSON persistence for area and region topic overrides
- Admin observability added:
  - `stat mob|char` now includes effective channel topic subscriptions for the target character.

### Remaining for Stage 1 Closure

- Verify/reconcile `auto` backend transition behavior under live resubscription (local ↔ redis) with explicit test coverage.
- Final Stage 1 validation pass and sign-off checklist update.

### Stage 1 Test Coverage Added

- `channel_effective_subscriptions_area_uid`
- `channel_effective_subscriptions_area_override`
- `channel_effective_subscriptions_movement_delta`
- `channel_auto_backend_subscription_continuity`
- `channel_effective_subscriptions_no_duplicates`

## Immediate Next Coding Slice

1. Add integration tests for effective topic generation (`GLOBAL`, `AREA`, override paths).
2. Add integration tests for subscription reconcile on movement transitions.
3. Add a transport-mode transition smoke test (`auto` failover/recovery) asserting subscriptions remain correct.
4. Mark Stage 1 complete and open Stage 2 migration work (`gtell`, `chtalk`).

## Non-Goals (Stage 1)

- No per-player Redis subscriptions.
- No migration of `gtell`/`chtalk`/`tell`/`say` yet.
- No moderation/reporting implementation yet.

## Validation

- Build Debug succeeds.
- Existing migrated channels maintain behavior.
- Topic generation is scope-correct for global and area channels.
- Subscription reconciliation does not block the game loop.

## Next Step After Stage 1

- Stage 2: migrate `gtell` (`GROUP_ID`) and `chtalk` (`CHURCH_ID`) on top of this subscription foundation.
