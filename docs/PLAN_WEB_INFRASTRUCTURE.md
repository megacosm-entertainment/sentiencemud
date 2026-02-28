# PLAN: Web Infrastructure Replacement

**Status:** Planning
**Date:** 2026-02-28
**Goal:** Replace WordPress + phpBB with a unified TypeScript/Node stack that handles authentication, a forum, game information/announcements, and a web MUD client.

---

## Background and Motivation

- Current site: WordPress (content) + phpBB (forum) — separate from MUD auth
- MUD accounts are JSON files in `/sentience/accounts/` (1 account → up to 10 characters)
- Immediate pain point: players must re-authenticate every time the MUD is restarted after an update push
- Game is currently dormant — relaunch is the target, so migration can be done cleanly without disrupting active players
- ~20 years of phpBB history worth preserving

---

## Requirements

1. **Authentication** — unified login across web and MUD
2. **Forum** — community discussion, preserve historical phpBB data
3. **Announcements / Game Info** — staff-editable content without code deployments
4. **Web MUD Client** — full React game client over WSS; multi-panel UI driven by MXP and out-of-band JSON the game already emits; not a browser-wrapped terminal emulator
5. **Seamless reconnect** — web client auto-reconnects after MUD restart, no password prompt

---

## Current MUD Account Structure

Representative JSON account file (`/sentience/accounts/<letter>/<name>`):

```
metadata:    format_version, account_id [timestamp, seq], created, last_saved, last_login
account:     username, password (argon2id v3), mfa_enabled/mfa_key, email/email_verified,
             account_flags, staff_account, character_count/limit, last_login_host
characters:  array of summaries — name, level, tot_level, race, class, last_area,
             staff/staff_rank, deleted, timestamps, id [int, seq]
penalties:   type, scope, reason, applied_by, applied_at, expires_at, target_name
unlocked_races: array of strings
```

### Key migration notes

- **Passwords are argon2id** — migrate directly to Postgres, no forced resets needed
- **MFA already implemented** — TOTP key stored in account, migrates as-is
- **Character data** (full stats, inventory, etc.) stays in JSON — game engine owns it
- **Character summaries** are embedded in the account file — sufficient for web display
- `"(null)"` recovery codes need to be cleaned to NULL during migration
- `account_id: [timestamp, seq]` → keep as reference columns, generate UUID as primary key
- `acct_flags_numeric` is redundant with `account_flags` array — consolidate during migration

---

## Technology Stack

### Core Framework
**Next.js (App Router) + TypeScript**
- Natural fit given team's TypeScript/Node background
- Full-stack: API routes + SSR/SSG for content pages
- Large ecosystem, strong TS support

### Database
**PostgreSQL + Prisma**
- Prisma is TypeScript-first, excellent DX, handles migrations cleanly
- Handles: web accounts, character summaries, announcements, forum data (if built-in)
- Jansson-style JSON column support available if needed for structured blobs

### Authentication
See Identity Provider section below.

### Web MUD Client
A full React-based game client, not a browser-wrapped telnet terminal. The client lives as
a protected route in the Next.js app and connects over WSS (already supported by the game).

The game already emits two structured data streams alongside the text:
- **MXP** — clickable links, send-on-click actions, element tagging in the text stream
- **Out-of-band JSON** — structured game state (stats, inventory, map data, etc.)

These drive a proper multi-panel UI:

```
┌─────────────────────────────────────────────┬──────────────────────┐
│                                             │  HP / MP / MV bars   │
│           Main text stream                 │  Status / effects    │
│         (ANSI, MXP inline)                 ├──────────────────────┤
│                                             │  Minimap / compass   │
│                                             ├──────────────────────┤
│                                             │  Inventory / gear    │
├─────────────────────────────────────────────┼──────────────────────┤
│  Input bar + command history                │  Hotbar / actions    │
└─────────────────────────────────────────────┴──────────────────────┘
```

**Stream architecture:**

```
MUD (WSS)
  └── protocol parser
        ├── raw ANSI text  →  xterm.js pane  (narrative, combat output)
        ├── MXP tags       →  React state    (clickable links, send-on-click)
        └── out-of-band JSON → React state   (stats, map, inventory panels)
```

**xterm.js** handles only the ANSI text pane — it's good at rendering large volumes of
colored text with scrollback. All other UI is React components fed by the JSON stream.

**MXP** provides interactivity within the text stream: clickable room exits, item names
that trigger examine/get, mob names that target attacks, etc. The parser strips MXP tags
before they reach xterm.js and converts them to overlaid React click handlers.

**Out-of-band JSON** drives everything outside the text pane. Since the game already
sends this, the client work is primarily React layout + the protocol parser. No MUD-side
changes needed for the initial client.

**Possible panels (driven by what the game already emits):**
- HP / MP / MV with color-coded bars
- Status effects and buffs with timers
- Minimap (if the game sends room/exit data)
- Inventory and equipped gear
- Hotbar of frequently used commands
- Combat log (separate scroll pane from main text)

---

## Content Management

### Option A: Built-in (Recommended for simple needs)
Announcements and game info as database-backed pages with a Next.js admin UI.
- Pro: one stack, unified auth, no extra services
- Pro: game info pages (races, classes, lore) change with code — MDX files committed to repo work well
- Con: need to build the admin UI
- **Best when:** content editors are comfortable with a simple custom interface

### Option B: Payload CMS (Recommended if non-dev staff edit content)
TypeScript-first, code-first config, self-hosted, PostgreSQL-native, Next.js plugin (v3).
- Pro: admin panel out of the box, typed schemas, REST + GraphQL auto-generated
- Pro: fits the stack perfectly, saves building admin UI
- Pro: auth can be extended to serve as identity provider
- Con: opinionated, adds complexity, auth integration with MUD token flow needs care
- **Best when:** staff need to edit content without developer involvement

### Option C: Headless CMS (Contentful, Sanity)
SaaS-managed content with API delivery.
- Pro: excellent editing UX
- Con: another external dependency, costs money at scale, overkill for a MUD site
- **Verdict:** Not recommended for this use case

---

## Forum

### Option A: Build in Next.js
Threaded forum directly in the Next.js app with PostgreSQL.
- Pro: unified auth, one codebase, full control
- Pro: MUD community scale is low enough that requirements are simple
- Con: 1-2 weeks of real work; phpBB importer is custom work
- **Best when:** starting fresh, historical data treated as archive

### Option B: NodeBB (Recommended for preserving phpBB data)
Node.js-based forum with a phpBB importer.
- Pro: phpBB importer exists — least risky preservation of 20-year history
- Pro: Node.js fits the stack, has REST API
- Pro: native Redis support (shared with MUD and web)
- Con: separate service, SSO bridging needed
- Con: primarily MongoDB (PostgreSQL plugin exists but less mature)
- **Verdict:** Best path if historical data preservation matters

### Option C: Discourse
- Con: Ruby/Postgres, heavy, complex SSO setup
- **Verdict:** Not recommended

### phpBB historical data strategy
Since the game is dormant, keep phpBB running as a **read-only archive** at a subdomain (e.g., `archive.domain.com`) while building the new forum fresh. NodeBB import handles the data; returning users link accounts by email on first login.

---

## Identity Provider / Authentication Architecture

The core challenge: Payload CMS, NodeBB, and the MUD all have separate auth systems. Three approaches to unification:

### Option A: Build OAuth2 in Payload
Implement OAuth2/OIDC endpoints in Payload (~100-150 lines using `oauth2orize`).
NodeBB SSOs against Payload. MUD uses Redis token bridge.
- Pro: one less service
- Con: hand-rolled auth infrastructure, more maintenance
- **Best when:** minimizing services is the priority

### Option B: Dedicated Identity Provider (Recommended)
A standalone OIDC provider serves as the single source of truth.
All services (Payload, NodeBB, MUD) become clients or resource servers.

| Provider   | Language | Est. RAM    | Notes |
|------------|----------|-------------|-------|
| Keycloak   | Java     | 512MB–1GB   | Most mature, battle-tested, heaviest |
| Authentik  | Python   | 300–500MB   | Modern UI, growing community |
| **Logto**  | Node.js  | ~256–512MB  | Best DX for TS stack, youngest |
| Zitadel    | Go       | ~100–200MB  | Lightest, strong OIDC compliance |

**Logto** is the best fit for the team's TypeScript background and has first-class Next.js integration. Official docs recommend 2 vCPU / 8 GiB for the full server environment; the Node.js process itself is likely 256–512MB in practice.

**Keycloak** has the most production war stories if operational maturity matters more than DX.

**Zitadel** is worth considering if RAM efficiency is a priority.

### MUD as JWT Resource Server
With a dedicated IdP, the MUD validates JWTs directly using libraries it already has:
- **OpenSSL** (already present for TLS) — RSA signature verification
- **Jansson** (already present for JSON) — claims parsing
- Fetch JWKS public keys from IdP on startup, cache them, validate tokens on connect
- No new library dependencies required

---

## Auth Token Bridge (Seamless Reconnect)

Regardless of IdP choice, Redis handles the "Play Now" → MUD connection flow:

```
1. Player clicks "Play Now" on the website
2. Web app generates a short-lived token (UUID), stores token→account_id
   in Redis with 60-second TTL
3. Web client opens WSS connection, sends token as first message
4. MUD looks up token in Redis, gets account, logs player in, deletes token
5. On MUD restart: web client detects disconnect, requests new token,
   reconnects transparently — player sees a brief disconnect only
```

This solves the re-authentication pain point. Redis is already present in the MUD stack.

---

## Container Architecture

### Services

```
nginx (443/80)
  ├── /*         → payload:3000    (Next.js app + admin + MUD client page)
  ├── /forum/*   → nodebb:4567
  ├── /auth/*    → logto:3001      (or keycloak/zitadel, if using dedicated IdP)
  └── /ws        → mud:4000        (WebSocket upgrade / proxy_pass)

payload:3000   → postgres:5432, redis:6379
nodebb:4567    → mongodb:27017, redis:6379
mud            → redis:6379, /sentience volume
logto:3001     → postgres:5432 (shares Postgres, separate database/schema)

postgres:5432  (accounts, Payload content, optionally Logto)
mongodb:27017  (NodeBB forum data — phpBB import lands here)
redis:6379     (shared: auth tokens, NodeBB cache, MUD cache)
```

### Notes
- nginx handles TLS termination for all services and WebSocket upgrade for the MUD
- Redis is shared across all three application services (already in MUD stack)
- NodeBB uses MongoDB because the PostgreSQL plugin is less battle-tested and the phpBB importer assumes MongoDB
- The `/sentience` data volume is owned by the MUD container; web app does not mount it directly

---

## Data Ownership

| Data | Owner | Storage | Notes |
|------|-------|---------|-------|
| Web accounts / auth | Web stack | PostgreSQL | Migrated from JSON via script |
| Character summaries | PostgreSQL | PostgreSQL | Synced from game on login/logoff |
| Full character data | MUD | JSON files | Game engine owns, web does not touch |
| Zone / area data | MUD | JSON files | No web access needed |
| Announcements / pages | Payload | PostgreSQL | Staff-editable via admin UI |
| Forum posts | NodeBB | MongoDB | phpBB data imported here |
| Session tokens | Redis | Redis | Short TTL, ephemeral |

### Phased transition

**Phase 1 (relaunch):**
- Postgres for web auth (accounts migrated from JSON via one-time script)
- MUD still reads JSON account files for game data
- Redis token bridge for seamless web client reconnect
- Character summaries in Postgres (populated during migration, updated on play)

**Phase 2 (MUD update):**
- MUD reads auth from Postgres via libpq (replaces JSON account file reads)
- MUD validates IdP-issued JWTs directly on WSS connect
- JSON account files become redundant backup
- `load_account()` swapped to Postgres query; rest of MUD unchanged

---

## Account Migration Script

One-time Node.js script (~150 lines):
1. Walk `accounts/<letter>/<name>` files
2. Parse JSON, clean `"(null)"` recovery codes
3. Insert into Postgres `accounts` table (generate UUID primary key, keep `mud_account_id` as reference)
4. Insert character summaries into `characters` table
5. Insert penalties into `penalties` table
6. Flag accounts for email-based password claim on first web login

Argon2id hashes copy directly — no rehashing, no forced password resets.

---

## Recommended Direction (Summary)

- **Framework:** Next.js (App Router) + TypeScript + Prisma
- **CMS:** Payload CMS v3 (Next.js plugin, TypeScript-native, PostgreSQL, built-in admin UI)
- **Forum:** NodeBB (phpBB importer, Node.js, separate MongoDB)
- **Identity Provider:** Logto (best DX for TypeScript stack) or Zitadel (if RAM matters)
- **Web MUD Client:** xterm.js, protected route in Payload/Next.js app
- **Auth bridge:** Redis token (already in MUD stack)
- **Historical phpBB data:** NodeBB importer + read-only phpBB archive subdomain

---

## Open Questions

- [ ] Does Payload CMS v3's auth system work cleanly as an OIDC client to Logto/Zitadel?
- [ ] NodeBB OAuth2 SSO plugin — test against chosen IdP before committing
- [ ] Logto vs Zitadel: evaluate based on actual container RAM on the dedicated server
- [ ] libpq integration in MUD for Phase 2 — assess scope against existing JSON load/save patterns
- [ ] Staff account mapping: `staff_rank` in MUD → roles/permissions in Payload and NodeBB
- [ ] Audit the full set of out-of-band JSON event types the game currently emits — this defines what panels the client can build on day one without MUD changes
- [ ] MXP coverage audit — which elements are currently sent? Links, room exits, entity names? What would need to be added for hotbar / action affordances?
- [ ] Protocol parser: build as a standalone JS/TS library (testable, potentially reusable for a native client later) or inline in the Next.js app?
