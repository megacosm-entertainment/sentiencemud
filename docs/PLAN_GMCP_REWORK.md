# PLAN: GMCP Rework & Client Protocol Modernization

**Status:** Design Draft
**Date:** 2026-03-24
**Related Plans:** `PLAN_WEB_INFRASTRUCTURE.md`, `PLAN_CLIENT_AUTOMATION.md`

---

## Overview

The current GMCP implementation was inherited from DikuMUD/ROT convention and assumes a
traditional single-class, vnum-based MUD. Sentience has diverged significantly: widevnum
identifiers, a multi-class/job system, wilderness coordinates, and a separate WebSocket port
for a browser-based client. The existing GMCP code also has a separate (and incomplete)
websocket protocol layer that sends everything to a generic `Char.Status` package instead of
proper modules.

This plan covers three tightly coupled tracks:

1. **GMCP module redesign** — define Sentience-native packages that reflect actual game data
2. **MXP abstraction** — make `mxp_links.c` emit either MXP or structured GMCP link data
   depending on the connection type
3. **Client targets** — web client (GUI expansion) and Mudlet packages

These tracks share a data model. Design the data model first; the delivery mechanism and
client-side rendering follow from it.

---

## Current State

### GMCP
- 8 packages, 40 variables, all sent every update cycle (no dirty tracking)
- Packages mirror standard DikuMUD convention: `Char.Base`, `Char.Vitals`, `Char.Stats`,
  `Char.AC`, `Char.Worth`, `Char.Affect`, `Char.Enemies`, `Room.Info`
- `Room.Info.vnum` — sends a numeric vnum; widevnum rooms would need a different field
- `Char.Base.class` — correctly uses `class_display_ch()`, but sends only the current
  (primary) class as a plain string; multi-class data is not exposed
- WebSocket layer sends all variables through a generic `Char.Status` package (not the
  named packages above) — effectively a stub

### MXP Links
- Full abstraction layer in `mxp_links.c` — 11 functions, all take `descriptor_t *d`
- ~125 call sites across 26 files, all already going through this layer
- Functions gracefully degrade when MXP is not enabled (emit plain text)
- No equivalent path for WebSocket/web clients — web connections currently get no links

### Connection Types
```c
typedef enum {
    CONN_TYPE_TCP,              // Plain TCP (telnet)
    CONN_TYPE_TLS,              // TLS-encrypted TCP (Mudlet, etc.)
    CONN_TYPE_WEBSOCKET_TLS     // WebSocket over TLS (web client)
} connection_type_t;
```

WebSocket connections are easily identified via `d->conn->type == CONN_TYPE_WEBSOCKET_TLS`.

---

## Track 1: GMCP Module Redesign

### Design Principles

- **Sentience-native namespacing**: `Sentience.*` prefix for custom packages; keep
  `Core.*` for standard negotiation
- **Widevnum throughout**: wherever a vnum was sent, send the widevnum string
- **Dirty tracking**: only send packages when the underlying data has actually changed,
  not every update cycle
- **Multi-class aware**: expose all active classes/jobs, not just current primary
- **Explicit package versions**: include `"_v": 1` in each package payload so clients can
  handle future changes gracefully

### Proposed Package Map

#### Negotiation (unchanged from standard)
```
Core.Hello              Client → Server: {"client": "...", "version": "..."}
Core.Supports.Set/Add   Client → Server: package negotiation
```

#### Character Data

```
Sentience.Char.Identity
  name        string   Character name
  race        string   Race widevnum string (e.g. "races:human")
  race_name   string   Display name
  gender      string   "male" | "female" | "neutral"
  level       int      Character level
  tot_level   int      Total level across all classes
  classes     array    [{id, name, level, is_primary}] — full class list
  title       string   Character title

Sentience.Char.Vitals
  hp          int
  hp_max      int
  mana        int
  mana_max    int
  move        int
  move_max    int

Sentience.Char.Stats
  str, int, wis, dex, con    int (current)
  str_base ... con_base      int (permanent/base)
  hitroll     int
  damroll     int
  wimpy       int

Sentience.Char.Combat
  ac_pierce, ac_bash, ac_slash, ac_exotic  int

Sentience.Char.Worth
  alignment   int
  xp          int
  xp_tnl      int
  practices   int
  gold        int
  silver      int   (if applicable)

Sentience.Char.Affects
  affects     array  [{name, wnum, duration, flags[]}]

Sentience.Char.Enemies
  enemies     array  [{name, wnum, level, hp, hp_max, is_primary_target}]
```

#### Room/World

```
Sentience.Room.Info
  wnum        string   Room widevnum (e.g. "areas:limbo:1")
  name        string
  area_name   string
  area_wnum   string
  sector      string   Sector type name
  flags       array    Room flag names
  exits       object   {dir: {wnum, name, flags[]}}
  is_wilds    bool
  wilds_uid   int      (if is_wilds)
  wilds_x     int      (if is_wilds)
  wilds_y     int      (if is_wilds)

Sentience.Room.Contents
  items       array    [{wnum, instance_id, name, short_desc, flags[]}]
  npcs        array    [{wnum, instance_id, name, short_desc, flags[]}]
  players     array    [{name, level, race_name, class_name}]
```

#### Links (new — see Track 2)

```
Sentience.Link
  id          string   Unique link ID (referenced in text stream)
  text        string   Display text
  left        string   Left-click command (primary action)
  right       array    [{label, cmd}] — context menu items
  category    string   "obj" | "mob" | "room" | "player" | "help" | "cmd"
```

#### Channel Messages (new — see PLAN_WEB_CLIENT_UI.md)

```
Sentience.Channel.Message
  channel     string   Channel name/identifier
  sender      string   Character name (empty for system messages)
  text        string   Message content (ANSI stripped; web renders its own styling)
  timestamp   int      Unix timestamp
  tell_target string   (tells only) recipient name
```

Sent in addition to (or instead of) the inline text stream output when the client has
the chat panel active. Per-channel terminal suppression controlled by client preference
in `Sentience.Client.Layout`.

#### Authentication Events (new)

```
Sentience.Auth.QRCode
  purpose     string   "totp_setup" (currently; extensible)
  image       string   PNG image base64-encoded (data URL ready: "data:image/png;base64,...")
  expires_at  int      Unix timestamp when this QR code expires
  uri         string   The otpauth:// URI encoded in the QR (for copy/paste fallback)
```

Sent during MFA setup (`setup_mfa_for_char`) when the client is a web client (WebSocket
connection). The PNG is generated in memory (no temp file) and base64-encoded before
sending. For Mudlet or terminal clients, the existing ASCII terminal QR display
(`display_qr_code`) is used instead — no URL endpoint is exposed.

Implementation notes:
- `save_qr_code_as_png()` in `account/otp.c` currently writes to a file via libpng's
  file I/O. A companion function `encode_qr_code_as_png_base64()` should use libpng's
  custom write callback (`png_set_write_fn`) to write into a `malloc`'d memory buffer,
  then base64-encode the result.
- Standard base64 table, no line breaks in the output (web `<img>` src doesn't need them).
- Buffer can be freed after `SendGMCPRaw()` returns.
- A QR code at scale=6 is roughly 150×150px → ~67KB PNG → ~90KB base64. Well within
  a single GMCP packet.

#### Client Capabilities

```
Sentience.Client.Ready
  Server → Client on boot: announces available packages and features
  {"packages": [...], "features": ["links", "gui", "minimap"]}
```

### Dirty Tracking

Add a `gmcp_dirty_flags` bitmask to `DESCRIPTOR_DATA`. Each bit corresponds to a package.
Set the bit when underlying data changes; clear it after send. `gmcp_update()` only
serializes and sends packages with dirty bits set.

This replaces the current approach of sending all 40 variables on every cycle.

---

## Track 2: MXP Abstraction → Connection-Aware Link Layer

### Current Behavior

All 11 `mxp_*` functions in `mxp_links.c` check `isMXP(d)`:
- MXP-capable: emit `<send href="...">` tags
- Not capable: emit plain text (no clickability)

### Target Behavior

```
mxp_link(d, buf, text, command, hint)
  ├── CONN_TYPE_WEBSOCKET_TLS  → emit Sentience.Link GMCP + OSC 8 span
  ├── MXP capable             → emit <send href="..."> tags (unchanged)
  ├── OSC 8 capable           → emit OSC 8 hyperlink (Mudlet 4.18+, xterm, iTerm2, Kitty)
  └── Plain                   → emit plain text (unchanged)
```

Mudlet has supported OSC 8 hyperlinks since version 4.18 (2023). This means the abstraction
gives us three distinct output paths without any call-site changes:
- **MXP** — existing Mudlet users who have MXP negotiated (backwards-compatible)
- **OSC 8** — Mudlet 4.18+ without MXP, and other OSC 8-capable terminals; left-click
  fires the `left` command; `right[]` items surface as a Mudlet trigger/alias layer
- **WebSocket** — web client via Sentience.Link GMCP + OSC 8 annotation

For `mxp_link_multi` (already models a menu), the `right[]` array in `Sentience.Link`
maps directly to the items array.

### Implementation Approach

1. Add a helper `descriptor_is_websocket(d)` (may already exist in comm.c)
2. In each `mxp_*` function, add a websocket branch that:
   - Generates a short unique link ID (e.g. `lk_<counter>`)
   - Emits the link ID as a styled span in the text stream: `\e[link:lk_42]text\e[/link]`
     (or a custom ESC sequence the web client recognizes)
   - Queues a `Sentience.Link` GMCP message with the full action list
3. The web client matches span IDs to GMCP link data and renders context menus

### Inline vs. Out-of-Band

Two options for associating link data with text:

**Option A — Inline annotation** (simpler for web client):
Embed a lightweight tag directly in the text stream that the web client parses:
```
\x1b]8;;lk_42\x07sword\x1b]8;;\x07
```
(Terminal hyperlink escape sequence — OSC 8. Terminals that don't understand it ignore it.
Web client intercepts it and attaches the GMCP-provided action list.)

**Option B — GMCP span ID** (cleaner separation):
Send a marker in text (e.g., custom ESC sequence with the link ID), send the full action
data out-of-band via `Sentience.Link` GMCP. Web client correlates by ID.

**Recommendation: Option A** — OSC 8 is natively supported by Mudlet 4.18+, xterm,
iTerm2, and Kitty. Mudlet users on 4.18+ get clickable links without needing MXP.
Web client intercepts the same sequence. Fallback is graceful (older clients/terminals
that don't support OSC 8 show plain text). MXP remains for backwards compatibility with
older Mudlet versions and other MXP clients.

### No Changes Required at Call Sites

Because all MXP functions already take `descriptor_t *d`, the websocket branch is entirely
internal to `mxp_links.c`. The 125 call sites need no changes.

---

## Track 3: Client Targets

### Web Client (GUI Expansion)

The web client runs at `wss://` on the dedicated websocket port. Because it's a separate
connection type, it can receive richer data without affecting Mudlet/telnet clients.

The web client uses a **dockable panel layout** (Golden Layout or equivalent) giving players
the same moveable/resizable mini-window experience as Mudlet, with the added benefit that
layout preferences are persisted server-side and roam across devices. See
`PLAN_WEB_CLIENT_UI.md` for full details.

**Server-side layout persistence** requires a new GMCP message:
```
Sentience.Client.Layout  — bidirectional
  Client → Server: {"panels": <Golden Layout JSON state>}
  Server → Client: {"panels": <saved state>}   (sent on login)
```
Stored in character JSON under `"web_client_layout"`.

**Phase 1 — Functional parity:**
- Receive and render `Sentience.Char.Vitals` and `Sentience.Char.Stats` in a sidebar panel
- Render `Sentience.Room.Info` for a room description panel with exit buttons
- Handle `Sentience.Link` for clickable text and custom context menus (right-click / long-press)

**Phase 2 — GUI features:**
- Dockable panel layout with saved positions (`Sentience.Client.Layout`)
- Minimap using `Sentience.Room.Info` exit data (graph traversal)
- Combat panel using `Sentience.Char.Enemies`
- Effects/buffs panel using `Sentience.Char.Affects`
- `Sentience.Room.Contents` for clickable room inventory

**Phase 3 — Rich client:**
- Character sheet panel (full stats, classes)
- Skill/spell list (new package TBD: `Sentience.Char.Skills`)
- Quest tracker (new package TBD: `Sentience.Quest.Active`)

### Mudlet Package

A Mudlet package provides the same GUI elements for players using the desktop client.
It subscribes to the same `Sentience.*` GMCP packages.

**Mudlet receives GMCP identically to the web client** — same packages, same dirty
tracking, same link data. The Mudlet Lua package maps GMCP events to Mudlet UI elements
(gauges, labels, mapper).

**Sentience.Room.Info** is particularly important for Mudlet — the mapper needs widevnum
room identifiers to correctly track unique rooms. The current `Room.Info.vnum` field is
insufficient for widevnum rooms.

**Key Mudlet package modules:**
- `SentienceVitals` — HP/mana/move gauges
- `SentienceStats` — stat display
- `SentienceMap` — mapper integration using `Sentience.Room.Info`
- `SentienceCombat` — enemy tracking
- `SentienceLinks` — convert `Sentience.Link` GMCP to Mudlet right-click menus

---

## Migration Strategy

The existing `Char.*` and `Room.Info` packages must keep working for existing clients
(e.g., players already using Mudlet with standard GMCP scripts). The new `Sentience.*`
packages run alongside the old ones during a transition period.

**Approach:**
1. New `Sentience.*` packages added alongside existing `Char.*` / `Room.Info`
2. Clients that send `Core.Supports.Set ["Sentience 1"]` get the new packages
3. Clients that don't negotiate `Sentience` continue to receive legacy packages
4. Once the official Mudlet package is released, deprecate legacy packages with a sunset date
5. WebSocket connections always receive `Sentience.*` only (no legacy packages sent to web)

This avoids breaking any existing player setups during the transition.

---

## Implementation Phases

### Phase 1 — Foundation (prerequisite for everything)
- [ ] Define `Sentience.*` package enum values and variable table in `protocol.h` / `protocol.c`
- [ ] Add `gmcp_dirty_flags` to `DESCRIPTOR_DATA`
- [ ] Add `Sentience.Char.Identity` (includes multi-class array)
- [ ] Add `Sentience.Char.Vitals`, `Sentience.Char.Stats`, `Sentience.Char.Combat`, `Sentience.Char.Worth`
- [ ] Add `Sentience.Room.Info` with widevnum fields
- [ ] Route new packages through `gmcp_update()` with dirty tracking
- [ ] WebSocket layer sends `Sentience.*` instead of generic `Char.Status`
- [ ] Tests: unit tests for GMCP serialization, dirty flag behavior

### Phase 2 — Links
- [ ] Add `Sentience.Link` GMCP package
- [ ] Add websocket branch to all `mxp_links.c` functions
- [ ] Add OSC 8 branch to `mxp_links.c` (for Mudlet 4.18+ and other OSC 8 terminals)
- [ ] Web client: intercept link annotations, attach context menus
- [ ] Tests: verify link data roundtrip for each mxp_* function type

### Phase 3 — Extended packages
- [ ] `Sentience.Room.Contents` (items, npcs, players in room)
- [ ] `Sentience.Char.Affects` with wnum field
- [ ] `Sentience.Char.Enemies` with wnum field
- [ ] `Sentience.Client.Ready` on connection
- [ ] Tests: integration tests for extended packages

### Phase 4 — Mudlet Package
- [ ] Mudlet package skeleton (XML bundle)
- [ ] `SentienceVitals` gauges
- [ ] `SentienceMap` mapper integration
- [ ] `SentienceCombat` enemy panel
- [ ] `SentienceLinks` right-click menu handler
- [ ] Package release process / update mechanism

---

## Track 3 note: Web Client Panel Layout

The web client UI design (dockable panels, layout persistence via `Sentience.Client.Layout`
GMCP, Golden Layout library recommendation) is detailed in `PLAN_WEB_CLIENT_UI.md`.

The GMCP side of layout persistence is:
- `Sentience.Client.Layout` — server stores and restores panel layout per character
- `Sentience.Client.Ready` — announces available packages/features on connect

---

## Open Questions

1. **OSC 8 capability detection**: Mudlet 4.18+ supports OSC 8 natively. The question is
   how to detect it server-side. Options: (a) client sends a `Sentience.Client.Caps` GMCP
   message on connect declaring OSC 8 support, (b) server infers from Mudlet version in
   `Core.Hello`, (c) always send OSC 8 to non-MXP Telnet/TLS connections (graceful fallback
   for those that don't support it — plain text shows through).
   **Preferred: option (c)** — emit OSC 8 on all non-MXP non-WebSocket connections; clients
   that don't support it simply ignore the escape sequences.

2. **`Sentience.Room.Contents` update frequency**: Room contents change on every enter/leave
   event. Should this be event-driven (send on room change only) or pulse-driven? Event-driven
   is cleaner but requires hooking into more code paths.

3. **Backward compatibility window**: How long to keep legacy `Char.*` / `Room.Info` packages
   alive alongside `Sentience.*`? Suggest: until the official Mudlet package ships + 90 days.

4. **GMCP for NPC/object wnum fields**: `Sentience.Char.Enemies` and `Sentience.Room.Contents`
   need widevnum strings for mobs/objects. Do we expose these to all players, or only staff
   (since wnums expose internal structure)? Probably fine for players — the wnum is just an ID.

5. **`Sentience.Char.Skills`**: A skills/spells package would enable the web client to render
   a skill list. This is a separate, larger effort — flag as Phase 5 or later.

6. **Mudlet package distribution**: Via the Mudlet package registry, or self-hosted download?
   Self-hosted gives us control over versioning aligned with server updates.

7. **Touch/mobile support in web client**: Long-press as right-click equivalent for
   `Sentience.Link` context menus. Should be designed in from Phase 2, not retrofitted.

---

## See Also

- `mxp_links.c` / `mxp_links.h` — current MXP abstraction layer
- `protocol.c` — GMCP variable table, UpdateGMCPString/Number, WriteGMCP
- `update.c` — `gmcp_update()` function
- `connection.h` — `connection_type_t` enum
- `PLAN_WEB_INFRASTRUCTURE.md` — web client, Payload CMS, auth architecture
- `PLAN_CLIENT_AUTOMATION.md` — server-side automation (triggers, timers, aliases)
