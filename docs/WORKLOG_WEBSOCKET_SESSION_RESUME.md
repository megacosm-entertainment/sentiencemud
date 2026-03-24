# Worklog: WebSocket Session Resume

## 2026-03-12

### Implemented

- Added Phase 1 in-memory resume session support in comm layer.
- Added token issuance helper for WebSocket descriptors:
  - websocket_resume_issue(d)
- Added token verification/reattach helper:
  - websocket_resume_try(d, token)
- Added login command path in account name step:
  - RESUME <token>
- Added prompt hint for WebSocket users:
  - Account name (or RESUME <token>):
- Added token issuance at successful play-state entry and relink paths.
- Added explicit control ACK lines for resume flow:
  - success: Core.Resume {"event":"ok"}
  - failure: Core.Resume {"event":"fail","reason":"invalid_or_expired"}
- Removed user-facing resume failure prose in login flow; failure is now machine-readable control output.
- Adjusted token expiry behavior so active websocket sessions keep/refresh resume entries;
  TTL is effectively enforced from disconnect time.
- Fixed resume token ownership for switched immortals by issuing/arming tokens from
  session owner (descriptor original character when present).
- Removed duplicate token re-issue in websocket resume path to avoid immediate
  rotate-twice behavior on a single resume.
- Hardened token storage by persisting only SHA-256 token hashes in server memory;
  raw resume tokens are no longer stored server-side.
- Added Redis-backed resume persistence with mirrored indexes:
  - ws:resume:player:<id0>:<id1> -> token_hash
  - ws:resume:token:<token_hash> -> "id0:id1"
- Resume lookup now falls back to Redis when in-memory session cache misses,
  enabling reconnect after server restart.
- Moved Redis token consume lookup to an async worker queue in comm.c; the
  main loop remains responsive and only the requesting descriptor waits.
- Moved remaining resume Redis operations (store/drop/ttl-arm) to the same
  async worker queue, removing synchronous resume Redis writes from main-thread
  descriptor/game-loop paths.
- Generalized async worker payloads to typed auth jobs/results (Patch B
  foundation), with reserved OAuth verification job type scaffold.
- Added timeout handling for descriptor-scoped pending async auth requests to
  prevent indefinite pending state.

### Notes

- Current token transport is text control line:
  - Core.Resume {"event":"token","token":"...","ttl":180}
- Tokens are short-lived and rotated on successful resume.
- This is intentionally process-local for the first cut.

### Follow-up

- Implement OAuth verification jobs on top of typed async auth worker scaffold.
- Add explicit cancellation tokens/list for pending jobs if we need hard cancel
  semantics beyond descriptor-close/request-id invalidation.
