# Sentience GMCP Web Client Reference

> Comprehensive reference for implementing a web client that communicates with the
> Sentience MUD server via GMCP over WebSocket.

## Table of Contents

1. [Connection & Negotiation](#connection--negotiation)
2. [Authentication & Session Resume](#authentication--session-resume)
3. [Package Reference](#package-reference)
   - [Sentience.Client.Ready.Capabilities](#sentienceclientreadycapabilities)
   - [Sentience.Client.Ready.State](#sentienceclientreadystate)
   - [Sentience.Client.Preferences](#sentienceclientpreferences)
   - [Sentience.Client.Layout](#sentienceclientlayout)
   - [Sentience.Char.Identity](#sentiencecharidentity)
   - [Sentience.Char.Vitals](#sentiencecharvitals)
   - [Sentience.Char.Stats](#sentiencecharstats)
   - [Sentience.Char.Combat](#sentiencecharcombat)
   - [Sentience.Char.Worth](#sentiencecharworth)
   - [Sentience.Char.Affects](#sentiencecharaffects)
   - [Sentience.Char.Enemies](#sentiencecharenemies)
   - [Sentience.Char.Inventory](#sentiencecharinventory)
   - [Sentience.Char.Equipment](#sentiencecharequipment)
   - [Sentience.Char.Abilities](#sentiencecharabilities)
   - [Sentience.Char.Reputations](#sentiencecharreputations)
   - [Sentience.Char.Church](#sentiencecharchurch)
   - [Sentience.Char.Race](#sentiencecharrace)
   - [Sentience.Room.Info](#sentienceroominfo)
   - [Sentience.Room.Contents](#sentienceroomcontents)
   - [Sentience.Room.Map](#sentienceroommap)
   - [Sentience.Channel.Message](#sentiencechannelmessage)
   - [Sentience.Link.List](#sentiencelinklklist)
   - [Sentience.Editor — Overview](#sentienceeditor--overview)
   - [Sentience.Editor.State](#sentienceeditorstate)
   - [Sentience.Editor.Field](#sentienceeditorfield)
   - [Sentience.Editor.Close](#sentienceeditorclose)
   - [Sentience.Editor.Error](#sentienceeditorerror)
   - [Sentience.Editor.Set](#sentienceeditorset)
   - [Sentience.Editor.Commit](#sentienceeditorcommit)
   - [Sentience.Editor.Revert](#sentienceeditorrevert)
   - [Sentience.Editor.Request](#sentienceeditorrequest)
   - [Sentience.Editor.CommitResult](#sentienceeditorcommitresult)
   - [Sentience.Editor.StringEdit.Open](#sentienceeditorstringeditopen)
   - [Sentience.Editor.StringEdit.Save](#sentienceeditorstringeditsave)
   - [Sentience.Editor.StringEdit.Cancel](#sentienceeditorstringeditcancel)
   - [Sentience.Editor.StringEdit.Close](#sentienceeditorstringeditclose)
   - [Sentience.Editor.Draft.Save](#sentienceeditordraftsave)
   - [Sentience.Editor.Draft.Load](#sentienceeditordraftload)
   - [Editor Client Implementation Guide](#editor-client-implementation-guide)
   - [Sentience.Auth.Resume](#sentienceauthresume)
   - [Sentience.Auth.QRCode](#sentienceauthqrcode)
4. [Update Lifecycle](#update-lifecycle)
5. [Color Code Reference](#color-code-reference)
6. [Client Implementation Notes](#client-implementation-notes)

---

## Connection & Negotiation

### WebSocket Endpoint

Connect via secure WebSocket (wss://) to the server's WebSocket port.

### Auto-Enable Behavior

**WebSocket clients automatically receive all Sentience GMCP packages.** Unlike
telnet clients which must negotiate GMCP support via `Core.Supports.Set`, WebSocket
connections have GMCP and `Sentience.*` support enabled immediately upon handshake
completion.

### Negotiation Flow

```
Client                              Server
  |                                    |
  |  ── WebSocket Upgrade ──────────>  |
  |  <── 101 Switching Protocols ────  |
  |                                    |
  |  <── Core.Hello ────────────────   |  Server identifies itself
  |  <── Client.Ready.Capabilities ──  |  Server lists all supported packages
  |                                    |
  |  ── Core.Hello ─────────────────>  |  Client identifies itself (optional)
  |  ── Core.Supports.Set ──────────>  |  Client declares support (optional*)
  |                                    |
  |  (Login prompt appears as text)    |
  |                                    |
  |  ── Account name ───────────────>  |  Plain text input
  |  ── Password ───────────────────>  |  Plain text input
  |                                    |
  |  <── Client.Ready.State ─────────  |  Timing constants (once, after login)
  |  <── Client.Preferences ─────────  |  Current preference state (once)
  |  <── Client.Layout (restore) ────  |  Active layout (WebSocket only, once)
  |  <── Char.Identity ──────────────  |  Full character data burst
  |  <── Char.Vitals ────────────────  |
  |  <── Char.Stats ─────────────────  |
  |  <── Char.Combat ────────────────  |
  |  <── Char.Worth ─────────────────  |
  |  <── Room.Info ──────────────────  |
  |  <── Room.Map ───────────────────  |
  |  <── Room.Contents ──────────────  |
  |  <── Char.Affects ───────────────  |  (if character has active affects)
  |  <── Char.Enemies ───────────────  |  (if character is in combat)
  |  <── Char.Inventory ─────────────  |  Items carried + capacity
  |  <── Char.Equipment ─────────────  |  All equipment slots
  |  <── Char.Abilities ─────────────  |  Skills, spells, songs
  |  <── Char.Reputations ───────────  |  Faction standings
  |  <── Char.Church ────────────────  |  Church membership
  |  <── Char.Race ──────────────────  |  Race details
  |                                    |
  |  (Ongoing game text + GMCP)        |
```

> *\* WebSocket clients get Sentience support automatically. Sending
> `Core.Supports.Set ["Sentience 1"]` is optional but harmless.*

### GMCP Transport Format

GMCP messages are sent as WebSocket text frames in the format:

```
PackageName JSONPayload
```

Example:
```
Sentience.Char.Vitals {"hp":500,"hp_max":500,"mana":300,"mana_max":400,"move":200,"move_max":200,"_v":1}
```

The package name and JSON payload are separated by a single space.

---

## Authentication & Session Resume

### Standard Login

Send account name and password as plain text lines (just like telnet).

### Session Resume (Reconnection)

The server supports transparent session resume for WebSocket clients, allowing
reconnection without re-entering credentials.

#### Token Issuance

When a WebSocket client disconnects (cleanly or via timeout), the server issues
a resume token **before** the connection closes:

```json
Sentience.Auth.Resume {"event":"token","token":"<64-char-hex>","ttl":180}
```

| Field   | Type   | Description                                |
|---------|--------|--------------------------------------------|
| `event` | string | Always `"token"` for token issuance        |
| `token` | string | 64-character hex string (one-time use)     |
| `ttl`   | int    | Token lifetime in seconds (currently 180)  |

**Store this token in the client.** It is single-use and expires after `ttl` seconds.

#### Resuming a Session

On reconnect, instead of entering an account name at the login prompt, send:

```
RESUME <token>
```

#### Resume Responses

**Pending** (async Redis lookup in progress):
```json
Sentience.Auth.Resume {"event":"pending"}
```
The client should wait — the server will follow up with `ok` or `fail`.

**Success**:
```json
Sentience.Auth.Resume {"event":"ok"}
```
The character is reattached. Full GMCP data burst follows immediately.

**Failure**:
```json
Sentience.Auth.Resume {"event":"fail","reason":"invalid_or_expired"}
```
Fall back to standard login. The login prompt will be re-displayed.

#### Resume Architecture

- Tokens are hashed server-side; the raw token is never stored
- Local in-memory cache + Redis backend for multi-server support
- Token is consumed on use (single-use only)
- If a new token is issued for the same character, the old one is invalidated

---

## Package Reference

All packages include a `_v` (version) field. Current version is `1` for all
packages. Clients should check `_v` for forward compatibility.

---

### Sentience.Client.Ready.Capabilities

**Direction:** Server → Client
**When:** Immediately after WebSocket handshake

Declares all GMCP packages the server supports.

```json
{
  "_v": 1,
  "packages": [
    "Sentience.Char.Identity",
    "Sentience.Char.Vitals",
    "Sentience.Char.Stats",
    "Sentience.Char.Combat",
    "Sentience.Char.Worth",
    "Sentience.Char.Affects",
    "Sentience.Char.Enemies",
    "Sentience.Room.Info",
    "Sentience.Room.Contents",
    "Sentience.Room.Map",
    "Sentience.Channel.Message",
    "Sentience.Client.Preferences",
    "Sentience.Client.Layout",
    "Sentience.Auth.QRCode",
    "Sentience.Link"
  ],
  "features": ["links", "osc8"]
}
```

| Field      | Type     | Description                                      |
|------------|----------|--------------------------------------------------|
| `packages` | string[] | All GMCP package names the server can send       |
| `features` | string[] | Feature flags: `links` (interactive), `osc8` (hyperlinks) |

---

### Sentience.Client.Ready.State

**Direction:** Server → Client
**When:** Once, on first game update after login

Provides timing constants needed for client-side calculations (e.g., affect
duration countdown timers).

```json
{
  "_v": 1,
  "tick_rate": 240,
  "pulse_per_second": 4
}
```

| Field              | Type | Description                                        |
|--------------------|------|----------------------------------------------------|
| `tick_rate`        | int  | Game ticks per minute cycle (PULSE_TICK = 60 × 4)  |
| `pulse_per_second` | int  | Server pulses per real second (currently 4)         |

**Calculating affect durations:** Affects report `duration` in ticks and
`estimated_seconds` pre-calculated. The formula is:
`estimated_seconds = duration * PULSE_TICK / PULSE_PER_SECOND`

---

### Sentience.Client.Preferences

**Direction:** Bidirectional
**When:** Server sends on login; client sends to update preferences

#### Server → Client (full state)

Sent on login and after any preference change. Preferences are delivered as a
structured array with metadata about each preference:

```json
{
  "_v": 1,
  "preferences": [
    {
      "key": "gmcp_channels",
      "category": "gmcp",
      "type": "bool",
      "source": "default",
      "label": "Enable GMCP channel delivery",
      "value": true
    },
    {
      "key": "gmcp_suppress_channels",
      "category": "gmcp",
      "type": "bool",
      "source": "character",
      "value": false,
      "label": "Suppress inline channel text"
    }
  ]
}
```

#### Preference Object

| Field      | Type   | Description                                              |
|------------|--------|----------------------------------------------------------|
| `key`      | string | Preference identifier                                    |
| `category` | string | Grouping category (e.g., `"gmcp"`)                       |
| `type`     | string | Value type: `"bool"`, `"int"`, or `"string"`             |
| `source`   | string | Where the current value comes from (see below)           |
| `label`    | string | Human-readable description for UI display                |
| `value`    | mixed  | Current value (type matches the `type` field)            |

#### Source Values

| Source        | Description                                              |
|---------------|----------------------------------------------------------|
| `"default"`   | Server default — no override has been set                |
| `"account"`   | Set at account level (shared across characters)          |
| `"character"` | Set at character level (per-character override)           |

#### Client → Server (set/reset)

To change a preference, send an action:

```json
Sentience.Client.Preferences {"action": "set", "key": "gmcp_suppress_channels", "value": true, "scope": "character"}
```

To remove an override and fall back to the default:

```json
Sentience.Client.Preferences {"action": "reset", "key": "gmcp_suppress_channels"}
```

| Field    | Type   | Description                                            |
|----------|--------|--------------------------------------------------------|
| `action` | string | `"set"` to change a value, `"reset"` to remove override |
| `key`    | string | Preference key to modify                               |
| `value`  | mixed  | New value (required for `"set"`, ignored for `"reset"`) |
| `scope`  | string | `"account"` or `"character"` (required for `"set"`)     |

#### Error Responses

```json
{"error": "invalid_key"}
{"error": "invalid_value"}
```

#### Preference Behavior

- **`gmcp_channels` + `gmcp_suppress_channels`:** When both are `true`, channel
  messages are delivered ONLY via GMCP (no inline text). When `gmcp_channels` is
  `true` but `gmcp_suppress_channels` is `false`, you get both GMCP and inline.
- **`gmcp_suppress_minimap`:** When `true`, the server does not send
  `Sentience.Room.Map` and does not include inline ASCII maps in room text.
- **Scope precedence:** Character overrides account, account overrides default.
  A `"reset"` action removes the override at the stored scope, falling back to
  the next level.
- Preferences are persisted and survive logout/reconnect.

---

### Sentience.Client.Layout

**Direction:** Bidirectional
**When:** Client sends to save/load/delete/list layouts; server responds with confirmation or data
**WebSocket only:** Layout restore on login is only sent to WebSocket clients

Allows the web client to persist FlexLayout configurations on the server,
so layouts survive across sessions and devices.

#### Client → Server Actions

**Save a layout:**

```json
Sentience.Client.Layout {
  "action": "save",
  "name": "default",
  "layout": { ... }
}
```

The `layout` field is an opaque JSON object (the FlexLayout model). The server
stores it as-is without interpreting the contents.

**Load a layout:**

```json
Sentience.Client.Layout {"action": "load", "name": "default"}
```

**Delete a layout:**

```json
Sentience.Client.Layout {"action": "delete", "name": "default"}
```

**List saved layouts:**

```json
Sentience.Client.Layout {"action": "list"}
```

#### Server → Client Responses

**Saved confirmation:**

```json
{"action": "saved", "name": "default", "_v": 1}
```

**Layout restore** (on load, or automatically on login for WebSocket clients):

```json
{"action": "restore", "name": "default", "layout": { ... }, "_v": 1}
```

**Layout list:**

```json
{"action": "list", "layouts": ["default", "compact", "mobile"], "active": "default", "_v": 1}
```

The `active` field is the name of the last saved/loaded layout, or `null` if none.

**Deleted confirmation:**

```json
{"action": "deleted", "name": "default", "_v": 1}
```

**Error:**

```json
{"action": "error", "reason": "invalid_name", "_v": 1}
```

#### Error Codes

| Code | Cause |
|------|-------|
| `invalid_payload` | Root JSON is not an object, or layout field is not an object |
| `invalid_action` | Missing, non-string, or unrecognized action |
| `invalid_name` | Missing name, or name fails validation rules |
| `not_found` | Load/delete for a name that doesn't exist |
| `size_limit_exceeded` | Layout JSON exceeds 16 KB when serialized |
| `max_layouts_reached` | Already at 5 saved layouts (save of new name) |

#### Constraints

| Constraint | Value |
|------------|-------|
| Name length | 1–32 characters |
| Name characters | `[a-zA-Z0-9_-]` only |
| Max layouts per character | 5 |
| Max layout size | 16 KB (JSON compact) |

#### Implementation Notes

- **Saving is explicit.** The client should provide a "Save Layout" button; the
  server does not auto-save on every resize. Consider debouncing rapid saves.
- **Active layout is auto-set** when saving or loading. It determines which
  layout is restored on the next login.
- **Login restore** is only sent to WebSocket clients. The server sends a
  `restore` message with the active layout during the first GMCP update burst.
- **Name matching is case-insensitive** for find/delete operations.

---

### Sentience.Char.Identity

**Direction:** Server → Client
**When:** Login, level change, name change, class change, trait change

```json
{
  "_v": 1,
  "name": "Tieryo",
  "race_wnum": "1#1",
  "race": "Human",
  "body_type": "humanoid",
  "level": 92,
  "tot_level": 92,
  "title": "the Archmage",
  "classes": [
    {
      "id": "mage",
      "name": "Mage",
      "level": 92,
      "is_primary": true,
      "max_level": 100,
      "type": "mage",
      "flags": "caster",
      "primary_stat": "int",
      "hp_range": [6, 8],
      "gains_mana": true,
      "description": "Masters of arcane magic...",
      "xp": 1234567,
      "active_title": "archmage",
      "available_titles": [
        {"keyword": "archmage", "display": "the Archmage", "is_default": true},
        {"keyword": "wizard", "display": "the Wizard", "is_default": false}
      ],
      "action": null
    },
    {
      "id": "warrior",
      "name": "Warrior",
      "level": 50,
      "is_primary": false,
      "max_level": 100,
      "type": "warrior",
      "flags": "melee",
      "primary_stat": "str",
      "hp_range": [12, 16],
      "gains_mana": false,
      "description": "Masters of martial combat...",
      "xp": 500000,
      "active_title": null,
      "available_titles": [],
      "action": {"label": "Switch", "cmd": "setclass warrior"}
    }
  ],
  "traits": [
    {
      "id": "darkvision",
      "name": "Darkvision",
      "description": "Can see in the dark",
      "category": "perception",
      "type": "bool",
      "source": "race",
      "value": true
    },
    {
      "id": "strength_bonus",
      "name": "Strength Bonus",
      "description": "Bonus to strength",
      "category": "combat",
      "type": "int",
      "source": "class",
      "value": 3
    }
  ],
  "race_info": {
    "id": "human",
    "name": "Human",
    "description": "The most common race...",
    "playable": true,
    "starting": true,
    "size": "medium",
    "stats": {"str": 13, "int": 13, "wis": 13, "dex": 13, "con": 13},
    "max_stats": {"str": 22, "int": 22, "wis": 22, "dex": 22, "con": 22},
    "max_vitals": {"hp": 100, "mana": 100, "move": 100},
    "skills": ["sword", "shield block"],
    "resistances": "",
    "vulnerabilities": "",
    "immunities": "",
    "affects": "",
    "remort_into": null,
    "traits": [
      {"id": "adaptable", "name": "Adaptable", "type": "bool", "value": true}
    ]
  }
}
```

| Field       | Type     | Description                              |
|-------------|----------|------------------------------------------|
| `name`      | string   | Character name                           |
| `race_wnum` | string   | Race wide-vnum (area#vnum format)        |
| `race`      | string   | Race display name                        |
| `body_type` | string   | Body type (humanoid, quadruped, etc.)    |
| `level`     | int      | Current class level                      |
| `tot_level` | int      | Total level across all classes           |
| `title`     | string   | Character title                          |
| `classes`   | object[] | Array of class objects (see below)       |
| `traits`    | object[] | Character traits from race and class     |
| `race_info` | object   | Embedded race details (see below)        |

#### Class Object

| Field              | Type        | Description                                          |
|--------------------|-------------|------------------------------------------------------|
| `id`               | string      | Class identifier                                     |
| `name`             | string      | Class display name                                   |
| `level`            | int         | Level in this class                                  |
| `is_primary`       | bool        | Whether this is the active class                     |
| `max_level`        | int         | Maximum attainable level for this class              |
| `type`             | string      | Class archetype (e.g., `"mage"`, `"warrior"`)        |
| `flags`            | string      | Class flags (e.g., `"caster"`, `"melee"`)            |
| `primary_stat`     | string      | Primary attribute (`"str"`, `"int"`, `"wis"`, etc.)  |
| `hp_range`         | int[2]      | HP gain range per level `[min, max]`                 |
| `gains_mana`       | bool        | Whether this class gains mana on level               |
| `description`      | string      | Class description text                               |
| `xp`               | long        | Current XP in this class                             |
| `active_title`     | string/null | Currently selected class title keyword, or `null`    |
| `available_titles` | object[]    | List of unlocked class titles (see below)            |
| `action`           | object/null | Switch action for non-primary classes, or `null`     |

#### Class Title Object

| Field        | Type   | Description                              |
|--------------|--------|------------------------------------------|
| `keyword`    | string | Title keyword (used for selection)        |
| `display`    | string | Display text (e.g., `"the Archmage"`)     |
| `is_default` | bool   | Whether this is the default class title   |

#### Class Action Object

| Field   | Type   | Description                              |
|---------|--------|------------------------------------------|
| `label` | string | Button label (e.g., `"Switch"`)          |
| `cmd`   | string | MUD command to execute                   |

Non-primary classes include an `action` with `{"label": "Switch", "cmd": "setclass <id>"}`.
The primary class has `action: null`.

#### Trait Object

| Field         | Type        | Description                                      |
|---------------|-------------|--------------------------------------------------|
| `id`          | string      | Trait identifier                                 |
| `name`        | string      | Display name                                     |
| `description` | string      | Trait description                                |
| `category`    | string      | Trait category (e.g., `"perception"`, `"combat"`) |
| `type`        | string      | Value type: `"bool"` or `"int"`                  |
| `source`      | string      | Origin: `"race"` or `"class"`                    |
| `value`       | bool/int    | Trait value (type matches the `type` field)      |

#### Race Info Object (embedded)

| Field             | Type     | Description                                        |
|-------------------|----------|----------------------------------------------------|
| `id`              | string   | Race identifier                                    |
| `name`            | string   | Race display name                                  |
| `description`     | string   | Race description text                              |
| `playable`        | bool     | Whether this race is playable                      |
| `starting`        | bool     | Whether this race is available at character creation |
| `size`            | string   | Size category (e.g., `"medium"`, `"small"`)        |
| `stats`           | object   | Base stats `{str, int, wis, dex, con}`             |
| `max_stats`       | object   | Maximum stats `{str, int, wis, dex, con}`          |
| `max_vitals`      | object   | Maximum vitals `{hp, mana, move}`                  |
| `skills`          | string[] | Racial skill names                                 |
| `resistances`     | string   | Space-separated resistance flags                   |
| `vulnerabilities` | string   | Space-separated vulnerability flags                |
| `immunities`      | string   | Space-separated immunity flags                     |
| `affects`         | string   | Space-separated affect flags                       |
| `remort_into`     | string/null | Remort destination race name, or `null`          |
| `traits`          | object[] | Racial traits (same structure as trait objects above, without `description`, `category`, or `source`) |

> **Note:** In the embedded `race_info`, `resistances`, `vulnerabilities`,
> `immunities`, and `affects` are **space-separated strings**. This differs from
> the standalone `Sentience.Char.Race` package which uses JSON arrays. See
> [Sentience.Char.Race](#sentiencecharrace) for details.

---

### Sentience.Char.Vitals

**Direction:** Server → Client
**When:** HP, mana, or move changes

```json
{
  "_v": 1,
  "hp": 500,
  "hp_max": 500,
  "mana": 300,
  "mana_max": 400,
  "move": 200,
  "move_max": 200
}
```

| Field      | Type | Description              |
|------------|------|--------------------------|
| `hp`       | int  | Current hit points       |
| `hp_max`   | int  | Maximum hit points       |
| `mana`     | int  | Current mana             |
| `mana_max` | int  | Maximum mana             |
| `move`     | int  | Current movement points  |
| `move_max` | int  | Maximum movement points  |

---

### Sentience.Char.Stats

**Direction:** Server → Client
**When:** Any stat, hitroll, damroll, or wimpy changes

```json
{
  "_v": 1,
  "str": 22, "str_base": 18,
  "int": 25, "int_base": 20,
  "wis": 20, "wis_base": 18,
  "dex": 21, "dex_base": 19,
  "con": 19, "con_base": 17,
  "hitroll": 45,
  "damroll": 38,
  "wimpy": 100
}
```

| Field       | Type | Description                               |
|-------------|------|-------------------------------------------|
| `str`       | int  | Current strength (with modifiers)         |
| `str_base`  | int  | Base/permanent strength                   |
| `int`       | int  | Current intelligence                      |
| `int_base`  | int  | Base intelligence                         |
| `wis`       | int  | Current wisdom                            |
| `wis_base`  | int  | Base wisdom                               |
| `dex`       | int  | Current dexterity                         |
| `dex_base`  | int  | Base dexterity                            |
| `con`       | int  | Current constitution                      |
| `con_base`  | int  | Base constitution                         |
| `hitroll`   | int  | Hit bonus                                 |
| `damroll`   | int  | Damage bonus                              |
| `wimpy`     | int  | Auto-flee HP threshold                    |

---

### Sentience.Char.Combat

**Direction:** Server → Client
**When:** Armor class changes

```json
{
  "_v": 1,
  "ac_pierce": -350,
  "ac_bash": -300,
  "ac_slash": -325,
  "ac_exotic": -275
}
```

| Field       | Type | Description                         |
|-------------|------|-------------------------------------|
| `ac_pierce` | int  | Armor class vs. piercing attacks    |
| `ac_bash`   | int  | Armor class vs. bashing attacks     |
| `ac_slash`  | int  | Armor class vs. slashing attacks    |
| `ac_exotic` | int  | Armor class vs. exotic attacks      |

> Lower (more negative) values are better.

---

### Sentience.Char.Worth

**Direction:** Server → Client
**When:** XP, gold, alignment, or practices change

```json
{
  "_v": 1,
  "alignment": 1000,
  "xp": 1500000,
  "xp_tnl": 250000,
  "practices": 42,
  "gold": 85000
}
```

| Field       | Type | Description                            |
|-------------|------|----------------------------------------|
| `alignment` | int  | Alignment (-1000 evil to +1000 good)   |
| `xp`        | long | Current experience points              |
| `xp_tnl`    | long | Experience needed for next level       |
| `practices` | int  | Available practice sessions            |
| `gold`      | long | Gold on hand                           |

---

### Sentience.Char.Affects

**Direction:** Server → Client
**When:** Affect list changes (added, removed, or expired)

```json
{
  "_v": 1,
  "affects": [
    {
      "name": "armor",
      "wnum": "0#1",
      "duration": 12,
      "estimated_seconds": 720,
      "modifier": "+20 armour class",
      "level": 50
    },
    {
      "name": "sanctuary",
      "wnum": "0#45",
      "duration": -1,
      "estimated_seconds": -1,
      "modifier": null,
      "level": 60
    }
  ]
}
```

| Field | Type     | Description                     |
|-------|----------|---------------------------------|
| `affects` | object[] | Array of active affects (may be empty) |

#### Affect Object

| Field               | Type        | Description                                       |
|---------------------|-------------|---------------------------------------------------|
| `name`              | string      | Spell/skill name                                  |
| `wnum`              | string/null | Skill wide-vnum (null if not applicable)          |
| `duration`          | int         | Remaining ticks (-1 = permanent)                  |
| `estimated_seconds` | int         | Pre-calculated remaining seconds (-1 = permanent) |
| `modifier`          | string/null | Effect description (e.g., "+20 armour class")     |
| `level`             | int         | Spell level                                       |

**Transition detection:** When all affects wear off, the server sends one final
update with an empty `affects` array so the client can clear its display.

**Client-side countdown:** Use `estimated_seconds` for initial display, then
count down in real-time. The server will send a fresh update when affects actually
change server-side.

---

### Sentience.Char.Enemies

**Direction:** Server → Client
**When:** Combat state changes (start, end, new targets)

```json
{
  "_v": 1,
  "enemies": [
    {
      "name": "a black guard",
      "instance_id": [15627200, 1],
      "hp_pct": 75,
      "is_primary": true,
      "target": "you"
    },
    {
      "name": "a black guard",
      "instance_id": [15627201, 1],
      "hp_pct": 90,
      "is_primary": false,
      "target": "Tieryo"
    }
  ],
  "self": {
    "hp_pct": 95
  }
}
```

| Field     | Type     | Description                                |
|-----------|----------|--------------------------------------------|
| `enemies` | object[] | Array of enemies in combat (may be empty)  |
| `self`    | object   | Player's own combat status                 |

#### Enemy Object

| Field         | Type     | Description                                       |
|---------------|----------|---------------------------------------------------|
| `name`        | string   | Enemy short description                           |
| `instance_id` | int[2]  | 64-bit unique instance ID as [low, high]          |
| `hp_pct`      | int      | Enemy health percentage (0-100)                   |
| `is_primary`  | bool     | `true` if this is the player's direct target      |
| `target`      | string   | Who this enemy is attacking (`"you"` or a name)   |

#### Self Object

| Field    | Type | Description                       |
|----------|------|-----------------------------------|
| `hp_pct` | int  | Player's health percentage (0-100)|

**Transition detection:** When combat ends, the server sends a final update with
an empty `enemies` array.

---

### Sentience.Char.Inventory

**Direction:** Server → Client
**When:** Item pickup/drop, give, get, loot changes

```json
{
  "_v": 1,
  "items": [
    {
      "id": [12345, 67890],
      "name": "a gleaming longsword",
      "keywords": "gleaming longsword sword",
      "keyword": "gleaming longsword sword",
      "item_type": "weapon",
      "level": 50,
      "weight": 5,
      "condition": 95,
      "condition_label": "excellent",
      "item_count": 1,
      "flags": [],
      "actions": [
        {"label": "Wear", "cmd": "wear"},
        {"label": "Drop", "cmd": "drop"},
        {"label": "Examine", "cmd": "examine"}
      ]
    }
  ],
  "capacity": {
    "items": 5,
    "max_items": 20,
    "weight": 25,
    "max_weight": 200,
    "coin_weight": 3
  }
}
```

| Field      | Type     | Description                            |
|------------|----------|----------------------------------------|
| `items`    | object[] | Array of carried items                 |
| `capacity` | object   | Carrying capacity info                 |

#### Item Object

| Field             | Type     | Description                                          |
|-------------------|----------|------------------------------------------------------|
| `id`              | int[2]   | Unique instance ID as `[lo, hi]`                     |
| `name`            | string   | Item short description                               |
| `keywords`        | string   | Space-separated keywords (display use)               |
| `keyword`         | string   | Space-separated keywords (for building commands)     |
| `item_type`       | string   | Item type (e.g., `"weapon"`, `"armor"`, `"potion"`)  |
| `level`           | int      | Item level                                           |
| `weight`          | int      | Item weight                                          |
| `condition`       | int      | Condition percentage (0–100)                         |
| `condition_label` | string   | Human-readable condition (e.g., `"excellent"`)       |
| `item_count`      | int      | Stack count (1 for non-stacked items)                |
| `flags`           | string[] | Item flags                                           |
| `actions`         | object[] | Server-determined actions based on item flags        |

#### Capacity Object

| Field        | Type | Description                               |
|--------------|------|-------------------------------------------|
| `items`      | int  | Number of items currently carried          |
| `max_items`  | int  | Maximum number of items                    |
| `weight`     | int  | Current total weight carried               |
| `max_weight` | int  | Maximum carry weight                       |
| `coin_weight`| int  | Weight contributed by coins                |

#### Action Object

| Field   | Type   | Description                          |
|---------|--------|--------------------------------------|
| `label` | string | Button/menu display text             |
| `cmd`   | string | MUD command to execute               |

**Instance IDs:** The `id` field is a 2-element array `[lo, hi]` forming a
unique instance ID. Use `keyword` for building commands (e.g., `wear longsword`).

**Duplicate items:** When multiple items share the same keyword, use the MUD's
`N.keyword` syntax to target a specific one (e.g., `drop 2.potion`).

---

### Sentience.Char.Equipment

**Direction:** Server → Client
**When:** Equip/remove/wear changes

```json
{
  "_v": 1,
  "slots": [
    {
      "slot_id": 0,
      "slot_name": "<used as light>",
      "occupied": false,
      "item": null
    },
    {
      "slot_id": 16,
      "slot_name": "<wielded>",
      "occupied": true,
      "item": {
        "id": [12345, 67890],
        "name": "a gleaming longsword",
        "keywords": "gleaming longsword sword",
        "item_type": "weapon",
        "level": 50,
        "condition": 95,
        "condition_label": "excellent",
        "flags": [],
        "actions": [
          {"label": "Remove", "cmd": "remove"},
          {"label": "Examine", "cmd": "examine"}
        ]
      }
    }
  ]
}
```

| Field   | Type     | Description                               |
|---------|----------|-------------------------------------------|
| `slots` | object[] | All equipment slots (occupied and empty)  |

#### Slot Object

| Field       | Type        | Description                                      |
|-------------|-------------|--------------------------------------------------|
| `slot_id`   | int         | Numeric slot identifier                          |
| `slot_name` | string      | Human-readable wear location (color codes stripped) |
| `occupied`  | bool        | Whether an item is equipped in this slot         |
| `item`      | object/null | Equipped item details, or `null` if empty        |

#### Equipment Item Object

| Field             | Type     | Description                                          |
|-------------------|----------|------------------------------------------------------|
| `id`              | int[2]   | Unique instance ID as `[lo, hi]`                     |
| `name`            | string   | Item short description                               |
| `keywords`        | string   | Space-separated keywords                             |
| `item_type`       | string   | Item type (e.g., `"weapon"`, `"armor"`)              |
| `level`           | int      | Item level                                           |
| `condition`       | int      | Condition percentage (0–100)                         |
| `condition_label` | string   | Human-readable condition                             |
| `flags`           | string[] | Item flags                                           |
| `actions`         | object[] | Server-determined actions (e.g., Remove, Examine)    |

**All slots are always sent** — both occupied and empty. This gives the client
the complete equipment layout. When `occupied` is `false`, `item` is `null`.

---

### Sentience.Char.Abilities

**Direction:** Server → Client
**When:** Skill/spell gain, practice, level change, class switch

```json
{
  "_v": 1,
  "abilities": [
    {
      "name": "fireball",
      "type": "spell",
      "available": true,
      "rating": 85,
      "modifier": 5,
      "mana": 25,
      "level": 20,
      "target": "offensive",
      "can_practice": true,
      "learn_rate": 4,
      "actions": [
        {"label": "Cast", "cmd": "cast"}
      ]
    },
    {
      "name": "sword",
      "type": "skill",
      "available": true,
      "rating": 100,
      "modifier": 0,
      "mana": 0,
      "level": 1,
      "target": "passive",
      "can_practice": false,
      "learn_rate": 0,
      "actions": [
        {"label": "Use", "cmd": "sword"}
      ]
    },
    {
      "name": "ballad of heroes",
      "type": "song",
      "available": true,
      "rating": 72,
      "modifier": 0,
      "mana": 30,
      "level": 15,
      "target": "ignore",
      "can_practice": true,
      "learn_rate": 3,
      "actions": [
        {"label": "Play", "cmd": "play"}
      ]
    }
  ]
}
```

| Field       | Type     | Description                                  |
|-------------|----------|----------------------------------------------|
| `abilities` | object[] | Array of known abilities with rating >= 1    |

#### Ability Object

| Field          | Type     | Description                                         |
|----------------|----------|-----------------------------------------------------|
| `name`         | string   | Ability name                                        |
| `type`         | string   | `"skill"`, `"spell"`, or `"song"`                   |
| `available`    | bool     | Whether the ability is currently usable              |
| `rating`       | int      | Proficiency percentage (1–100)                       |
| `modifier`     | int      | Bonus/penalty modifier                               |
| `mana`         | int      | Mana cost (0 for passive skills)                     |
| `level`        | int      | Level at which this ability is gained                |
| `target`       | string   | Targeting type (see below)                           |
| `can_practice` | bool     | Whether this ability can be practiced                |
| `learn_rate`   | int      | Practice efficiency rating (0 if not practicable)    |
| `actions`      | object[] | Available actions (only present when `available`)    |

#### Target Types

| Target        | Description                          |
|---------------|--------------------------------------|
| `"offensive"` | Targets an enemy                     |
| `"defensive"` | Targets an ally                      |
| `"self"`      | Targets self only                    |
| `"object"`    | Targets an item                      |
| `"passive"`   | No active targeting (passive skill)  |
| `"ignore"`    | No target required                   |

#### Ability Actions

Actions are determined by ability type:

| Type    | Action Label | Command                      |
|---------|-------------|-------------------------------|
| `spell` | `"Cast"`    | `"cast"`                      |
| `skill` | `"Use"`     | Skill name (e.g., `"sword"`)  |
| `song`  | `"Play"`    | `"play"`                      |

Actions are only present when `available` is `true`. Only abilities with
`rating` >= 1 are included in the list.

---

### Sentience.Char.Reputations

**Direction:** Server → Client
**When:** Reputation gain/loss, rank change

```json
{
  "_v": 1,
  "reputations": [
    {
      "name": "Elven Court",
      "rank": "Honored",
      "rank_color": "green",
      "points": 15000,
      "paragon_level": 0,
      "max_rank": "Exalted"
    }
  ]
}
```

| Field         | Type     | Description                          |
|---------------|----------|--------------------------------------|
| `reputations` | object[] | Array of visible reputation standings |

#### Reputation Object

| Field           | Type   | Description                                       |
|-----------------|--------|---------------------------------------------------|
| `name`          | string | Faction name                                      |
| `rank`          | string | Current rank name (e.g., `"Honored"`)             |
| `rank_color`    | string | CSS-friendly color name for the rank (see below)  |
| `points`        | int    | Current reputation points                         |
| `paragon_level` | int    | Paragon level (0 if not at paragon rank)          |
| `max_rank`      | string | Highest possible rank name                        |

#### Rank Colors

The `rank_color` field contains a CSS-friendly color name:

| Color     | Usage                |
|-----------|----------------------|
| `red`     | Hostile/hated        |
| `green`   | Friendly/honored     |
| `blue`    | Allied               |
| `yellow`  | Cautious/neutral     |
| `magenta` | Special              |
| `cyan`    | Special              |
| `white`   | Neutral/default      |
| `dark`    | Unknown/unfriendly   |

**Hidden reputations** (server-side `REPUTATION_HIDDEN` flag) are excluded from
this list and never sent to the client.

---

### Sentience.Char.Church

**Direction:** Server → Client
**When:** Church join/leave, rank change, church update

When the character is **not** a church member:

```json
{
  "_v": 1,
  "is_member": false
}
```

When the character **is** a church member:

```json
{
  "_v": 1,
  "is_member": true,
  "church_name": "Order of the Dawn",
  "church_flag": "OtD",
  "alignment": "good",
  "size": "order",
  "pk": false,
  "rank_name": "Guardian",
  "rank_type": "officer",
  "rank_title": "Sir Guardian",
  "actions": [
    {"label": "Talk", "cmd": "chtalk"},
    {"label": "Go Hall", "cmd": "gohall"}
  ]
}
```

| Field         | Type     | Presence    | Description                                    |
|---------------|----------|-------------|------------------------------------------------|
| `is_member`   | bool     | Always      | Whether the character belongs to a church      |
| `church_name` | string   | Member only | Church display name                            |
| `church_flag` | string   | Member only | Short church abbreviation                      |
| `alignment`   | string   | Member only | `"good"`, `"evil"`, or `"neutral"`             |
| `size`        | string   | Member only | Church size tier (see below)                   |
| `pk`          | bool     | Member only | Whether the church is PK-enabled               |
| `rank_name`   | string   | Member only | Character's rank name                          |
| `rank_type`   | string   | Member only | Rank category (see below)                      |
| `rank_title`  | string   | Member only | Gender-appropriate rank title                  |
| `actions`     | object[] | Member only | Available church actions                       |

#### Church Size Tiers

| Size       | Description         |
|------------|---------------------|
| `"band"`   | Smallest church     |
| `"cult"`   | Small church        |
| `"order"`  | Medium church       |
| `"church"` | Largest church      |

#### Rank Types

| Type       | Description                  |
|------------|------------------------------|
| `"member"` | Regular member               |
| `"officer"`| Officer with elevated access |
| `"leader"` | Church leader                |

When `is_member` is `false`, no fields other than `_v` and `is_member` are
present. The `rank_title` is gender-appropriate (selected from the rank
definition's male/female/neutral title).

---

### Sentience.Char.Race

**Direction:** Server → Client
**When:** Race change (remort, polymorph)

Sent as a standalone notification when the character's race changes. This is a
rare event (remort, polymorph).

```json
{
  "_v": 1,
  "id": "elf",
  "name": "Elf",
  "description": "Graceful and long-lived...",
  "playable": true,
  "starting": true,
  "size": "medium",
  "stats": {"str": 11, "int": 16, "wis": 14, "dex": 15, "con": 11},
  "max_stats": {"str": 20, "int": 25, "wis": 23, "dex": 24, "con": 20},
  "max_vitals": {"hp": 100, "mana": 120, "move": 100},
  "skills": ["longbow", "elven lore"],
  "resistances": ["charm"],
  "vulnerabilities": ["iron"],
  "immunities": [],
  "affects": ["infrared"],
  "remort_into": "high elf",
  "traits": [
    {"id": "elven_grace", "name": "Elven Grace", "type": "bool", "value": true}
  ]
}
```

| Field             | Type        | Description                                        |
|-------------------|-------------|----------------------------------------------------|
| `id`              | string      | Race identifier                                    |
| `name`            | string      | Race display name                                  |
| `description`     | string      | Race description text                              |
| `playable`        | bool        | Whether this race is playable                      |
| `starting`        | bool        | Whether this race is available at character creation |
| `size`            | string      | Size category (e.g., `"medium"`, `"small"`)        |
| `stats`           | object      | Base stats `{str, int, wis, dex, con}`             |
| `max_stats`       | object      | Maximum stats `{str, int, wis, dex, con}`          |
| `max_vitals`      | object      | Maximum vitals `{hp, mana, move}`                  |
| `skills`          | string[]    | Racial skill names                                 |
| `resistances`     | string[]    | Array of resistance flag names                     |
| `vulnerabilities` | string[]    | Array of vulnerability flag names                  |
| `immunities`      | string[]    | Array of immunity flag names                       |
| `affects`         | string[]    | Array of affect flag names                         |
| `remort_into`     | string/null | Remort destination race name, or `null`            |
| `traits`          | object[]    | Racial traits (same structure as identity traits)  |

> **Note:** In this standalone package, `resistances`, `vulnerabilities`,
> `immunities`, and `affects` are **JSON arrays** of flag name strings. This
> differs from the embedded `race_info` in `Sentience.Char.Identity`, which uses
> space-separated strings. The two packages use the same underlying data but
> different serialization: Identity uses `json_string()` for compactness, while
> this package uses `split_to_json_array()` for easier client parsing.

---

### Sentience.Room.Info

**Direction:** Server → Client
**When:** Player moves to a new room

```json
{
  "_v": 1,
  "wnum": "924#37",
  "name": "The City Limits",
  "area_name": "Crystal City",
  "area_wnum": "924",
  "sector": "city",
  "is_wilds": false,
  "wilds_uid": 0,
  "wilds_x": 0,
  "wilds_y": 0,
  "exits": {
    "n": {
      "wnum": "924#38",
      "name": "",
      "is_door": false,
      "is_closed": false,
      "is_locked": false
    },
    "s": {
      "wnum": "924#36",
      "name": "gate",
      "is_door": true,
      "is_closed": true,
      "is_locked": false
    }
  }
}
```

| Field       | Type   | Description                                    |
|-------------|--------|------------------------------------------------|
| `wnum`      | string | Room wide-vnum (`area#vnum` format)            |
| `name`      | string | Room title                                     |
| `area_name` | string | Area display name                              |
| `area_wnum` | string | Area wide-vnum                                 |
| `sector`    | string | Sector type (city, forest, water, etc.)        |
| `is_wilds`  | bool   | Whether this is a wilderness/overland room     |
| `wilds_uid` | int    | Wilderness UID (0 if not wilderness)           |
| `wilds_x`   | int    | X coordinate in wilderness (0 if not wilds)    |
| `wilds_y`   | int    | Y coordinate in wilderness (0 if not wilds)    |
| `exits`     | object | Map of direction → exit info                   |

#### Exit Object

| Field       | Type   | Description                          |
|-------------|--------|--------------------------------------|
| `wnum`      | string | Destination room wide-vnum           |
| `name`      | string | Door keyword (empty if no door)      |
| `is_door`   | bool   | Whether this exit has a door         |
| `is_closed` | bool   | Whether the door is closed           |
| `is_locked` | bool   | Whether the door is locked           |

**Direction keys:** `n`, `s`, `e`, `w`, `u` (up), `d` (down), plus any custom
exits defined by builders.

---

### Sentience.Room.Contents

**Direction:** Server → Client
**When:** Room entity count changes (items/NPCs/players enter/leave, doors open/close)

```json
{
  "_v": 1,
  "items": [
    {
      "name": "a rusty sword",
      "instance_id": [196228330, 1],
      "short_desc": "A rusty sword lies here."
    }
  ],
  "npcs": [
    {
      "name": "the crystalline guard",
      "instance_id": [15627017, 1],
      "short_desc": "A crystalline guard stands here, watching."
    }
  ],
  "players": [
    {
      "name": "Tieryo",
      "instance_id": [12345, 0],
      "short_desc": ""
    }
  ],
  "doors": [
    {
      "direction": "south",
      "state": "closed",
      "is_locked": false
    }
  ]
}
```

| Field     | Type     | Description                              |
|-----------|----------|------------------------------------------|
| `items`   | object[] | Visible items on the ground (max 128)    |
| `npcs`    | object[] | Visible NPCs in the room (max 64)       |
| `players` | object[] | Visible players in the room (max 64)     |
| `doors`   | object[] | Doors with the EX_ISDOOR flag (max 10)   |

#### Entity Object (items, npcs, players)

| Field         | Type   | Description                                  |
|---------------|--------|----------------------------------------------|
| `name`        | string | Short description (items/NPCs) or name (players) |
| `instance_id` | int[2] | 64-bit unique instance ID as [low, high]     |
| `short_desc`  | string | Long description (empty string for players)  |

#### Door Object

| Field       | Type   | Description                                |
|-------------|--------|--------------------------------------------|
| `direction` | string | Exit direction (north, south, etc.)        |
| `state`     | string | One of: `"open"`, `"closed"`, `"locked"`   |
| `is_locked` | bool   | Whether the door is locked                 |

**Note:** Room.Contents uses a fingerprint-based comparison (room ID + total
entity count). It does not track individual entity changes — the entire contents
list is resent when the fingerprint changes.

---

### Sentience.Room.Map

**Direction:** Server → Client
**When:** Player moves to a new room (sent alongside Room.Info)

```json
{
  "_v": 1,
  "type": "area",
  "map_text": "{G#{x-{G#{x\n{G|{x {G|{x\n{G#{x-{G#{x",
  "width": 5,
  "height": 3
}
```

| Field      | Type   | Description                                        |
|------------|--------|----------------------------------------------------|
| `type`     | string | `"area"` (minimap) or `"wilds"` (overland)         |
| `map_text` | string | Pre-rendered ASCII map with MUD color codes         |
| `width`    | int    | Map width in characters                             |
| `height`   | int    | Map height in lines                                 |

**Color codes in map_text:** The map uses MUD color codes (see
[Color Code Reference](#color-code-reference)). The client must parse and render
these as ANSI or HTML colors.

**Map types:**
- `"area"` — Small minimap showing nearby rooms and exits (typically 7-15 chars wide)
- `"wilds"` — Larger overland terrain map showing the wilderness around the player

**Suppression:** If the `gmcp_suppress_minimap` preference is enabled, this
package is not sent, and the inline ASCII map in room text is also suppressed.

---

### Sentience.Channel.Message

**Direction:** Server → Client
**When:** Channel message or tell received

```json
{
  "_v": 1,
  "channel": "gossip",
  "sender": "Tieryo",
  "text": "Hello everyone!",
  "timestamp": 1711382400,
  "report_id": "local-12345",
  "actions": [
    {"label": "Info", "cmd": "chinfo"},
    {"label": "Report", "cmd": "report local-12345"}
  ]
}
```

Directed message (tell):
```json
{
  "_v": 1,
  "channel": "tell",
  "sender": "Tieryo",
  "text": "Hey, are you there?",
  "timestamp": 1711382400,
  "tell_target": "Merlin",
  "report_id": "local-12346",
  "actions": [
    {"label": "Info", "cmd": "chinfo"},
    {"label": "Report", "cmd": "report local-12346"},
    {"label": "Reply", "cmd": "reply"}
  ]
}
```

| Field         | Type           | Description                                        |
|---------------|----------------|----------------------------------------------------|
| `channel`     | string         | Channel ID (gossip, say, tell, auction, etc.)      |
| `sender`      | string         | Sender name (empty string for system messages)     |
| `text`        | string         | Message text (**color codes stripped**)             |
| `timestamp`   | long           | Unix timestamp                                     |
| `tell_target` | string/null    | Recipient name for directed messages (tells only)  |
| `report_id`   | string/absent  | Message identifier for reporting (see below)       |
| `actions`     | object[]/absent| Server-determined actions (see below)              |

#### Report ID

The `report_id` field uses the format `"local-<seq>"` where `<seq>` is a
server-side sequence number. It is only present when a report ID was generated.
Use this ID with the `report` command to report abusive messages.

#### Message Actions

| Action     | Command                  | When Present                  |
|------------|--------------------------|-------------------------------|
| `"Info"`   | `"chinfo"`               | Always (when actions present) |
| `"Report"` | `"report <report_id>"`   | When `report_id` is set       |
| `"Reply"`  | `"reply"`                | Tells and directed channels   |

The `actions` array is only present when actions exist. Each action follows the
standard `{label, cmd}` pattern used across all packages.

**Color stripping:** Unlike most packages, Channel.Message text has MUD color
codes stripped before sending. The client should render the text as-is.

**Delivery scope:** The server respects channel scope rules. You will only
receive messages for channels you have access to and are subscribed to.

**Inline suppression:** When both `gmcp_channels` and `gmcp_suppress_channels`
preferences are `true`, channel messages appear only via GMCP (not in the text
stream). This allows the web client to render channels in a dedicated panel.

---

### Sentience.Link.List

**Direction:** Server → Client (WebSocket only)
**When:** Before every text frame that contains interactive links

```json
[
  {
    "id": "lk_0",
    "text": "a rusty sword",
    "category": "obj",
    "actions": [
      {"label": "Look", "cmd": "look sword"},
      {"label": "Get", "cmd": "get sword"},
      {"label": "Identify", "cmd": "cast 'identify' sword"}
    ]
  },
  {
    "id": "lk_1",
    "text": "the crystalline guard",
    "hint": "Level 40 guard",
    "category": "mob",
    "actions": [
      {"label": "Look", "cmd": "look guard"},
      {"label": "Consider", "cmd": "consider guard"},
      {"label": "Kill", "cmd": "kill guard"}
    ]
  },
  {
    "id": "lk_2",
    "text": "924#38",
    "category": "room",
    "actions": [
      {"label": "Look", "cmd": "look north"}
    ]
  }
]
```

> **Note:** This is a JSON **array**, not an object. It has no `_v` wrapper.

| Field | Type | Description |
|-------|------|-------------|
| (root) | object[] | Array of link objects for the upcoming text frame |

#### Link Object

| Field      | Type     | Description                                   |
|------------|----------|-----------------------------------------------|
| `id`       | string   | Link ID referenced in text (e.g., `"lk_0"`)  |
| `text`     | string   | Display text (may contain MUD color codes)    |
| `hint`     | string   | Optional tooltip/hover text                   |
| `category` | string   | Entity type (see below)                       |
| `actions`  | object[] | Context menu actions (max 8)                  |

#### Link Categories

| Category  | Description          |
|-----------|----------------------|
| `obj`     | Items/objects        |
| `mob`     | NPCs/mobiles         |
| `room`    | Rooms                |
| `player`  | Player characters    |
| `help`    | Help topics          |
| `cmd`     | Commands             |

#### Action Object

| Field   | Type   | Description                          |
|---------|--------|--------------------------------------|
| `label` | string | Menu item display text               |
| `cmd`   | string | MUD command to send when clicked     |
| `hint`  | string | Optional tooltip for this action     |

#### How Links Work in Text

The text stream references links using OSC 8 hyperlink sequences:

```
␛]8;;lk_0<display text>␛]8;;
```

Where `lk_0` matches a link `id` in the preceding `Sentience.Link.List` frame.

**Client implementation:**
1. Receive `Sentience.Link.List` — store the link metadata
2. Receive text frame — parse OSC 8 sequences to find link IDs
3. Replace each link reference with interactive UI (clickable text, context menu
   from `actions`, tooltip from `hint`, styling based on `category`)
4. Clear stored links after processing the text frame

**Constraints:**
- Maximum 256 links per frame
- Maximum 8 actions per link
- Link queue is flushed (sent + cleared) before each text frame

---

### Sentience.Editor — Overview

The Editor package supports **staged editing** for all Tier 1 OLC editors
(redit, medit, oedit, aedit). Changes accumulate in memory and are applied
atomically via `Commit`. This enables web client UIs that show pending changes,
provide undo, and batch complex edits.

> **Note:** There is no `Editor.Open` message currently. When a builder enters
> an editor, the server sends `Editor.State` with the initial (empty) changeset.
> A future version will send an `Editor.Open` message with a full field schema
> so the web client can dynamically render editor forms.

#### Entity ID Format

Entity IDs use the pattern `"type:auid#vnum"` where `auid` is the area unique ID
and `vnum` is the entity's virtual number within that area.

| Editor | Format | Example |
|--------|--------|---------|
| Room   | `room:auid#vnum` | `"room:5#3001"` |
| Mobile | `mob:auid#vnum`  | `"mob:5#3005"` |
| Object | `obj:auid#vnum`  | `"obj:5#3010"` |
| Area   | `area:auid`      | `"area:5"` |

#### Field Types

The `type` field in change objects and field updates uses these values:

| Type | Description | Suggested Client Widget |
|------|-------------|-------------------------|
| `"string"` | Single-line text | Text input |
| `"multiline"` | Multi-line text (descriptions) | Text area / StringEdit panel |
| `"int"` | Integer | Number input |
| `"int16"` | Short integer | Number input |
| `"bool"` | Boolean toggle | Checkbox |
| `"flags"` | Bitfield (multiple selections) | Multi-select / checkbox group |
| `"widevnum"` | Wide virtual number reference | Custom vnum selector |
| `"exit"` | Exit data (complex sub-object) | Custom exit editor |
| `"embedded"` | Embedded sub-object | Nested form |
| `"list_add"` | List append operation | Add button + item form |
| `"list_remove"` | List remove operation | Delete button |
| `"list_update"` | List item update | Inline edit |
| `"type_data"` | Type-specific structured data | Custom widget |

---

### Sentience.Editor.State

**Direction:** Server → Client
**When:** Sent on editor entry, after revert, after draft restore, or in response
to `Editor.Request`. Provides the full pending change list for the entity.

```json
{
  "entity_id": "room:5#3001",
  "pending_count": 2,
  "changes": [
    {"field": "name", "type": "string", "value": "A Glowing Cavern"},
    {"field": "heal_rate", "type": "int", "value": 200}
  ],
  "draft_restored": false,
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `pending_count` | int | ✓ | Number of staged changes |
| `changes` | array | ✓ | Array of pending change objects (empty if none) |
| `draft_restored` | bool | ✓ | `true` if state was loaded from a saved draft |
| `_v` | int | ✓ | Message version (always `1`) |

#### Change Object Fields

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `field` | string | ✓ | Field path (e.g., `"name"`, `"description"`) |
| `type` | string | ✓ | Field type (see Field Types above) |
| `value` | mixed | ✓ | New staged value (`null` if value is not set) |

> **Note:** The change object contains only the **new value**, not the old value.
> To show a diff, the client should capture the original value from the entity
> at editor entry time, or use the `is_pending` flag from `Editor.Field` updates.

---

### Sentience.Editor.Field

**Direction:** Server → Client
**When:** After a `Set` stages a change, after a single-field revert, or after a
MUD-side command modifies a field

```json
{
  "entity_id": "room:5#3001",
  "field": "name",
  "value": "A Glowing Cavern",
  "type": "string",
  "is_pending": true,
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `field` | string | ✓ | Field path that changed |
| `value` | mixed | ✓ | New value (`null` if the value was cleared or reverted to a null live value) |
| `type` | string | ✓ | Field type (default: `"string"`) |
| `is_pending` | bool | ✓ | `true` if the field has a staged change; `false` if it was reverted to the live value |
| `_v` | int | ✓ | Message version (always `1`) |

When `is_pending` is `false`, it means a previous staging was collapsed away
(e.g., the builder changed a value back to its original). The `value` field
may be `null` in this case — the client should revert the field to its original
display value.

---

### Sentience.Editor.Close

**Direction:** Server → Client
**When:** The builder exits the editor (via `done` command or disconnect). Not
sent on commit — the editor stays open after committing.

```json
{
  "entity_id": "room:5#3001",
  "reason": "done",
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `entity_id` | string | ✓ | Entity that was being edited |
| `reason` | string | ✓ | Close reason (see table) |
| `_v` | int | ✓ | Message version (always `1`) |

| Reason | Description |
|--------|-------------|
| `"done"` | Builder exited normally |
| `"forced"` | Admin override or entity deleted |
| `"disconnect"` | Session lost (draft was auto-saved if changes were pending) |

---

### Sentience.Editor.Error

**Direction:** Server → Client
**When:** A client request failed validation or processing

```json
{
  "entity_id": "room:5#3001",
  "field": "heal_rate",
  "error": "limit_reached",
  "message": "Too many pending changes.",
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `entity_id` | string | ✓ | Entity ID (empty string `""` if not applicable) |
| `field` | string | ✗ | Field name (omitted when error is not field-specific) |
| `error` | string | ✓ | Machine-readable error code (see table) |
| `message` | string | ✓ | Human-readable error description |
| `_v` | int | ✓ | Message version (always `1`) |

| Error Code | When |
|------------|------|
| `"invalid_request"` | Missing required fields in the payload |
| `"invalid_entity"` | Entity ID doesn't match current editor session |
| `"not_editing"` | No active editor session for this entity type |
| `"limit_reached"` | Too many pending changes (per-entity or per-builder limit) |
| `"no_changes"` | Commit, save, or revert requested with no pending changes |
| `"not_found"` | Single-field revert — no pending change for that field |
| `"internal_error"` | Editor definition lookup failed |
| `"commit_failed"` | Error applying changes to the live entity |
| `"no_session"` | No active editing state (for string edit operations) |
| `"invalid_session"` | String edit session ID not found |
| `"parse_error"` | Invalid JSON in the incoming payload |
| `"save_failed"` | Draft persistence failed (I/O error) |
| `"no_draft"` | No saved draft exists for this entity |
| `"load_failed"` | Draft file exists but could not be loaded (corrupt) |

---

### Sentience.Editor.Set

**Direction:** Client → Server
**When:** Client wants to stage a field change

```json
{"entity_id": "room:5#3001", "field": "name", "value": "A Glowing Cavern"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Must match current editor session |
| `field` | string | ✓ | Field path to modify |
| `value` | mixed | ✓ | New value — type is auto-detected: JSON integer → `int`, JSON boolean → `bool`, otherwise → `string` |

**Response:**
- **Success:** `Editor.Field` with `is_pending: true` and the staged value
- **No-op (reverted to original):** `Editor.Field` with `is_pending: false` and `value: null`
- **Failure:** `Editor.Error`

The server automatically detects no-op changes (setting a field back to its
original value) and collapses them, removing the pending change entirely.

---

### Sentience.Editor.Commit

**Direction:** Client → Server
**When:** Client wants to apply all pending changes to the live entity

```json
{"entity_id": "room:5#3001", "comment": "Updated room name and heal rate"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity to commit |
| `comment` | string | ✗ | Optional audit log comment (reserved for future use) |

**Response:** `Editor.CommitResult` on success, `Editor.Error` on failure.

> **Note:** Group commit (multiple entities in one message) is defined in the
> protocol but not yet exposed via the `Commit` handler. Currently, commit one
> entity at a time.

---

### Sentience.Editor.Revert

**Direction:** Client → Server
**When:** Client wants to discard pending changes

Revert **all** pending changes:
```json
{"entity_id": "room:5#3001"}
```

Revert a **single** field:
```json
{"entity_id": "room:5#3001", "field": "name"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity to revert |
| `field` | string | ✗ | If provided, revert only this field; if omitted, revert all |

**Response:** `Editor.State` with the updated pending changes after revert.

---

### Sentience.Editor.Request

**Direction:** Client → Server
**When:** Client needs current editor state (e.g., after reconnect, on tab focus)

```json
{"entity_id": "room:5#3001"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity to query |

**Response:** `Editor.State` with current pending changes.

---

### Sentience.Editor.CommitResult

**Direction:** Server → Client
**When:** After a successful `Commit` request

#### Single Entity Result

```json
{
  "entity_id": "room:5#3001",
  "status": "success",
  "changes_applied": 3,
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `entity_id` | string | ✓ | Entity that was committed |
| `status` | string | ✓ | `"success"` or `"error"` |
| `changes_applied` | int | ✓ | Number of changes successfully applied |
| `_v` | int | ✓ | Message version (always `1`) |

#### Group Commit Result (future)

When group commit is implemented, the response will use this format:

```json
{
  "group_id": 1,
  "results": [
    {"entity_id": "room:5#3001", "status": "success", "changes_applied": 2},
    {"entity_id": "mob:5#3005", "status": "success", "changes_applied": 1}
  ],
  "comment": "Rebuilt tavern area",
  "_v": 1
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `group_id` | int | ✓ | Group transaction identifier |
| `results` | array | ✓ | Per-entity commit results |
| `comment` | string | ✗ | Echo of the provided comment (omitted if none) |
| `_v` | int | ✓ | Message version (always `1`) |

---

### Sentience.Editor.StringEdit.Open

**Direction:** Server → Client
**When:** Builder invokes a multiline text editor (e.g., editing a room description)
on a **WebSocket** connection

WebSocket clients receive this instead of entering the traditional modal
line-by-line string editor. The client should open a text editing panel where the
builder can edit freely while continuing to interact with the game.

```json
{
  "session_id": "se_1",
  "entity_id": "room:5#3001",
  "field": "description",
  "value": "The room is dark and musty...",
  "max_length": 4096
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `session_id` | string | ✓ | Unique session identifier in `"se_N"` format (N is an incrementing integer) |
| `entity_id` | string | ✓ | Entity being edited |
| `field` | string | ✓ | Field name being edited |
| `value` | string | ✓ | Current text content (empty string if field was blank) |
| `max_length` | int | ✓ | Maximum allowed text length in characters |

> **Note:** This message does **not** include `_v`. Multiple string edit sessions
> can be open simultaneously (e.g., editing descriptions on different entities).
> Each has a unique `session_id`.

---

### Sentience.Editor.StringEdit.Save

**Direction:** Client → Server
**When:** Builder finishes editing text and submits the result

```json
{
  "session_id": "se_1",
  "value": "The room glows with an ethereal light..."
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `session_id` | string | ✓ | Session ID from the `StringEdit.Open` message |
| `value` | string | ✓ | New text content |

**Behavior:**
- In **staged mode** (editor uses changesets): the text is stored as an
  `OLC_FIELD_MULTILINE` pending change — not applied to the entity until commit.
- In **direct mode**: the text is written to the entity immediately.

**Response:** `Editor.StringEdit.Close` with `status: "saved"`.
The session is destroyed after this message.

---

### Sentience.Editor.StringEdit.Cancel

**Direction:** Client → Server
**When:** Builder cancels text editing without saving

```json
{
  "session_id": "se_1"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `session_id` | string | ✓ | Session ID from the `StringEdit.Open` message |

**Response:** `Editor.StringEdit.Close` with `status: "cancelled"`.
The session is destroyed after this message.

---

### Sentience.Editor.StringEdit.Close

**Direction:** Server → Client
**When:** After a `StringEdit.Save` or `StringEdit.Cancel` is processed

```json
{
  "session_id": "se_1",
  "status": "saved"
}
```

| Field | Type | Always | Description |
|-------|------|--------|-------------|
| `session_id` | string | ✓ | Session that was closed |
| `status` | string | ✓ | `"saved"`, `"cancelled"`, or `"done"` |

> **Note:** This message does **not** include `_v`.
> The client should close the text editing panel for this `session_id`.

---

### Sentience.Editor.Draft.Save

**Direction:** Client → Server
**When:** Client explicitly saves the current changeset as a draft to disk

```json
{"entity_id": "room:5#3001"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity whose changeset to save |

Drafts are saved per-builder per-entity at
`data/drafts/{character_name}/{editor_type}_{auid}_{vnum}.json`.
They survive server restarts and disconnects. The server also **auto-saves**
drafts when a builder disconnects with pending changes.

**Response:** `Editor.State` on success, `Editor.Error` (code `"no_changes"`
or `"save_failed"`) on failure.

---

### Sentience.Editor.Draft.Load

**Direction:** Client → Server
**When:** Client wants to restore a previously saved draft

```json
{"entity_id": "room:5#3001"}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity whose draft to load |

Replaces the current (possibly empty) changeset with the draft's contents.

**Response:** `Editor.State` with `draft_restored: true` on success,
`Editor.Error` (code `"no_draft"` or `"load_failed"`) on failure.

> **Note:** Drafts are also restored **automatically** when entering an editor
> if a saved draft exists for that entity. The initial `Editor.State` message
> will have `draft_restored: true` in that case.

---

### Editor Client Implementation Guide

This section provides guidance for web client developers consuming the Editor
GMCP package.

#### Lifecycle

1. **Builder enters editor** (via MUD command like `redit`):
   - Server sends `Editor.State` with `pending_count: 0` and empty `changes[]`
   - If a saved draft exists, it is auto-restored: `draft_restored: true` with
     the draft's changes populated
   - Client should open an editor panel

2. **Builder makes changes** (via `Editor.Set` or MUD commands):
   - Server sends `Editor.Field` for each changed field
   - Client updates the field display and tracks `is_pending` state

3. **Builder commits** (via `Editor.Commit`):
   - Server applies all changes atomically and sends `Editor.CommitResult`
   - The editor remains open with a clean changeset — builder can continue editing

4. **Builder exits** (via `done` command):
   - Server sends `Editor.Close` with `reason: "done"`
   - Client should close the editor panel

5. **Disconnect with pending changes:**
   - Server auto-saves a draft and sends `Editor.Close` with `reason: "disconnect"`
   - On next editor entry, the draft is auto-restored

#### Recommended UI Architecture

```
┌─────────────────────────────────────────────┐
│  Editor Panel (opened on Editor.State)       │
│ ┌─────────────────────────────────────────┐ │
│ │ Entity: room:5#3001 - "A Dark Cave"     │ │
│ ├─────────────────────────────────────────┤ │
│ │ Name: [A Glowing Cavern        ] ● (*)  │ │
│ │ Description: [Click to edit...  ] ●     │ │
│ │ Sector: [cave ▾]                        │ │
│ │ Heal Rate: [200] ● (*)                  │ │
│ │ Mana Rate: [100]                        │ │
│ ├─────────────────────────────────────────┤ │
│ │ Pending: 2 changes                      │ │
│ │ [Commit]  [Revert All]  [Save Draft]    │ │
│ └─────────────────────────────────────────┘ │
│                                             │
│  (*) = is_pending indicator                 │
│  ● = field has been modified                │
└─────────────────────────────────────────────┘
```

- Show a **pending indicator** (dot, highlight, badge) on fields where
  `is_pending` is `true`
- Show a **pending count** from `Editor.State.pending_count`
- **Commit** button sends `Editor.Commit`
- **Revert All** button sends `Editor.Revert` (no `field`)
- Per-field **revert** sends `Editor.Revert` with the `field` name
- **Save Draft** button sends `Editor.Draft.Save`
- Multiline fields should open a `StringEdit` panel on click

#### StringEdit Panel

When the server sends `StringEdit.Open`, open a dedicated text editing panel:

```
┌──────────────────────────────────────┐
│ Editing: description (se_1)          │
│ ┌──────────────────────────────────┐ │
│ │ The room glows with an ethereal  │ │
│ │ light that seems to emanate from │ │
│ │ the crystalline walls...         │ │
│ │                                  │ │
│ └──────────────────────────────────┘ │
│ 156 / 4096 chars                     │
│ [Save]  [Cancel]                     │
└──────────────────────────────────────┘
```

- Display `max_length` as a character limit indicator
- **Save** sends `StringEdit.Save` with the new text
- **Cancel** sends `StringEdit.Cancel`
- Close the panel when `StringEdit.Close` is received
- Multiple panels can be open (use `session_id` to track)

#### Error Handling

Display `Editor.Error` messages to the builder. The `error` code can be used
for programmatic handling (e.g., highlighting an invalid field), while `message`
provides human-readable text for display.

#### Staging Limits

The server enforces limits on pending changes:
- **Per entity:** 100 pending changes maximum
- **Per builder:** 500 total pending changes across all entities

When limits are reached, `Editor.Error` with code `"limit_reached"` is sent.
The client should indicate this to the builder and suggest committing or
reverting some changes.

---

### Sentience.Auth.Resume

See [Authentication & Session Resume](#authentication--session-resume) for the
complete flow. Summary of events:

| Event     | Direction | Payload                                                    |
|-----------|-----------|------------------------------------------------------------|
| `token`   | S → C     | `{"event":"token","token":"<hex>","ttl":180}`              |
| `pending` | S → C     | `{"event":"pending"}`                                      |
| `ok`      | S → C     | `{"event":"ok"}`                                           |
| `fail`    | S → C     | `{"event":"fail","reason":"invalid_or_expired"}`           |
| (resume)  | C → S     | Plain text: `RESUME <token>` (not GMCP — sent as input)    |

---

### Sentience.Auth.QRCode

**Direction:** Server → Client
**When:** During MFA TOTP setup flow (account or character level)
**WebSocket only:** Telnet clients receive an ASCII QR code instead

Delivers a QR code as a PNG image encoded in base64, allowing the web client
to display a scannable QR code during authenticator app setup.

#### Server → Client

```json
{
  "_v": 1,
  "purpose": "totp_setup",
  "image": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUg...",
  "uri": "otpauth://totp/Sentience:PlayerName?secret=BASE32SECRET&issuer=Sentience",
  "expires_at": 0
}
```

| Field        | Type    | Description                                                |
|--------------|---------|------------------------------------------------------------|
| `purpose`    | string  | Always `"totp_setup"` (reserved for future use)            |
| `image`      | string  | PNG image as a `data:image/png;base64,...` data URL         |
| `uri`        | string  | The `otpauth://` URI for manual entry or copy/paste        |
| `expires_at` | integer | Unix timestamp when QR expires, or `0` for no expiry       |

#### Client Implementation

```jsx
// React example
<img src={data.image} alt="Scan this QR code with your authenticator app" />
<p>Or enter this URI manually: <code>{data.uri}</code></p>
```

The `image` field is a complete data URL — assign it directly to an `<img>` tag's
`src` attribute. No decoding required.

#### Notes

- Only sent to WebSocket clients. Telnet clients see an ASCII art QR code.
- The QR code is generated server-side using libqrencode + libpng.
- Appears during the MFA setup menu flow (both account-level and character-level).
- The `uri` field can be used as a fallback for users who cannot scan QR codes.

---

## Update Lifecycle

### Server Update Loop

The server runs a main game loop at **4 pulses per second**. Each pulse, for
every connected descriptor with GMCP support:

1. **Cache comparison** — Current character/room state is compared against the
   cached values from the last GMCP send.
2. **Dirty detection** — Changed fields generate dirty flags.
3. **Package sends** — Only changed packages are sent.
4. **Cache update** — Sent values are cached to prevent redundant sends.

### Dirty Flags

| Flag                     | Bit  | Triggers                                        |
|--------------------------|------|-------------------------------------------------|
| `SENTIENCE_DIRTY_IDENTITY` | 0  | Level, name, or class change                    |
| `SENTIENCE_DIRTY_VITALS`   | 1  | HP, mana, or move change                        |
| `SENTIENCE_DIRTY_STATS`    | 2  | Stat, hitroll, damroll, or wimpy change          |
| `SENTIENCE_DIRTY_COMBAT`   | 3  | Armor class change                               |
| `SENTIENCE_DIRTY_WORTH`    | 4  | XP, gold, alignment, or practices change         |
| `SENTIENCE_DIRTY_ROOM`     | 5  | Room change (also triggers Map + Contents)       |
| `SENTIENCE_DIRTY_AFFECTS`  | 6  | Affect list modification                         |
| `SENTIENCE_DIRTY_ENEMIES`  | 7  | Combat state change                              |
| `SENTIENCE_DIRTY_INVENTORY`  | 8  | Item count change in carried items             |
| `SENTIENCE_DIRTY_EQUIPMENT`  | 9  | Worn item count change                         |
| `SENTIENCE_DIRTY_ABILITIES`  | 10 | Skill/spell/song list change                   |
| `SENTIENCE_DIRTY_REPUTATIONS`| 11 | Visible reputation count change                |
| `SENTIENCE_DIRTY_CHURCH`     | 12 | Church membership or church UID change         |
| `SENTIENCE_DIRTY_RACE`       | 13 | Race UID change                                |

### First Update (Login Burst)

On first update after login, the server forces all dirty flags to `0x3FFF`
(all 14 flags), ensuring every package is sent. This includes:

- `Sentience.Client.Ready.State` (one-shot, never sent again)
- `Sentience.Client.Preferences` (one-shot, echoed on changes)
- `Sentience.Client.Layout` restore (WebSocket only, if active layout exists)
- All `Char.*` packages
- All `Room.*` packages

### Transition Detection

For `Char.Affects` and `Char.Enemies`, the server tracks transitions:

- **Affects:** If the character had affects last update but has none now, an
  empty `affects` array is sent (so the client can clear its affect display).
- **Enemies:** If the character was fighting last update but is no longer, an
  empty `enemies` array is sent (so the client can exit combat mode).

### Room Contents Fingerprinting

Room.Contents does not use per-entity dirty tracking. Instead, it uses a
fingerprint: `(room_id, total_visible_entity_count)`. When the fingerprint
changes, the entire contents list is resent.

---

## Color Code Reference

MUD color codes appear in text frames and some GMCP fields (Room.Map, Link text,
Identity title). The client must parse these into rendered colors.

### Standard Color Codes

| Code  | Color              | Code  | Color               |
|-------|--------------------|-------|---------------------|
| `{x`  | Reset/default      | `{X`  | Reset (bold off)    |
| `{r`  | Dark red           | `{R`  | Bright red          |
| `{g`  | Dark green         | `{G`  | Bright green        |
| `{b`  | Dark blue          | `{B`  | Bright blue         |
| `{y`  | Dark yellow/brown  | `{Y`  | Bright yellow       |
| `{m`  | Dark magenta       | `{M`  | Bright magenta      |
| `{c`  | Dark cyan          | `{C`  | Bright cyan         |
| `{w`  | Gray (dark white)  | `{W`  | Bright white        |
| `{d`  | Dark gray          | `{D`  | Dark gray (bold)    |
| `{J`  | Custom/special     |       |                     |

> Lowercase = normal intensity, Uppercase = bright/bold.

### OSC 8 Hyperlinks

Text may contain OSC 8 escape sequences for links:

```
ESC]8;;<link_id>TEXT_CONTENTESC]8;;
```

- `ESC` is the escape character (0x1B or `␛`)
- `link_id` maps to an entry in the preceding `Sentience.Link.List`
- Empty `link_id` in the closing sequence terminates the link

### MXP/Send Tags

Some staff-facing output may contain legacy MXP tags:

```
<send href='command' hint='tooltip'>text</send>
```

These should be parsed into clickable elements or stripped for display.

---

## Client Implementation Notes

### Recommended Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Web Client                            │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │  Vitals   │  │  Stats   │  │ Affects  │  │ Combat │  │
│  │  Panel    │  │  Panel   │  │  Panel   │  │ Panel  │  │
│  └──────────┘  └──────────┘  └──────────┘  └────────┘  │
│                                                          │
│  ┌────────────────────┐  ┌───────────────────────────┐  │
│  │    Room Map        │  │     Main Text Output       │  │
│  │    (minimap)       │  │     (with links)           │  │
│  └────────────────────┘  │                            │  │
│                          │                            │  │
│  ┌────────────────────┐  │                            │  │
│  │  Room Contents     │  │                            │  │
│  │  (items/NPCs)      │  └───────────────────────────┘  │
│  └────────────────────┘                                  │
│                                                          │
│  ┌────────────────────┐  ┌───────────────────────────┐  │
│  │  Channel Tabs      │  │     Command Input          │  │
│  │  (gossip/say/etc.) │  │                            │  │
│  └────────────────────┘  └───────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### GMCP Message Parsing

```javascript
function parseGMCP(frame) {
  const spaceIndex = frame.indexOf(' ');
  if (spaceIndex === -1) return { package: frame, data: null };

  const packageName = frame.substring(0, spaceIndex);
  const jsonStr = frame.substring(spaceIndex + 1);
  const data = JSON.parse(jsonStr);

  return { package: packageName, data };
}
```

### Link Processing Pipeline

```javascript
// 1. Store links from Sentience.Link.List
let currentLinks = {};
function handleLinkList(links) {
  currentLinks = {};
  for (const link of links) {
    currentLinks[link.id] = link;
  }
}

// 2. Parse OSC 8 sequences in text
const OSC8_REGEX = /\x1b\]8;;([^\x1b]*)\x1b\\?(.*?)\x1b\]8;;\x1b\\?/g;

function processText(text) {
  return text.replace(OSC8_REGEX, (match, linkId, displayText) => {
    const link = currentLinks[linkId];
    if (link) {
      // Render as interactive element with context menu from link.actions
      return renderLink(link, displayText);
    }
    return displayText;
  });
}

// 3. Clear links after processing
function afterTextFrame() {
  currentLinks = {};
}
```

### Resume Token Management

```javascript
class ResumeManager {
  constructor() {
    this.token = null;
    this.expiresAt = null;
  }

  handleResume(data) {
    switch (data.event) {
      case 'token':
        this.token = data.token;
        this.expiresAt = Date.now() + (data.ttl * 1000);
        this.persist(); // Save to localStorage
        break;
      case 'pending':
        // Show "Reconnecting..." UI
        break;
      case 'ok':
        // Resume succeeded — character data burst incoming
        break;
      case 'fail':
        this.token = null;
        // Fall back to login prompt
        break;
    }
  }

  getResumeCommand() {
    if (this.token && Date.now() < this.expiresAt) {
      return `RESUME ${this.token}`;
    }
    return null;
  }
}
```

### Preference Synchronization

```javascript
// On login, receive structured preferences
function handlePreferences(data) {
  for (const pref of data.preferences) {
    store.setPreference(pref.key, pref.value, pref.source);
  }
}

// When user changes a preference in the UI
function setSuppressChannels(value) {
  sendGMCP('Sentience.Client.Preferences', {
    action: 'set',
    key: 'gmcp_suppress_channels',
    value: value,
    scope: 'character'
  });
  // Server will echo back the full preference state
}

// Reset a preference to its default
function resetPreference(key) {
  sendGMCP('Sentience.Client.Preferences', {
    action: 'reset',
    key: key
  });
}
```

### Affect Duration Countdown

```javascript
function startAffectCountdown(affect) {
  if (affect.estimated_seconds < 0) {
    return 'permanent'; // -1 = permanent
  }

  let remaining = affect.estimated_seconds;
  const interval = setInterval(() => {
    remaining--;
    updateAffectDisplay(affect.name, remaining);
    if (remaining <= 0) {
      clearInterval(interval);
      // Note: server will send updated Char.Affects when it actually expires
    }
  }, 1000);
}
```

### Instance IDs

Many entities use 64-bit instance IDs represented as `[low, high]` two-element
arrays. These uniquely identify entity instances in the game world:

```javascript
// Create a string key from instance_id for use in Maps/Sets
function instanceKey(id) {
  return `${id[0]}:${id[1]}`;
}

// Compare two instance IDs
function sameInstance(a, b) {
  return a[0] === b[0] && a[1] === b[1];
}
```

### Wide-Vnums (wnum)

Many identifiers use the "wide-vnum" format: `area#vnum`

```javascript
function parseWnum(wnum) {
  const [area, vnum] = wnum.split('#');
  return { area: parseInt(area), vnum: parseInt(vnum) };
}
```

---

## Appendix: Complete Package Summary

| Package | Direction | Frequency | Notes |
|---------|-----------|-----------|-------|
| Client.Ready.Capabilities | S→C | Once (handshake) | Lists all server packages |
| Client.Ready.State | S→C | Once (first update) | Timing constants |
| Client.Preferences | Bidirectional | On change | Preference sync |
| Client.Layout | Bidirectional | On save/load/delete/list | FlexLayout persistence |
| Char.Identity | S→C | On change | Level/name/class |
| Char.Vitals | S→C | On change | HP/mana/move (most frequent) |
| Char.Stats | S→C | On change | Attributes |
| Char.Combat | S→C | On change | Armor class |
| Char.Worth | S→C | On change | XP/gold |
| Char.Affects | S→C | On change | Spell effects + durations |
| Char.Enemies | S→C | On change | Combat targets |
| Char.Inventory | S→C | On change | Items carried + capacity |
| Char.Equipment | S→C | On change | All wear slots + items |
| Char.Abilities | S→C | On change | Skills, spells, songs |
| Char.Reputations | S→C | On change | Faction standings |
| Char.Church | S→C | On change | Church membership |
| Char.Race | S→C | On race change | Race details (rare) |
| Room.Info | S→C | On room change | Room details + exits |
| Room.Contents | S→C | On fingerprint change | Items/NPCs/players/doors |
| Room.Map | S→C | On room change | Pre-rendered minimap |
| Channel.Message | S→C | On message | Chat channels + tells |
| Link.List | S→C | Per text frame | Interactive link metadata |
| Editor.State | S→C | On state change | Revert/draft/request/entry |
| Editor.Field | S→C | On field change | Individual field update |
| Editor.Close | S→C | On editor exit | Session cleanup |
| Editor.Error | S→C | On validation failure | Error code + message |
| Editor.Set | C→S | On field edit | Stage a change |
| Editor.Commit | C→S | On submit | Apply all pending changes |
| Editor.Revert | C→S | On discard | Revert one or all fields |
| Editor.Request | C→S | On reconnect | Request current state |
| Editor.CommitResult | S→C | After commit | Changes applied count |
| Editor.StringEdit.Open | S→C | On multiline edit | Non-blocking text editor (WS) |
| Editor.StringEdit.Save | C→S | On text submit | Save edited text |
| Editor.StringEdit.Cancel | C→S | On text cancel | Discard text edits |
| Editor.StringEdit.Close | S→C | After save/cancel | Close text editor panel |
| Editor.Draft.Save | C→S | Explicit save | Persist changeset to disk |
| Editor.Draft.Load | C→S | Explicit load | Restore saved changeset |
| Auth.Resume | Bidirectional | On connect/disconnect | Session resume tokens |
| Auth.QRCode | S→C | MFA TOTP setup | QR code as PNG data URL (WS only) |

> All `Sentience.*` packages are prefixed with `Sentience.` in the GMCP frame.
> Example: `Sentience.Char.Vitals {...}`
