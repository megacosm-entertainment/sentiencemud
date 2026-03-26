# WebSocket Session Resume Plan

## Goal

Allow browser users to refresh the page without losing their in-game session.

## Scope

Phase 1 in this plan covers in-memory, single-process session resume.

- No database persistence for resume sessions yet.
- No cross-process failover yet.
- Resume token TTL is short (minutes), intended for quick browser refresh recovery.

Current implementation now includes Redis-backed persistence in addition to in-memory fast path,
so resume survives process restart/reboot as long as Redis is available.
Redis token consume lookup is handled asynchronously via a worker thread so the
main game loop does not block; only the requesting descriptor is marked pending.
Redis store/drop/ttl-arm operations are also queued to the same worker thread,
removing remaining synchronous resume Redis calls from the main loop path.

## Current Constraints

- Existing reconnect logic already supports linkdead characters.
- Browser refresh drops the socket and creates a fresh WebSocket connection.
- Login flow currently starts at account-name prompt.

## Phase 1 Design

### Server-side resume state

Maintain a short-lived in-memory map:

- token -> player id (id[0], id[1]), expires_at

Maintain mirrored Redis indexes (hash-only token storage):

- ws:resume:player:<id0>:<id1> -> <token_hash>
- ws:resume:token:<token_hash> -> "<id0>:<id1>"

Rules:

- One active resume token per player.
- New issue rotates/replaces previous token.
- Token is one-time on successful resume attempt.
- Expired tokens are purged opportunistically.
- For actively connected WebSocket players, token expiry is refreshed/retained;
  TTL effectively starts when the socket disconnects.
- Server stores only a hash of the token, never the plaintext token.
- Redis also stores only token hashes (never plaintext tokens).

### Token issuance

Issue a resume token when a WebSocket descriptor enters/returns to play state.

Transport to client:

- GMCP-style control message in WebSocket stream:
  - Sentience.Auth.Resume {"event":"token","token":"...","ttl":180}
- Explicit resume ACK control messages:
  - Sentience.Auth.Resume {"event":"ok"}
  - Sentience.Auth.Resume {"event":"fail","reason":"invalid_or_expired"}

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

- Server stores only SHA-256 token hashes in memory and Redis.
- TTL is short and token is one-use on success.
- Pending async auth requests are bounded by a timeout to avoid indefinite waits.

## Phase 2 (Planned)

- Redis-backed resume sessions.
- Token hash storage.
- Session metadata: account id, client fingerprint hash, issue ip, last_seen.
- Explicit invalidation on logout/quit.

## Phase 3 (Planned)

- OAuth-backed game session minting with backend mediation.
- Resume token bound to backend-authenticated account session.
- Multi-server shared session state.

## Patch B Foundation (Implemented)

- Async auth worker now uses typed jobs/results instead of resume-only payloads.
- Added reserved OAuth job type scaffold for future backend token verification.
- Added pending-auth timeout/cancel plumbing for descriptor-scoped async auth.

## WebSocket Client Requirements

The browser client should:

1. Parse incoming control lines and capture resume token.
2. Store token in localStorage (or sessionStorage if preferred).
3. On socket open, if token exists, send RESUME <token> first.
4. If resume fails, fall back to normal login UX.
5. On receiving a newer Sentience.Auth.Resume token event, replace stored token.
6. Clear token on explicit logout/quit flow.

## Wire Examples

Server -> client:

Sentience.Auth.Resume {"event":"token","token":"d6v4Q6F6I4Qh0wHegLJ6v0m6XWf3D0w4qA6E8nH3","ttl":180}
Sentience.Auth.Resume {"event":"ok"}
Sentience.Auth.Resume {"event":"fail","reason":"invalid_or_expired"}

Client -> server (first message after connect):

RESUME d6v4Q6F6I4Qh0wHegLJ6v0m6XWf3D0w4qA6E8nH3

## Migration Note: Core.Resume → Sentience.Auth.Resume

The GMCP package was renamed from `Core.Resume` to `Sentience.Auth.Resume` to respect
the convention that `Core.*` is reserved for standard GMCP negotiation (`Core.Hello`,
`Core.Supports.*`). Since resume is a Sentience-specific extension, it belongs in the
`Sentience.Auth.*` namespace alongside `Sentience.Auth.QRCode`.

**Code changes required:**
- `comm.c`: ~4 `SendGMCPRaw()` calls referencing `"Core.Resume"` → `"Sentience.Auth.Resume"`
- `nanny.c`: ~1 `SendGMCPRaw()` call referencing `"Core.Resume"` → `"Sentience.Auth.Resume"`

**Client impact:** The `RESUME <token>` command (client → server) is unchanged. Only the
server → client GMCP package name changes. Existing web clients will need to update their
GMCP message parsing to look for `Sentience.Auth.Resume` instead of `Core.Resume`.
