# PLAN: Pub/Sub Phase 2 Scaffold (Listener + Inbound Event Contract)

**Status:** Completed (code-verified 2026-02-26)

## Code Reality Check (2026-02-26)

- `CHANNEL_MESSAGE` history metadata is wired through both local and redis transports (`history_stream`, `history_id`).
- Redis hydration path is implemented (`XRANGE` lookup in `hydrate_event_from_history`).
- Inbound worker subscription loop hardening is implemented (`subscribe_active` guard around `PSUBSCRIBE rt:*`).
- Service layer and transport glue for inbound/outbound flow are present, and integration tests for the scaffolded path exist in `tests/integration/channel_pubsub_tests.c`.


## Scope

This document captures the implemented Phase 2 scaffolding layer for listener worker delivery into the game loop.

## Implemented Contract Additions

`CHANNEL_MESSAGE` now carries transport history metadata:

- `history_stream` (example: `history:rt:gossip`)
- `history_id` (Redis Stream ID returned by `XADD`)

These fields are propagated by:

- local transport queue (`channel_transport_local.c`)
- redis transport queue and publish payload (`channel_transport_redis.c`)

## Redis Worker Behavior

Outbound worker now:

1. `XADD`s message payload to history stream
2. captures returned Stream ID
3. publishes realtime event containing:
   - channel/topic identity
   - sender identity
   - message text
   - `history_stream`
   - `history_id`

Inbound worker decodes and forwards this metadata to the game loop.

## Receiver Flow Status

History-reference hydration is now implemented in the Redis inbound worker thread:

- if an inbound pub/sub payload has no `message_text` but includes `history_stream/history_id`,
  the worker performs `XRANGE <stream> <id> <id> COUNT 1` and reconstructs the message event
- hydrated event is then queued to game loop inbound queue
- game loop remains non-blocking and only processes already-hydrated events

This keeps runtime behavior aligned with the main-loop non-blocking constraint while enabling compact inbound event support.

## Bug Fixes Applied (2026-02-22)

### Inbound Worker PSUBSCRIBE Loop

The Redis inbound worker called `PSUBSCRIBE rt:*` on every loop iteration via `ensure_subscription()`. Each call generated a subscription confirmation reply, so `redisGetReply()` always received confirmations instead of actual published messages. The worker was stuck processing its own subscription confirmations.

**Fix:** Added `subscribe_active` flag. `PSUBSCRIBE` is now sent only once per connection. The flag resets on disconnect or transport init, so reconnection re-subscribes correctly.

### Test Harness Fixes

- `test_channel_local_inbound_dispatch`: `CHANNEL_MESSAGE` was not zeroed, leaving garbage pointers for `history_stream`/`history_id`/`sender_uid` that passed NULL checks but crashed in `strlcpy`. Fixed with `memset`.
- `test_channel_compact_publish_hydrated_delivery`: Fake test players had `id[0]=0` (default), so `channel_find_sender()` couldn't locate them by ID. Fixed by assigning proper unique IDs. Also required `position = POS_STANDING`, `in_room` assignment, and linking characters into `room.people` list for `act_new()` delivery to function.

## End-to-End Delivery Status

Both transport backends now have verified end-to-end delivery:

### Local Backend
`channel_service_send()` → `local_publish()` (queue) → `channel_service_pulse()` → `local_drain_inbound()` → `channel_service_receive_message()` → `channel_find_sender()` → `channel_dispatch_legacy_by_id()` → legacy delivery to recipients.

Verified by `test_channel_local_end_to_end_delivery`.

### Redis Backend
`channel_service_send()` → outbound queue → outbound worker `XADD` + `PUBLISH` → inbound worker `PSUBSCRIBE` receive → optional hydration via `XRANGE` → inbound queue → `channel_service_pulse()` → `redis_drain_inbound()` → `channel_service_receive_message()` → legacy delivery.

Verified by `test_channel_compact_publish_hydrated_delivery`.

## Test Coverage (17 tests, all passing)

- Backend mode selection (4 tests: legacy, local, auto, redis)
- Local transport queue dispatch
- Subscription generation (area UID, area override, movement delta, dedup, group scope)
- Auto backend subscription continuity
- Delivery filtering (COMM_NOCT, COMM_QUIET, ignore)
- Legacy fallback without init
- Redis compact publish with hydration
- Local backend end-to-end delivery
