# GMCP Phase 3: Extended Packages — Design Spec

**Date:** 2026-03-25
**Status:** Draft
**Depends on:** Phase 1 (Sentience.* foundation), Phase 2 (Sentience.Link)

## Overview

Phase 3 extends the Sentience GMCP namespace with four new packages:

1. **Sentience.Client.Ready** — two-stage capability/state announcement
2. **Sentience.Char.Affects** — active spell/effect list with durations
3. **Sentience.Char.Enemies** — combat threat list with targeting info
4. **Sentience.Room.Contents** — items, NPCs, players, and door states in the current room

All builders follow the Phase 1 pattern: pure functions taking explicit input structs, returning `json_t*` with `"_v": 1` versioning. Implementation extends `gmcp_sentience.h/c`.

## Architecture

**Approach:** Extend the existing `gmcp_sentience.c` file with 4 new builder functions and update logic in `sentience_gmcp_update()`. No new files required — follows the established Phase 1 single-file pattern.

**Update strategy (hybrid):**

| Package | Strategy | Rationale |
|---------|----------|-----------|
| Client.Ready.Capabilities | One-shot on GMCP negotiation | Static server info, never changes |
| Client.Ready.State | One-shot on game entry | Provides tick rate for client timers |
| Char.Affects | Always-send each cycle when affects exist | Changes frequently in combat, small payload (5-15 entries) |
| Char.Enemies | Always-send each cycle when fighting | Changes every combat round |
| Room.Contents | Dirty flag + content fingerprint | Full snapshot on room change; re-snapshot within room when entity count changes |

## Package Specifications

### 1. Sentience.Client.Ready.Capabilities

**Direction:** Server → Client
**Trigger:** GMCP negotiation completes with `GMCP_SUPPORT_SENTIENCE` enabled
**Location:** `protocol.c` (where `bGMCPSupport[GMCP_SUPPORT_SENTIENCE]` is set)

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
    "Sentience.Link"
  ],
  "features": ["links", "osc8"]
}
```

**Fields:**
- `packages`: array of all available Sentience GMCP package names
- `features`: array of supported feature flags (transport capabilities)

### 2. Sentience.Client.Ready.State

**Direction:** Server → Client
**Trigger:** Character enters the game world (post-login, after character selection)
**Location:** `sentience_gmcp_update()` first-run path (when `!cache->initialized`)

```json
{
  "_v": 1,
  "tick_rate": 4,
  "pulse_per_second": 4
}
```

**Fields:**
- `tick_rate`: game ticks per real-time period, for client-side duration conversion
- `pulse_per_second`: pulses per second, allowing precise timer calculations

The client uses these to convert affect tick durations to real-time countdowns: `seconds = ticks * (PULSE_TICK / PULSE_PER_SECOND)`.

### 3. Sentience.Char.Affects

**Direction:** Server → Client
**Trigger:** Every update cycle when character has affects, or once when last affect removed
**Location:** `sentience_gmcp_update()` — no dirty tracking, always-send during combat

```json
{
  "_v": 1,
  "affects": [
    {
      "name": "armor",
      "wnum": "1:53",
      "duration": 12,
      "estimated_seconds": 48,
      "modifier": "-20 armor class",
      "level": 25
    },
    {
      "name": "bless",
      "wnum": "1:67",
      "duration": -1,
      "estimated_seconds": -1,
      "modifier": "+1 hitroll",
      "level": 30
    }
  ]
}
```

**Fields per affect:**
- `name`: string — affect name from `af->skill->name` or `af->custom_name`, fallback `"unknown"`
- `wnum`: string or null — widevnum from `af->skill->wnum` if skill pointer set, null otherwise
- `duration`: int — remaining ticks, -1 for permanent affects
- `estimated_seconds`: int — `duration * PULSE_TICK / PULSE_PER_SECOND`, -1 if permanent
- `modifier`: string — human-readable modifier description (e.g., `"+2 strength"`, `"-20 armor class"`), built from `affect_loc_name(af->location)` + `af->modifier`
- `level`: int — spell/skill level that created this affect

**Edge cases:**
- Empty `affects: []` sent once when last affect is removed (signals client to clear display)
- Affects with `af->location == APPLY_NONE` and `af->modifier == 0` get `modifier: null`
- Multiple affects from the same spell appear as separate entries (the client may group them)

### 4. Sentience.Char.Enemies

**Direction:** Server → Client
**Trigger:** Every update cycle when in combat, or once when combat ends
**Location:** `sentience_gmcp_update()` — no dirty tracking, always-send during combat

```json
{
  "_v": 1,
  "enemies": [
    {
      "name": "a huge troll",
      "instance_id": [3, 42],
      "hp_pct": 73,
      "is_primary": true,
      "target": "you"
    },
    {
      "name": "a goblin warrior",
      "instance_id": [3, 108],
      "hp_pct": 45,
      "is_primary": false,
      "target": "Gandalf"
    }
  ],
  "self": {
    "hp": 450,
    "max_hp": 600,
    "hp_pct": 75
  }
}
```

**Fields per enemy:**
- `name`: string — short description for NPCs, character name for players
- `instance_id`: array of 2 longs — `[id[0], id[1]]` for unique identification
- `hp_pct`: int (0-100) — health percentage, computed as `(hp * 100) / UMAX(1, max_hp)`
- `is_primary`: bool — true if this is `ch->fighting` (who the player is attacking)
- `target`: string — `"you"` if attacking the player, otherwise the target's name

**`self` block:**
- `hp`, `max_hp`: exact values for the player character
- `hp_pct`: percentage for consistency

**Enemy enumeration:**
1. Start with `ch->fighting` (primary target, `is_primary: true`)
2. Iterate `room->people` for anyone whose `fighting == ch` and not already listed
3. All non-primary enemies get `is_primary: false`

**Health display rules:**
- NPCs: `hp_pct` only (no exact HP — prevents cheat advantage)
- Player characters (other players): `hp_pct` only
- Self: exact `hp` and `max_hp` in the `self` block

**Edge cases:**
- Empty `enemies: []` with `self: null` sent once when combat ends (all fighting stops)
- If `ch->fighting` is dead/invalid, skip and only include attackers
- Charmed/group NPCs attacking someone else in the room are NOT included (only direct threats)

### 5. Sentience.Room.Contents

**Direction:** Server → Client
**Trigger:** Full snapshot on room change; re-snapshot within room when content fingerprint changes
**Location:** `sentience_gmcp_update()` — dirty flag with cached room ID and entity counts

#### Full Snapshot: `Sentience.Room.Contents`

```json
{
  "_v": 1,
  "items": [
    {
      "name": "a gleaming sword",
      "instance_id": [2, 15],
      "short_desc": "A gleaming sword lies here."
    }
  ],
  "npcs": [
    {
      "name": "a huge troll",
      "instance_id": [3, 42],
      "short_desc": "A huge troll stands here, looking mean."
    }
  ],
  "players": [
    {
      "name": "Gandalf"
    }
  ],
  "doors": [
    {
      "direction": "north",
      "state": "closed",
      "is_locked": false
    },
    {
      "direction": "east",
      "state": "open",
      "is_locked": false
    }
  ]
}
```

**Fields — items array:**
- `name`: string — object keyword list (first keyword)
- `instance_id`: array of 2 longs — `[id[0], id[1]]`
- `short_desc`: string — room-visible description

**Fields — npcs array:**
- `name`: string — NPC name/keywords
- `instance_id`: array of 2 longs
- `short_desc`: string — long description (room display)

**Fields — players array:**
- `name`: string — player character name

**Fields — doors array:**
- `direction`: string — exit direction name (`"north"`, `"south"`, etc.)
- `state`: string — `"open"`, `"closed"`, or `"locked"`
- `is_locked`: bool — whether the door is locked

**Visibility rules:**
- Items: only visible objects (not hidden/buried unless player can detect)
- NPCs: only visible NPCs (respect `can_see()`)
- Players: only visible players (respect `can_see()`)
- Doors: all exits that have doors, regardless of visibility (doors are structural room geometry — their existence is not hidden, only their locked/trapped state may be non-obvious; clients need door data for map rendering)

#### Differential Events (protocol defined, implementation deferred)

These events are part of the protocol specification for future implementation. The initial implementation uses full snapshots with content fingerprinting instead.

- `Sentience.Room.Contents.Add` — `{ "type": "npc"|"item"|"player", "name": "...", "instance_id": [...] }`
- `Sentience.Room.Contents.Remove` — `{ "type": "npc"|"item"|"player", "instance_id": [...] }` (or `"name"` for players)
- `Sentience.Room.Contents.DoorState` — `{ "direction": "north", "state": "locked" }`

#### Content Fingerprint

The dirty-tracking cache stores:
- `room_id[2]`: the room's widevnum (for detecting room changes)
- `content_count`: total count of `items + npcs + players + doors` (for detecting in-room changes)

On each update cycle:
1. If room ID changed → full snapshot, update cache
2. Else if content count changed → full snapshot, update cache
3. Else → no send (nothing changed)

This is an approximation — if a mob leaves and another enters in the same tick, the count stays the same and no update fires. This is acceptable; true differential events will fix this when implemented.

## Input Structures

Following the Phase 1 pattern, each builder takes an explicit input struct:

```c
/* Affects */
typedef struct {
    const char *name;
    const char *wnum;       /* NULL if no skill pointer */
    int duration;           /* -1 for permanent */
    int estimated_seconds;  /* -1 for permanent */
    const char *modifier;   /* NULL if APPLY_NONE with 0 modifier */
    int level;
} sentience_affect_input_t;

/* Enemies */
typedef struct {
    const char *name;
    long instance_id[2];
    int hp_pct;
    bool is_primary;
    const char *target;
} sentience_enemy_input_t;

/* Room content entity */
typedef struct {
    const char *name;
    long instance_id[2];
    const char *short_desc;  /* NULL for players */
} sentience_room_entity_input_t;

/* Room door */
typedef struct {
    const char *direction;
    const char *state;       /* "open", "closed", "locked" */
    bool is_locked;
} sentience_room_door_input_t;

/* Room contents (full snapshot) */
typedef struct {
    const sentience_room_entity_input_t *items;
    int num_items;
    const sentience_room_entity_input_t *npcs;
    int num_npcs;
    const sentience_room_entity_input_t *players;
    int num_players;
    const sentience_room_door_input_t *doors;
    int num_doors;
} sentience_room_contents_input_t;
```

## Builder Function Signatures

```c
/* Client.Ready */
json_t *sentience_build_client_ready_capabilities_json(void);
/* ^ No parameters — intentionally hard-codes the package list and features.
 *   The available packages are static server configuration, not per-connection.
 *   If per-connection features are needed later, the signature can be extended. */
json_t *sentience_build_client_ready_state_json(int tick_rate, int pulse_per_second);

/* Char.Affects */
json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects);

/* Char.Enemies */
json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp);

/* Room.Contents */
json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data);
```

All return `json_t*` with `"_v": 1`. Caller must `json_decref()`.

## Dirty Flags

No new dirty flag constants are needed for Phase 3. `SENTIENCE_DIRTY_AFFECTS` and `SENTIENCE_DIRTY_ENEMIES` are already reserved in `gmcp_sentience.h` (Phase 1) but are **not used** by the hybrid update strategy — affects and enemies use always-send + transition tracking instead. Room.Contents uses fingerprint comparison (room ID + entity count) rather than a dirty bitmask.

The reserved `SENTIENCE_DIRTY_AFFECTS` and `SENTIENCE_DIRTY_ENEMIES` constants remain for potential future use (e.g., if we switch to dirty-flag tracking later) but are not referenced by Phase 3 code.

## Cache Extensions

Add to `sentience_gmcp_cache_t`:

```c
/* Room contents fingerprint — separate from Phase 1's room_id0/room_id1 (used for Room.Info dirty tracking).
 * Phase 1 uses two separate longs; Phase 3 uses an array for cleaner comparison. Both track the same
 * widevnum concept (area->uid, room->vnum) but serve different purposes. */
long contents_room_id[2];   /* [0]=area->uid, [1]=room->vnum */
int contents_count;          /* item + npc + player + door count */

/* Affects tracking — transition detection for empty-list sends */
bool had_affects;            /* True if last cycle had affects */

/* Combat tracking — transition detection for empty-list sends */
bool was_fighting;           /* True if last cycle was in combat */
```

**First-send semantics:** These fields are zero-initialized by `memset` (via `calloc` in Phase 1). On the first update cycle (`!cache->initialized`):
- `had_affects = false` and `was_fighting = false` — if the character has affects or is fighting on login, the `ch->affected || cache->had_affects` condition triggers correctly (true || false = true)
- `contents_room_id = {0,0}` and `contents_count = 0` — will differ from any real room, so a full Room.Contents snapshot fires on first cycle
- After Phase 1 sets `cache->initialized = true`, subsequent cycles use normal transition logic

## Update Logic

### Execution Order in `sentience_gmcp_update()`

The function is gated by `bGMCPSupport[GMCP_SUPPORT_SENTIENCE]` at entry (same as Phase 1). The execution order within the function is:

1. **Phase 3: Client.Ready.State** — `if (!cache->initialized)` — fires once on first cycle
2. **Phase 1: Dirty-flag checks** — Identity, Vitals, Stats, Combat, Worth, Room
3. **Phase 3: Char.Affects** — always-send with transition tracking
4. **Phase 3: Char.Enemies** — always-send with transition tracking
5. **Phase 3: Room.Contents** — fingerprint comparison
6. **Phase 1: `cache->initialized = true`** — set LAST, after all first-send logic

Phase 3's Client.Ready.State send is inserted **before** the existing Phase 1 block so it shares the `!cache->initialized` gate. The `cache->initialized = true` assignment at the end of the function (existing Phase 1 code) remains unchanged.

### Pseudocode

```
// Entry gate (existing Phase 1)
if (!bGMCPSupport[GMCP_SUPPORT_SENTIENCE]) return;

// --- Phase 3: Client.Ready.State (one-shot, before Phase 1 block) ---
if (!cache->initialized) {
    json = sentience_build_client_ready_state_json(PULSE_TICK, PULSE_PER_SECOND)
    send "Sentience.Client.Ready.State"
}

// --- Phase 1 block (existing, unchanged) ---
// Dirty-flag checks for Identity, Vitals, Stats, Combat, Worth, Room
// On first run (!initialized), all are sent unconditionally
// ...existing Phase 1 code...

// --- Phase 3: Char.Affects (always-send + transition tracking) ---
if (ch->affected || cache->had_affects) {
    build affects array from ch->affected linked list
    send "Sentience.Char.Affects"
    cache->had_affects = (ch->affected != NULL)
}

// --- Phase 3: Char.Enemies (always-send + transition tracking) ---
if (ch->fighting || cache->was_fighting) {
    build enemy list: ch->fighting (primary) + room->people where fighting==ch
    send "Sentience.Char.Enemies"
    cache->was_fighting = (ch->fighting != NULL)
}

// --- Phase 3: Room.Contents (fingerprint comparison) ---
current_room_id[0] = ch->in_room->area->uid
current_room_id[1] = ch->in_room->vnum
current_count = count(visible items + visible npcs + visible players + doors)
if (current_room_id[0] != cache->contents_room_id[0]
    || current_room_id[1] != cache->contents_room_id[1]
    || current_count != cache->contents_count) {
    build room contents from ch->in_room
    send "Sentience.Room.Contents"
    cache->contents_room_id[0] = current_room_id[0]
    cache->contents_room_id[1] = current_room_id[1]
    cache->contents_count = current_count
}

// --- Phase 1 (existing, unchanged): mark initialized ---
cache->initialized = true
```

## Client.Ready Integration Points

Both stages are **in scope for Phase 3 implementation.** They are fired from different locations:

### Capabilities (protocol.c + protocol_websocket.c)

**In scope.** Fired from protocol negotiation — NOT from the `sentience_gmcp_update()` cycle. This happens before login, so there is no character or cache involved.

In the GMCP negotiation handler (`protocol.c`), after setting `bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true`:

```c
json_t *caps = sentience_build_client_ready_capabilities_json();
char *dump = json_dumps(caps, JSON_COMPACT);
SendGMCPRaw(d, "Sentience.Client.Ready.Capabilities", dump);
free(dump);
json_decref(caps);
```

Same pattern in `protocol_websocket.c` for WebSocket auto-enable.

### State (gmcp_sentience.c)

In `sentience_gmcp_update()`, when `!cache->initialized`:

```c
json_t *state = sentience_build_client_ready_state_json(PULSE_TICK, PULSE_PER_SECOND);
// send...
```

## Testing

**New test cases (estimated 14-16):**

| Builder | Test Cases |
|---------|-----------|
| `client_ready_capabilities` | 1: verify package list and features array |
| `client_ready_state` | 1: verify tick_rate and pulse_per_second |
| `affects` | 4: empty list, single affect, multiple affects, permanent duration (-1) |
| `enemies` | 4: no combat (empty), single enemy, multiple enemies with targets, self block values |
| `room_contents` | 4-6: empty room, items only, NPCs only, players only, mixed, doors with states |

All tests are pure builder tests — no game state required. Follow the existing `gmcp_sentience_unit_tests` pattern with JSON test data files.
