# Plan: Unified Error Handling & Multi-Sink Logging

## Why this plan exists

Sentience currently has three overlapping logging styles:

1. Legacy `sprintf` -> `log_string` / `log_stringf` / `bug`
2. zlog wrappers (`log_message`, `log_message_f`) and convenience macros (`plogf`, `pbugf`, etc.)
3. Direct command feedback (`send_to_char`, `act`) with no corresponding structured event

This causes drift between player-facing errors, operator-facing logs, and future aggregator payloads.

The target state is **one canonical error/event pipeline** that can emit all of the following from one call site:

- Player-facing message (safe and concise)
- Structured JSON context (for Redis/HTTP aggregator flow)
- Plaintext fallback string (for zlog/local files and degraded modes)

---

## Goals

1. Centralize command/runtime error handling through one API family.
2. Keep existing call sites working during migration (non-breaking).
3. Ensure each event can produce:
   - public message (optional)
   - structured context (optional but encouraged)
   - plaintext fallback (required)
4. Preserve current zlog output while enabling async aggregator dispatch.
5. Establish stable error codes for analytics and automation.

## Non-goals (for initial rollout)

- Rewriting every command in one pass
- Replacing all `send_to_char` informational text (only error flows first)
- Forcing remote logging to be enabled in all environments

---

## Canonical Event Model

### Core concept

Introduce a single internal event type used for both errors and notable operational events.

```c
typedef enum {
    EVENT_SEV_INFO,
    EVENT_SEV_WARN,
    EVENT_SEV_ERROR,
    EVENT_SEV_CRITICAL,
    EVENT_SEV_BUG
} event_severity_t;

typedef enum {
    ERROR_CODE_NONE = 0,
    ERROR_CODE_INVALID_ARGUMENT,
    ERROR_CODE_PERMISSION_DENIED,
    ERROR_CODE_NOT_FOUND,
    ERROR_CODE_STATE_CONFLICT,
    ERROR_CODE_DEPENDENCY_FAILURE,
    ERROR_CODE_INTERNAL,
    ERROR_CODE_SCRIPT_RUNTIME,
    ERROR_CODE_SECURITY
} error_code_t;

typedef struct {
    /* Existing schema-aligned context (LOGGING_SCHEMA.md) */
    const char *actor_type;
    const char *actor_id;
    const char *action;
    const char *target_type;
    const char *target_id;
    int64_t value;
    int64_t duration_ms;
    const char *extra_json; /* optional JSON object string */
} log_context_t;

typedef struct {
    event_severity_t severity;
    const char *category;         /* LOG_* */
    error_code_t error_code;      /* stable machine-readable code */

    const char *public_message;   /* optional player-safe text */
    const char *plain_message;    /* required fallback text */

    const log_context_t *context; /* optional */

    /* captured by macro at call site */
    const char *source_file;
    long source_line;
    const char *source_func;
} log_event_t;
```

### Important rules

1. `plain_message` is always required.
2. `public_message` is optional and must contain no sensitive internals.
3. `error_code` is mandatory for errors (`severity >= ERROR`) and optional for info/warn.
4. Structured serialization must include `error_code` in `context.extra` until schema v2 adds a top-level field.

---

## Emission Pipeline (single call, fan-out outputs)

`log_emit_event()` performs deterministic fan-out:

1. **Public output path (optional):**
   - If a target character/descriptor is provided and `public_message != NULL`, send to player.
2. **Plaintext local path (required):**
   - Route `plain_message` to zlog (`log_message` equivalent behavior).
3. **Structured path (optional/configured):**
   - Serialize to schema JSON and enqueue for Redis/remote dispatcher.

This guarantees parity: one source event drives all channels.

---

## API Layers

### Layer 1: Core emitter

```c
void log_emit_event(const log_event_t *event, CHAR_DATA *public_recipient);
void log_emit_event_f(const log_event_t *base, CHAR_DATA *public_recipient, const char *plain_fmt, ...);
```

- Core logging internals only.
- Used by wrappers and by high-value systems directly.

### Layer 2: Command-centric helpers

```c
void cmd_error(
    CHAR_DATA *ch,
    error_code_t code,
    const char *category,
    const log_context_t *ctx,
    const char *public_msg,
    const char *plain_fmt,
    ...
);

void cmd_warn(...);
void cmd_info(...);
```

- Default category for command parsing failures: `LOG_INFO` or `LOG_ADMIN` depending on command class.
- Ensures every command-level failure can emit player + structured + fallback without duplicate code.

### Layer 3: Compatibility wrappers (migration bridge)

- `log_message*`, `plogf`, `pbugf`, `bug`, `log_string*` become adapters into `log_emit_event*`.
- No mass refactor required before behavior improvement.

---

## Handling "Any Command or Argument"

Use a standard command parse/error pattern:

1. Parse arguments.
2. On invalid syntax/argument/value/state, call `cmd_error(...)` once.
3. Return early.

Example (target style):

```c
if (arg[0] == '\0') {
    log_context_t ctx = {
        .actor_type = "char",
        .actor_id = ch->name,
        .action = "combine",
        .target_type = "command",
        .target_id = "combine",
        .extra_json = "{\"missing_arg\":\"item1\"}"
    };

    cmd_error(
        ch,
        ERROR_CODE_INVALID_ARGUMENT,
        LOG_INFO,
        &ctx,
        "Syntax: combine <item1> <item2>",
        "combine failed: missing required argument item1 (char=%s)",
        ch->name
    );
    return;
}
```

This replaces parallel `send_to_char(...)` + ad-hoc log calls.

---

## Schema alignment strategy

Current schema in `LOGGING_SCHEMA.md` is preserved.

Additions for interoperability:

1. Include `error_code` and `public_message_sent` in `context.extra` for error events.
2. Include `event_id` UUID (or monotonic id) in `metadata` for correlation across local/remote paths.
3. Include `public_message_template` (optional, non-PII) if using message catalogs later.

No breaking change required for schema version 1.

---

## Migration plan

### Phase A: Foundation

1. Add `error_code_t`, `log_event_t`, and `log_emit_event*` in `log.h` / `log.c`.
2. Keep `log_message*` macros intact; route internals through new emitter.
3. Implement safe defaults (no context, no public recipient).

### Phase B: Compatibility bridge

1. Re-implement legacy functions in `db.c` as wrappers:
   - `bug(...)` -> `log_emit_event` with `EVENT_SEV_BUG`
   - `log_string*` -> `EVENT_SEV_INFO`
2. Keep function signatures unchanged to avoid compile churn.

### Phase C: Command error adoption (high value first)

Prioritize commands with high player usage and frequent syntax errors:

1. `act_comm.c`
2. `act_obj2.c`
3. security-sensitive/login/admin flows (`nanny.c`, `act_wiz.c`)

For each adopted command:
- replace direct syntax error `send_to_char` path with `cmd_error`
- include minimal context (`actor`, `action`, relevant argument)

### Phase D: Aggregator integration

1. Wire `log_emit_event` structured branch into Redis queueing from `PLAN_LOGGING_AGGREGATORS.md`.
2. Ensure local zlog path remains active regardless of remote status.
3. Add queue health metrics (`queued`, `dropped`, `fallback_written`).

### Phase E: Enforcement and cleanup

1. Add CI grep checks to flag new direct `bug(` and raw `log_string(` additions.
2. Gradually reduce direct `send_to_char` for error paths where structured telemetry is required.
3. Mark legacy APIs deprecated in headers after migration threshold is reached.

---

## Operational safeguards

1. **Rate limiting:** throttle repeated identical command errors per character/session.
2. **PII controls:** avoid logging secrets in `public_message` and `extra_json`.
3. **Failure isolation:** if structured serialization fails, still emit plaintext zlog entry.
4. **Deterministic fallback:** plaintext emission must never depend on Redis/HTTP availability.

---

## Success criteria

1. New command error paths use `cmd_error` rather than ad-hoc `send_to_char` + `plogf` combos.
2. Every migrated error event has:
   - stable `error_code`
   - player-facing message where applicable
   - plaintext fallback
   - schema-compatible structured payload when enabled
3. Legacy interfaces continue to compile and behave while migration is in progress.

---

## Recommended first implementation slice

Deliver in one small PR:

1. Introduce `log_event_t`, `error_code_t`, `log_emit_event`, `cmd_error`.
2. Wrap `bug` and `log_string*` via new core emitter.
3. Convert 2-3 commands with frequent syntax errors in `act_comm.c`.
4. Add a short operations note in docs on how to query new `error_code` fields.

This proves the architecture end-to-end before broad migration.