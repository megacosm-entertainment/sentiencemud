# Plan: Enhanced Logging for External Aggregators (v2)

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document outlines the plan to enhance the Sentience MUD's logging system to support JSON-formatted, asynchronous logging suitable for external log aggregation services. This revision incorporates feedback regarding the use of existing infrastructure (Redis, jansson, libcurl) and a desire for richer contextual data.

This plan builds upon the existing `zlog` implementation as detailed in `docs/CLAUDE_LOGGING_REFACTOR.md`.

For unified command/runtime error handling semantics (player-facing + structured + plaintext outputs from one call), see `docs/PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md`. This aggregator plan focuses on transport, buffering, dispatch, and operational resilience.

## Goals

1.  **Rich JSON Formatting:** Log entries must be converted to a structured JSON format, including extended context.
2.  **Asynchronous Processing:** Logging must not block the main game loop.
3.  **Resilient Buffering:** Use Redis to buffer log entries, providing persistence and resilience against remote endpoint downtime.
4.  **External Push:** Push logs via HTTP/S to a configurable endpoint.
5.  **Configurability:** The system should be configurable via game settings.

## Revised Architecture

1.  **Main Thread & Context:** The existing `log_message` and `log_message_f` functions will be updated to accept additional contextual parameters (e.g., function name, actor identifier).
2.  **Serialization:** Inside the logging functions, a JSON object will be created using `jansson` to structure the log message and its full context.
3.  **Redis Queue:** The resulting JSON string will be pushed onto a Redis list (e.g., `LPUSH sentience_log_queue ...`) using the existing Redis client (`hiredis`). This provides a robust, persistent buffer.
4.  **Logger Thread:** A dedicated background thread will run a loop that performs a blocking pop from the Redis list (e.g., `BRPOP sentience_log_queue ...`).
5.  **Batching & Sending:** The logger thread will collect entries from Redis into batches. It will then wrap the batch in a JSON array and `POST` it to the configured HTTP endpoint using `libcurl`.
6.  **Error Handling & Overflow:** If the HTTP endpoint is unresponsive, logs will naturally accumulate in the Redis queue. The logger thread will monitor the queue size. If it exceeds a configured threshold, the thread will enter a fallback mode, popping items from Redis and dumping them to a local file for later processing, preventing unbounded memory use by Redis.

## Phases of Implementation

### Phase 1: Foundational Setup & Rich JSON Logging

#### 1.1 Define Log Schema
See the detailed **Logging Schema & Context Design** document (`docs/LOGGING_SCHEMA.md`) which includes:
- Core JSON entry structure (metadata, source, message, context)
- C data structure `log_context_t` (lightweight, optional context bundle)
- Eight logging scenarios with full JSON examples:
  - Security (login success/failure, MFA, account changes)
  - Combat (PvP damage, mob attacks, player death)
  - Character (creation, deletion, leveling)
  - Administrative (immortal commands, settings changes, audits)
  - System/Performance (slow operations, Redis issues, queue monitoring)
  - Script Execution (script errors, event triggers)
  - Item Transfer & Trading (give, drop, sell, mail, trade)
  - Vault/Storage (deposit, withdraw, rent, purge)
- Query examples for external aggregators (Elasticsearch, Splunk, etc.)

#### 1.2 Update Logging Signatures (Non-Breaking)

Adopt a **context-optional** approach to maintain backward compatibility:

**Core functions (unchanged):**
```c
#define log_message(level, category, message) \
    _log_message_ctx(level, category, message, NULL, __FILE__, __LINE__, __func__)

#define log_message_f(level, category, format, ...) \
    _log_message_f_ctx(level, category, NULL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
```

**New internal functions (context-aware):**
```c
void _log_message_ctx(
    log_level level,
    const char *category,
    const char *message,
    const log_context_t *ctx,  // NULL-safe, optional
    const char *file,
    long line,
    const char *func
);

void _log_message_f_ctx(
    log_level level,
    const char *category,
    const log_context_t *ctx,  // NULL-safe, optional
    const char *file,
    long line,
    const char *func,
    const char *format,
    ...
);
```

**Convenience macros (opt-in, for new code):**
```c
// Security/authentication logging
#define log_security_actor(actor_type, actor_id, msg) \
    do { \
        log_context_t _ctx = {.actor_type = (actor_type), .actor_id = (actor_id)}; \
        _log_message_ctx(LOG_LEVEL_INFO, LOG_SECURITY, (msg), &_ctx, __FILE__, __LINE__, __func__); \
    } while(0)

// Combat/PvP logging
#define log_combat_hit(attacker, victim, damage) \
    do { \
        log_context_t _ctx = { \
            .actor_type = "char", .actor_id = (attacker), \
            .target_type = "char", .target_id = (victim), .value = (damage) \
        }; \
        _log_message_ctx(LOG_LEVEL_INFO, LOG_COMBAT, "Combat hit", &_ctx, __FILE__, __LINE__, __func__); \
    } while(0)

// Performance/system logging
#define log_perf(component, duration_ms, msg) \
    do { \
        log_context_t _ctx = { \
            .actor_type = "system", .actor_id = (component), .duration_ms = (duration_ms) \
        }; \
        _log_message_ctx(LOG_LEVEL_WARN, LOG_DEBUG, (msg), &_ctx, __FILE__, __LINE__, __func__); \
    } while(0)

// Character action logging
#define log_char_action(action, char_name, target_char, extra_json) \
    do { \
        log_context_t _ctx = { \
            .actor_type = "char", .actor_id = (char_name), .action = (action), \
            .target_type = "char", .target_id = (target_char), \
            .extra_json = (extra_json) \
        }; \
        _log_message_ctx(LOG_LEVEL_INFO, LOG_INFO, action, &_ctx, __FILE__, __LINE__, __func__); \
    } while(0)
```

**Key points:**
- All existing `log_message_f(...)` calls remain valid without modification.
- `log_context_t` is stack-allocated; minimal overhead when NULL.
- New code gradually adopts convenience macros for richer context.
- Context serialization to JSON only occurs if remote logging is enabled (lazy evaluation).

#### 1.3 Implement Queuing

Modify `_log_message_ctx()` and `_log_message_f_ctx()` implementations:
1. If remote logging is disabled, call zlog as usual (preserves current behavior).
2. If remote logging is enabled:
   - Serialize context + message to JSON using `jansson`.
   - Create JSON log entry matching schema in `docs/LOGGING_SCHEMA.md`.
   - Push JSON string to Redis list (e.g., `LPUSH sentience_log_queue ...`).
   - Still call zlog for local file logging (dual path).

#### 1.4 Dependency Confirmation

Verify that `hiredis` (Redis client), `libcurl`, and `jansson` are:
- Linked in `CMakeLists.txt` and `Makefile`.
- Properly included in build (check `.deps` or system packages).
- Tested for basic functionality (connection, push/pop, JSON serialization).

### Phase 2: Logger Thread and HTTP Dispatch

1. **Logger Thread Creation:**
   - Create a background POSIX thread at server startup (function `log_remote_thread_start()`).
   - Thread connects to Redis at `game_settings.redis_host:game_settings.redis_port`.
   - Loop: use `BRPOP sentience_log_queue timeout` to efficiently wait for entries.

2. **Batching & HTTP Dispatch:**
   - Collect log entries from Redis into batches (e.g., 100 entries or 5-second timeout).
   - Wrap batch in JSON array: `{ "logs": [entry1, entry2, ...], "batch_id": "...", "sent_at": "..." }`.
   - POST to `game_settings.remote_logging_url` with `Content-Type: application/json`.
   - Include HTTP timeouts (e.g., 10 seconds) via `libcurl` options.

3. **Configuration Addition:**
   Add settings to `json_game_settings.c` (loaded from `game_settings.json` or env overrides):
   - `remote_logging_enabled` (bool) — enable/disable remote logging
   - `remote_logging_url` (string) — HTTP(S) endpoint for log dispatch
   - `remote_logging_redis_key` (string) — Redis list name (default: `sentience_log_queue`)
   - `remote_logging_batch_size` (int) — logs per batch (default: 100)
   - `remote_logging_batch_timeout_sec` (int) — timeout before sending partial batch (default: 5)
   - `remote_logging_redis_dump_threshold` (int) — queue size threshold for fallback (default: 10000)
   - `remote_logging_server_id` (string) — server identifier for logs (auto-populated from `game_settings.server_id` if not set)

### Phase 3: Error Handling and Fallback

1. **Unresponsive Endpoint Logic:**
   - If `libcurl` POST fails (network error, timeout, HTTP error), log the failure locally.
   - Retrieve entries from front of successful send queue and push them back to Redis head (`RPUSH`).
   - Implement exponential backoff (1s, 2s, 4s, 8s, max 60s) before retry.
   - Log retry attempts and eventual success to local logs.

2. **Fallback to Disk (Queue Overflow Prevention):**
   - Before blocking `BRPOP`, check Redis queue size with `LLEN sentience_log_queue`.
   - If size exceeds `remote_logging_redis_dump_threshold`:
     - Switch to "dump mode": `BRPOP` entries and append to fallback file (e.g., `logs/remote_fallback_YYYYMMDD.log`).
     - Continue until queue size drops below 80% of threshold.
     - Log entry count dumped and reason for fallback.
   - Fallback file format: one JSON entry per line (JSONL), easy to re-ingest or process.

3. **Graceful Shutdown:**
   - On server shutdown, send any remaining batched logs before terminating thread.
   - If endpoint unreachable, dump remainder to fallback file.

### Phase 4: Refinement & Control

1. **Dynamic Configuration:**
   - Add immortal command `logstatus` to view:
     - Remote logging enabled/disabled status
     - Current Redis queue size (`LLEN`)
     - Last successful send timestamp
     - Last error (if any) and retry backoff state
     - Fallback file info (if in use)
   - Add `logtoggle` command to enable/disable remote logging at runtime (no restart needed).

2. **Utility Script:**
   - Create `scripts/reingest_logs.py` to read fallback JSONL file(s) and retry sending to remote endpoint.
   - Usage: `python3 scripts/reingest_logs.py --file logs/remote_fallback_20260124.log --url https://logs.example.com/ingest --batch-size 500`
   - Supports resume on failure (track processed lines).

3. **Documentation:**
   - Create `docs/LOGGING_REMOTE_OPERATIONS.md` covering:
     - Configuration examples (game_settings.json)
     - Troubleshooting (queue stuck, endpoint errors, disk fallback)
     - Recovery procedures (reingest script, manual cleanup)
     - Expected performance impact and tuning recommendations

## Updated Decisions

- **Queueing Mechanism:** Redis list (`LPUSH`, `BRPOP`) for robustness and persistence.
- **Contextual Data:** `log_context_t` optional struct; backward-compatible via NULL checks.
- **Signature Strategy:** Non-breaking; existing code works unchanged, new code adopts convenience macros.
- **Unresponsive Endpoint:** Exponential backoff + fallback to disk JSONL for overflow prevention.
- **Lazy Evaluation:** JSON serialization only if remote logging enabled; zero overhead when disabled.
