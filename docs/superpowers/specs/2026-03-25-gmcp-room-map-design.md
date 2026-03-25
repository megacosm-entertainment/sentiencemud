# Sentience.Room.Map GMCP Package — Design Spec

## Overview

Add a `Sentience.Room.Map` GMCP package that sends the pre-rendered ASCII minimap
as structured data, allowing web clients to display it in a dedicated panel rather
than inline in the text stream.

**Goal**: Ship the existing map output (both area minimap and wilderness grid) as
GMCP data, with inline suppression when the client has a map panel.

## Package Format

```json
Sentience.Room.Map {
  "_v": 1,
  "type": "wilds | area",
  "map_text": "<pre-rendered map with MUD color codes>",
  "width": 21,
  "height": 7
}
```

### Fields

| Field      | Type   | Description |
|------------|--------|-------------|
| `_v`       | int    | Schema version, always `1` |
| `type`     | string | `"area"` for room minimap, `"wilds"` for wilderness grid |
| `map_text` | string | Pre-rendered map using MUD color codes (`{R`, `{B`, etc.) |
| `width`    | int    | Map width in character columns |
| `height`   | int    | Map height in character rows |

### Color Code Convention

The `map_text` uses MUD color codes (not ANSI escapes), consistent with all other
server→client text. The client's existing color parser handles conversion.

- Area maps: `{B` borders, `{M` player, `{Y` rooms, `{G` shops, `{y` doors, etc.
- Wilderness maps: Terrain-specific colors from `WILDS_TERRAIN` data.

### Why MUD Color Codes (Not ANSI)

The map text passes through the same `protocol_process_output()` pipeline as all
other server text. For telnet clients, MUD codes become ANSI. For WebSocket/web
clients, MUD codes arrive raw and the client-side parser converts them. Sending
ANSI directly would bypass the protocol layer and break for clients that handle
colors differently.

## Trigger Conditions

**When to send**: On room change, same dirty-flag as `Sentience.Room.Info`. When
the room_id comparison in `sentience_gmcp_update()` detects a new room, build and
send the map.

**What triggers a room change**:
- Player movement (walk, flee, teleport, recall)
- `look` at a different room (via `look` command with target)
- Login/reconnect to a room

**Not re-sent**: If the player is in the same room and the map hasn't changed
(same room_id as last send), the map is not re-sent. This piggybacks on the
existing Room.Info dirty detection.

## Map Generation

### Area Minimap (type: "area")

Reuse `create_map()` from `act_info.c:7869` which generates a 10×7 character grid:
- Grid center (5,3) = player position marked `@`
- Adjacent rooms shown as single characters via `determine_room_type()`
- Connectors: `|`, `-`, `/`, `\` for cardinal and diagonal exits
- Doors: `#` for closed doors
- Floor indicators: `<` (previous floor), `>` (next floor)

The raw grid is then rendered to colored text via `show_map()` + `convert_map_char()`.

**For GMCP**: Call `create_map()` to get the raw grid, then render it to a string
buffer using `convert_map_char()` per character (same as `show_map()` does for the
inline display). The buffer is the `map_text` value.

**Dimensions**: Area minimap is always 10 columns × 7 rows of map characters,
rendered with color codes and borders to approximately 21 display columns × 7 rows.

### Wilderness Map (type: "wilds")

Reuse `show_map_to_char_wyx()` from `wilds.c:2690+` which renders a terrain grid
centered on the player's wilderness coordinates.

**For GMCP**: Create a `render_wilds_map_to_buffer()` variant that writes to a
string buffer instead of calling `send_to_char()`. The function takes the same
parameters as `show_map_to_char_wyx()`:
- `wilds` pointer, player x/y, center x/y, viewport x/y, and flags.

**Dimensions**: Dynamic based on weather and `wildview_bonus`:
- Width: `2 * squares_to_show_x + 1` (typically 21–33 columns)
- Height: `2 * squares_to_show_y + 1` (typically 11 rows)

The actual width/height are calculated at render time and included in the JSON.

## Inline Suppression

When the server is sending `Sentience.Room.Map` via GMCP, the inline map display
should be suppressed to avoid showing the map twice.

### Suppression Mechanism

Check `d->pProtocol->bGMCP && d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE]`
before displaying the inline map. If true, skip the inline display.

**Suppression points** (both in `act_info.c`):

1. **Area minimap**: In `show_room()` at the `show_map_and_description()` call
   (line ~2604). When suppressed, fall through to `show_room_description()` only.

2. **Wilderness map**: In `show_room()` at the `show_map_to_char_wyx()` call
   (line ~2630). When suppressed, skip the wilderness map display entirely.

### Suppression Does NOT Affect

- Room name, description, exits, contents — these still display inline
- The `look` command explicitly targeting another room
- Staff overrides or builder tools

## Integration with sentience_gmcp_update()

The map builder is called from `sentience_gmcp_update()` in `gmcp_sentience.c`,
inside the existing room-change dirty check (same condition that triggers
`Sentience.Room.Info`).

### Execution Order (within sentience_gmcp_update)

After the existing Room.Info send, add the Room.Map send:

```
if (room changed) {
    build and send Sentience.Room.Info    // existing
    build and send Sentience.Room.Map     // new
    update cached room_id
}
```

The map is built fresh on each room change. No separate dirty flag needed — it
piggybacks on the room_id comparison.

## Builder Function

```c
json_t *sentience_build_room_map(const sentience_room_map_input_t *input);
```

### Input Structure

```c
typedef struct {
    const char *type;       /* "area" or "wilds" */
    const char *map_text;   /* Pre-rendered map string with MUD color codes */
    int width;              /* Character columns */
    int height;             /* Character rows */
} sentience_room_map_input_t;
```

### Builder Logic

Simple field-to-JSON mapping — no complex logic. The map generation functions
produce the map_text; the builder just wraps it in the JSON envelope.

## File Changes

| File | Change |
|------|--------|
| `gmcp_sentience.h` | Add `sentience_room_map_input_t` struct, builder declaration |
| `gmcp_sentience.c` | Add `sentience_build_room_map()` builder, add map send to `sentience_gmcp_update()` |
| `act_info.c` | Add `render_area_map_to_buffer()` (extracts from existing show_map logic), add suppression checks |
| `wilds.c` | Add `render_wilds_map_to_buffer()` (variant of show_map_to_char_wyx) |
| `merc.h` | Declare render functions if needed |
| `tests/unit/gmcp_sentience_tests.c` | Add Room.Map builder tests |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Add Room.Map test data |

## Testing

### Builder Tests (unit)

- `gmcp_build_room_map_area`: Area map with typical input → valid JSON with all fields
- `gmcp_build_room_map_wilds`: Wilderness map with larger dimensions → valid JSON
- `gmcp_build_room_map_empty`: Empty map_text → valid JSON with empty string

### Integration Verification

- Verify map is sent on room change (same trigger as Room.Info)
- Verify inline suppression when GMCP is active
- Verify correct type field for area vs wilderness rooms

## Scope Boundaries

### In Scope
- `Sentience.Room.Map` GMCP package with pre-rendered map text
- Area minimap and wilderness map support
- Inline suppression when GMCP map is active
- Builder function with unit tests

### Out of Scope
- Structured tile/terrain data for client-side rendering (future enhancement)
- Map panel UI implementation (web client concern, separate project)
- `Client.Ready` capability negotiation for map panel (Phase 3 handles this)
- Interactive map features (click-to-move, fog of war)

## Relationship to Phase 3

This package is added as an additional task in Phase 3, alongside `Char.Affects`,
`Char.Enemies`, and `Room.Contents`. It uses the same infrastructure:
- Same dirty-flag pattern as Room.Info
- Same builder/input pattern as other Phase 1/3 packages
- Announced in `Client.Ready.Capabilities` package list
- Same test patterns (JSON test data + C handler)
