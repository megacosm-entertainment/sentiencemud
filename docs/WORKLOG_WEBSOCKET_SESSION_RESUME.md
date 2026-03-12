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

### Notes

- Current token transport is text control line:
  - Core.Resume {"event":"token","token":"...","ttl":180}
- Tokens are short-lived and rotated on successful resume.
- This is intentionally process-local for the first cut.

### Follow-up

- Add Redis-backed session storage for process restarts/failover.
- Store token hashes instead of plaintext.
- Bind session token to account/session context from web backend auth.
