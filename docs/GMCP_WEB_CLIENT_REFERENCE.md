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
   - [Sentience.Char.Identity](#sentiencecharidentity)
   - [Sentience.Char.Vitals](#sentiencecharvitals)
   - [Sentience.Char.Stats](#sentiencecharstats)
   - [Sentience.Char.Combat](#sentiencecharcombat)
   - [Sentience.Char.Worth](#sentiencecharworth)
   - [Sentience.Char.Affects](#sentiencecharaffects)
   - [Sentience.Char.Enemies](#sentiencecharenemies)
   - [Sentience.Room.Info](#sentienceroominfo)
   - [Sentience.Room.Contents](#sentienceroomcontents)
   - [Sentience.Room.Map](#sentienceroommap)
   - [Sentience.Channel.Message](#sentiencechannelmessage)
   - [Sentience.Link.List](#sentiencelinklklist)
   - [Sentience.Auth.Resume](#sentienceauthresume)
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

Sent on login and after any preference change:

```json
{
  "_v": 1,
  "gmcp_channels": true,
  "gmcp_suppress_channels": false,
  "gmcp_suppress_minimap": false
}
```

| Field                    | Type | Default (WS) | Description                                              |
|--------------------------|------|---------------|----------------------------------------------------------|
| `gmcp_channels`          | bool | `true`        | Enable `Sentience.Channel.Message` delivery              |
| `gmcp_suppress_channels` | bool | `false`       | Suppress inline channel text when GMCP channels enabled  |
| `gmcp_suppress_minimap`  | bool | `false`       | Suppress inline ASCII minimap in room display            |

#### Client → Server (partial update)

Send only the preferences you want to change:

```json
Sentience.Client.Preferences {"gmcp_suppress_channels": true}
```

The server validates against the known preference whitelist, applies changes,
persists them, and echoes the full preference state back.

**Invalid keys are silently ignored.**

#### Preference Behavior

- **`gmcp_channels` + `gmcp_suppress_channels`:** When both are `true`, channel
  messages are delivered ONLY via GMCP (no inline text). When `gmcp_channels` is
  `true` but `gmcp_suppress_channels` is `false`, you get both GMCP and inline.
- **`gmcp_suppress_minimap`:** When `true`, the server does not send
  `Sentience.Room.Map` and does not include inline ASCII maps in room text.
- Preferences are persisted per-character and survive logout/reconnect.

---

### Sentience.Char.Identity

**Direction:** Server → Client
**When:** Login, level change, name change, class change

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
      "is_primary": true
    }
  ]
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
| `classes`   | object[] | Array of class objects                   |

#### Class Object

| Field        | Type   | Description                    |
|--------------|--------|--------------------------------|
| `id`         | string | Class identifier               |
| `name`       | string | Class display name             |
| `level`      | int    | Level in this class            |
| `is_primary` | bool   | Whether this is the active class |

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
  "timestamp": 1711382400
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
  "tell_target": "Merlin"
}
```

| Field         | Type        | Description                                        |
|---------------|-------------|----------------------------------------------------|
| `channel`     | string      | Channel ID (gossip, say, tell, auction, etc.)      |
| `sender`      | string      | Sender name (empty string for system messages)     |
| `text`        | string      | Message text (**color codes stripped**)             |
| `timestamp`   | long        | Unix timestamp                                     |
| `tell_target` | string/null | Recipient name for directed messages (tells only)  |

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

### First Update (Login Burst)

On first update after login, the server forces all dirty flags to `0xFF`,
ensuring every package is sent. This includes:

- `Sentience.Client.Ready.State` (one-shot, never sent again)
- `Sentience.Client.Preferences` (one-shot, echoed on changes)
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
// On login, receive server preferences
function handlePreferences(prefs) {
  store.setGmcpChannels(prefs.gmcp_channels);
  store.setSuppressChannels(prefs.gmcp_suppress_channels);
  store.setSuppressMinimap(prefs.gmcp_suppress_minimap);
}

// When user changes a preference in the UI
function toggleSuppressChannels(value) {
  sendGMCP('Sentience.Client.Preferences', {
    gmcp_suppress_channels: value
  });
  // Server will echo back the full state
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
| Char.Identity | S→C | On change | Level/name/class |
| Char.Vitals | S→C | On change | HP/mana/move (most frequent) |
| Char.Stats | S→C | On change | Attributes |
| Char.Combat | S→C | On change | Armor class |
| Char.Worth | S→C | On change | XP/gold |
| Char.Affects | S→C | On change | Spell effects + durations |
| Char.Enemies | S→C | On change | Combat targets |
| Room.Info | S→C | On room change | Room details + exits |
| Room.Contents | S→C | On fingerprint change | Items/NPCs/players/doors |
| Room.Map | S→C | On room change | Pre-rendered minimap |
| Channel.Message | S→C | On message | Chat channels + tells |
| Link.List | S→C | Per text frame | Interactive link metadata |
| Auth.Resume | Bidirectional | On connect/disconnect | Session resume tokens |

> All `Sentience.*` packages are prefixed with `Sentience.` in the GMCP frame.
> Example: `Sentience.Char.Vitals {...}`
