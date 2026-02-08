# Multi‑Server Architecture & Migration Plan

Purpose
- Provide a clear, actionable plan to support:
  - Global accounts (single login point)
  - Multiple independent game servers (server‑scoped characters and storage)
  - Minimal disruption to existing telnet clients and current codebase conventions

Context
- Reference: project coding & contribution guidelines in [sentience/.github/copilot-instructions.md](sentience/.github/copilot-instructions.md).
- Keep all changes scoped to `src/` when implementing; update build files (CMake + Make) when adding/removing `.c`/`.h`.

Goals
- Preserve current account model as the global identity.
- Move characters and per‑character persistent storage (vaults/lockers) to server scope.
- Keep telnet UX unchanged for players (no required client changes).
- Support a staged, reversible migration with feature flags and backups.

High-level architecture
- Auth / Lobby Service
  - Central service for credentials, MFA, and server selection.
  - Issues short‑lived session tokens and exposes token verification API.
- Telnet Gateway (recommended)
  - Accepts telnet connections, performs login flow, proxies long‑lived telnet session to a chosen game server, or performs a trusted handshake.
  - Must correctly handle Telnet negotiation and MCCP.
- Game Servers
  - Verify tokens or accept gateway handshake, then proceed as currently implemented with minimal `nanny` changes to support token-based auth.
- Redis
  - Used for session tokens, character locks, server metadata, and pub/sub events.

Data model changes (backward‑compatible approach)
- Accounts: remain global JSON files with minimal structural change.
- Characters:
  - Add `server_id` to `ACCOUNT_CHARACTER`.
  - Default missing `server_id` to configured `default_server` during migration/read.
- Vaults:
  - Replace `vault` with `vaults` map keyed by `server_id`.
  - If `vaults` missing, treat legacy `vault` as `vaults[default_server]`.
- JSON compatibility:
  - Read both legacy and new formats; write new format once migration flag is enabled.

Key code touchpoints (implement in `src/`)
- `json_account.c` — read/write `characters[].server` + `vaults` map; compatibility logic.
- `json_char.c` — ensure character save/load honors `server_id`.
- `nanny.c` — show account menu filtered by `server_id`; accept gateway handshake / token validation.
- `account/auth.c` — token verification paths; reuse existing verify/password/MFA logic.
- `storage.c`, `mail.c`, `trade.c` — enforce server boundaries on persistent transfers.
- `redis_cache.c` — session & lock helpers (SET NX + TTL, atomic Lua scripts).
- `json_game_settings.c` — add server‑specific configuration loading and merge.

Configuration & precedence
- File layout:
  - Global: `DATA_DIR/system/game_settings.json`
  - Server overrides: `DATA_DIR/system/servers/<server_id>.json`
  - Optional registry: `DATA_DIR/system/servers/registry.json`
- Precedence (low → high): built-in defaults → global JSON → server JSON → env vars (`SENTIENCE_*` / `SENTIENCE_SERVER`) → runtime flags/admin.
- Each server process must be started with a `server_id` (env or CLI) so it loads its server JSON override.

Session & locking (Redis patterns)
- Keys:
  - `session:{token}` → JSON { account_id, server_id, expiry } (TTL short)
  - `charlock:{server_id}:{character_id}` → owner token (SET NX + TTL)
  - `account:server:meta:{server_id}` → metadata (capacity/status)
- Acquire locks atomically; refresh TTL while online; recover stale locks with TTL watchdog.
- Require char lock prior to finalizing login.

Migration & rollout (staged)
- Phase 0 — backup: snapshot `accounts/`, `area/`, and `data/`.
- Phase 1 — compatibility code:
  - Support both formats; add `server_scoped_vaults` feature flag (default off).
- Phase 2 — migration tool (dry‑run first):
  - Convert `vault` → `vaults[default_server]`
  - Assign `server` on characters lacking it (operator mapping optional)
- Phase 3 — pilot: enable flag for staging/pilot server; run integration tests.
- Phase 4 — global enable: flip flag, monitor, then prune legacy fields after safe interval.

Security & operational notes
- TLS for internal service calls (gateway↔auth, gateway↔server where possible).
- Short TTLs for tokens; strict validation.
- Audit and log all cross‑server transfer attempts; deny by default.
- Per‑server backups and retention for vaults/storage to limit blast radius.
- Rate limiting on auth endpoints; monitor for abuse.

Testing matrix
- Unit tests: JSON read/write, migration script dry‑run.
- Integration tests: gateway login → proxy → backend; char lock races; vault isolation and mail/trade denial.
- Manual tests: telnet negotiation, MCCP, reconnect behavior.

Developer & build notes (follow repository conventions)
- Implement changes under `src/` and update `CMakeLists.txt` and `Makefile` when adding/removing files.
- Add new services (gateway/auth) as separate projects or small services; prefer containerized builds; provide `docker-compose` dev stack for local testing.
- Keep documentation and migration scripts in `docs/` and `tools/`.

Next steps (pick one)
- Implement the compatibility layer for `json_account.c` & `json_char.c` and include unit tests.
- Write the migration script (Python) with dry‑run and mapping options.
- Scaffold a minimal Telnet Gateway prototype + `docker-compose` (Auth + Redis + gateway + `sent`) for dev testing.
- Add server JSON support in `json_game_settings.c` and startup handling for `server_id`.

Notes
- All design and implementation actions should honor the guidelines in [sentience/.github/copilot-instructions.md](sentience/.github/copilot-instructions.md) regarding file placement, build system synchronization, and avoiding generated files outside `.build`.