# WebSocket Session Resume Plan

## Goal

Allow browser users to refresh the page without losing their in-game session.

## Scope

Phase 1 in this plan covers in-memory, single-process session resume.

- No database persistence for resume sessions yet.
- No cross-process failover yet.
- Resume token TTL is short (minutes), intended for quick browser refresh recovery.

## Current Constraints

- Existing reconnect logic already supports linkdead characters.
- Browser refresh drops the socket and creates a fresh WebSocket connection.
- Login flow currently starts at account-name prompt.

## Phase 1 Design

### Server-side resume state

Maintain a short-lived in-memory map:

- token -> player id (id[0], id[1]), expires_at

Rules:

- One active resume token per player.
- New issue rotates/replaces previous token.
- Token is one-time on successful resume attempt.
- Expired tokens are purged opportunistically.
- For actively connected WebSocket players, token expiry is refreshed/retained;
  TTL effectively starts when the socket disconnects.

### Token issuance

Issue a resume token when a WebSocket descriptor enters/returns to play state.

Transport to client:

- GMCP-style control message in WebSocket stream:
  - Core.Resume {"event":"token","token":"...","ttl":180}
- Explicit resume ACK control messages:
  - Core.Resume {"event":"ok"}
  - Core.Resume {"event":"fail","reason":"invalid_or_expired"}

### Resume handshake

From account prompt, client can send:

- RESUME <token>

Server behavior:

- Validate token exists and not expired.
- Resolve player by stored id.
- Require player to be linkdead (desc == NULL) and currently valid in-world state.
- Reattach descriptor and enter playing state.
- Rotate and re-issue token.

## Security and Risk Notes

- Token currently stored in memory as plaintext for Phase 1 simplicity.
- TTL is short and token is one-use on success.
- Future phases should hash stored tokens and move to Redis.

## Phase 2 (Planned)

- Redis-backed resume sessions.
- Token hash storage.
- Session metadata: account id, client fingerprint hash, issue ip, last_seen.
- Explicit invalidation on logout/quit.

## Phase 3 (Planned)

- OAuth-backed game session minting with backend mediation.
- Resume token bound to backend-authenticated account session.
- Multi-server shared session state.

## WebSocket Client Requirements

The browser client should:

1. Parse incoming control lines and capture resume token.
2. Store token in localStorage (or sessionStorage if preferred).
3. On socket open, if token exists, send RESUME <token> first.
4. If resume fails, fall back to normal login UX.
5. On receiving a newer Core.Resume token event, replace stored token.
6. Clear token on explicit logout/quit flow.

## Wire Examples

Server -> client:

Core.Resume {"event":"token","token":"d6v4Q6F6I4Qh0wHegLJ6v0m6XWf3D0w4qA6E8nH3","ttl":180}
Core.Resume {"event":"ok"}
Core.Resume {"event":"fail","reason":"invalid_or_expired"}

Client -> server (first message after connect):

RESUME d6v4Q6F6I4Qh0wHegLJ6v0m6XWf3D0w4qA6E8nH3
