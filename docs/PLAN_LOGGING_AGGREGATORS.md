# Plan: Enhanced Logging for External Aggregators (v3)

**Status:** In Progress — Redis Streams + sidecar architecture (updated 2026-03-12)

This document outlines the plan to enhance the Sentience MUD's logging system to support JSON-formatted logging suitable for external log aggregation. OpenSearch is the current sink and is being transitioned from reading `system.log` to consuming a Redis Stream populated by the game.

This plan builds upon the existing `zlog` implementation as detailed in `docs/CLAUDE_LOGGING_REFACTOR.md`.

For unified command/runtime error handling semantics (player-facing + structured + plaintext outputs from one call), see `docs/PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md`. This aggregator plan focuses on transport, buffering, and the MUD-side dispatch pipeline.

## Goals

1.  **Rich JSON Formatting:** Log entries are serialized to structured JSON per `LOGGING_SCHEMA.md`.
2.  **Non-blocking dispatch:** The MUD pushes to Redis and returns immediately. No background thread, no HTTP in-process.
3.  **Resilient buffering:** Redis Streams provide durable, ordered, consumer-group-aware buffering. Entries survive MUD restarts.
4.  **Sidecar handles forwarding:** A separate process (Fluent Bit, Logstash, or custom) reads the stream and forwards to OpenSearch. Retries, backpressure, and batching are the sidecar's concern.
5.  **Configurability:** Stream key, max length, and enable/disable are game settings.

## Architecture

```
MUD process
  log_emit_event()
    ├── zlog → system.log (always)
    └── XADD → Redis Stream (if log_stream_enabled)
                    │
                    ▼
            sentience:log:stream
                    │
              [Sidecar process]
                    │
                    ▼
               OpenSearch
```

### Why Redis Streams over Lists

| Concern | List (LPUSH/BRPOP) | Stream (XADD/XREADGROUP) |
|---|---|---|
| Crash recovery | Entry lost on pop | PEL tracks unacknowledged entries |
| Multiple sinks | Must copy entries | Independent consumer groups |
| Overflow control | Custom logic required | `MAXLEN ~` built-in |
| Observability | `LLEN` only | `XLEN`, `XPENDING`, lag per group |

### MUD-side responsibilities

- `XADD <key> MAXLEN ~ <maxlen> * json <payload>` — fire-and-forget
- If Redis is unavailable, zlog-only path continues; no error thrown
- No background thread, no libcurl, no retry logic in C

### Sidecar responsibilities

- Consumer group member: `XREADGROUP GROUP log_workers <consumer> BLOCK 1000 COUNT 50 > <key>`
- `XACK` after successful delivery to OpenSearch
- Periodic `XAUTOCLAIM` to recover stale pending entries after consumer crash
- Batching, retries, backpressure, and fallback-to-disk all handled externally

## Implementation Phases

### Phase 1: MUD-side stream dispatch ✅ (in progress)

**Schema** (`LOGGING_SCHEMA.md`): Defined. `log_context_t` and `log_event_t` structs exist in `log.h`. The `log_emit_event()` fanout (public → `send_to_char`, staff → `wiznet`, plain → zlog) is implemented.

**Redis Stream push:** Added to `log_emit_event()` in `log.c`:
- Serializes event to JSON (jansson) matching the schema when `log_stream_enabled` is true and Redis is available.
- Single `XADD <key> MAXLEN ~ <maxlen> * json <payload>` call — fire-and-forget.
- Falls back silently to zlog-only if Redis is down.

**New game settings** (all in `SETTING_CAT_REDIS`):
- `log_stream_enabled` (bool, default: `false`) — opt-in; off by default
- `log_stream_key` (string, default: `sentience:log:stream`)
- `log_stream_maxlen` (int, default: `100000`) — Redis trims with `MAXLEN ~`

**Convenience macros** (opt-in, for new call sites, all in `log.h`):
```c
// Route through log_emit_event with pre-filled context
log_security_actor(actor_type, actor_id, msg)
log_combat_hit(attacker, victim, damage)
log_perf(component, duration_ms, msg)
log_char_action(action, char_name, target_char, extra_json)
```

**Existing macros unchanged** — `plog`, `plogf`, `pbugf`, `pbug`, `pdebug`, `log_string`, `log_stringf`, `bug` all route through `log_emit_event` already.

### Phase 2: Sidecar setup (external)

Configure a sidecar to consume the stream and forward to OpenSearch:

**Fluent Bit example** (`/etc/fluent-bit/fluent-bit.conf`):
```ini
[INPUT]
    Name          redis
    Host          127.0.0.1
    Port          6379
    Key           sentience:log:stream
    Consumer      fluent-bit-1
    Group         log_workers

[OUTPUT]
    Name          opensearch
    Host          <opensearch-host>
    Port          9200
    Index         sentience-logs
    tls           On
```

The sidecar owns: batching, retries, `XACK`, `XAUTOCLAIM` for stale entries, backpressure, and fallback to disk if OpenSearch is unreachable.

### Phase 3: `logstatus` immortal command (planned)

Add `do_logstatus` to `act_wiz.c`:
- Show `log_stream_enabled`, stream key, `XLEN` count, estimated sidecar lag
- Allow runtime toggle: `logstatus toggle` calls `game_settings.log_stream_enabled = !enabled`

### Phase 4: Broad call-site migration (ongoing)

The bulk of work: migrating `send_to_char` + ad-hoc log calls in command handlers to `log_emit_event` or the `cmd_error`/`cmd_warn`/`cmd_info` helpers (see `PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md`).

Priority order:
1. Security/auth paths (`nanny.c`, account commands)
2. Admin/immortal commands (`act_wiz.c`)
3. Combat events (`fight.c`)
4. Character creation/deletion (`nanny.c`, `db.c`)
5. Economy/trade (`act_obj.c`, `shop.c`)
6. Remaining `bug()` / `log_string()` call sites

## Configuration Reference

```json
{
  "redis": {
    "log_stream_enabled": true,
    "log_stream_key": "sentience:log:stream",
    "log_stream_maxlen": 100000
  }
}
```

The `server_id` in the JSON payload uses `game_settings.mssp_hostname` (falls back to `"unknown"`).

## Decisions

- **Transport:** Redis Streams with MAXLEN ~; sidecar handles everything after XADD.
- **No libcurl in C:** Removed from scope. Transport concerns belong to the sidecar.
- **No background thread in C:** MUD does synchronous XADD (~microseconds); sidecar blocks on XREADGROUP.
- **Payload format:** Single `json` field per stream entry containing the full JSON document per LOGGING_SCHEMA.md.
- **Overflow:** MAXLEN ~ caps stream size in Redis; sidecar handles fallback-to-disk externally.
- **Lazy serialization:** JSON is only built when `log_stream_enabled && redis_is_available()`.
- **Backward compatibility:** All existing call sites (`log_string`, `bug`, `plogf`, etc.) are unchanged.
