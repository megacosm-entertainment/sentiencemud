# PLAN: `rview` Staff Review Queue Tool (Phase 1 Spec)

## Purpose

`rview` is the immortal-facing moderation review command for channel filter events captured in the staff review stream.

- Stream name: `audit:filtered_messages`
- Schema version: `1`
- Source: channel filtering decision path in `channel_service_send()`

## Command Surface (Phase 1)

- `rview list [limit]`
  - Show recent review entries from `audit:filtered_messages`
- `rview read <stream_id>`
  - Show full payload and context for one entry
- `rview ack <stream_id> [note]`
  - Mark review item acknowledged; include optional staff note

## Payload Contract

Each review event carries:

- `schema` (int)
- `decision` (`allow|redact|block|review`)
- `reason` (string)
- `channel` (string channel id)
- `original` (original text)
- `delivered` (final text delivered after filtering, if any)
- sender metadata from channel message envelope (`sender_name`, `sender_uid`, `sender_id0`, `sender_id1`, `timestamp`)

## Phase 1 Deliverable Scope

Phase 1 provides:

- command shape and permission boundary (`IS_IMMORTAL`)
- list/read/ack action flow in command handler
- explicit stream and payload contract in code/docs

Phase 2+ will provide:

- backend history retrieval/rendering for `list/read`
- persistent ack state tracking
- advanced moderation actions from `rview`
