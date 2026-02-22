# PLAN: Pub/Sub Phase 2 Scaffold (Listener + Inbound Event Contract)

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
