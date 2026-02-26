# Wilds Systems & Scripting Guide

This guide is a practical reference for building world content that combines wilderness maps, temporary overlays, region spawns, vlinks, and instanced interiors.

It focuses on production workflows and script patterns that already exist in the codebase.

## 1) Core Model

A wilderness has three map layers to think about:

- **Static map** (`staticmap`): long-term terrain layout.
- **Runtime map** (`map`): mutable terrain used during play.
- **Temporary overlays** (chunk overlays): short-lived tile/region masks (storms, invasions, POI markers).

When deciding where to put logic:

- Use **runtime map edits** for persistent simulation changes.
- Use **temporary overlays** for event-driven, expiring state.
- Use **region definitions/spawns** for ambient population behavior.

## 2) Builder Tooling (wedit)

Primary OLC surfaces:

- `wedit region`: region boxes, metadata, spawn lists, requirements.
- `wedit overlay`: temporary map/region overlays.
- `wedit vlink`: wilderness-to-room/dungeon connectivity.
- `wedit wildgen`: map import/check/export flows.

Recommended order for new content:

1. Build terrain + regions.
2. Configure vlinks and destination interiors.
3. Add spawn definitions per region.
4. Add script automation (overlays/events/instance control).

## 3) Script Hooks for Wilds

### Existing/Relevant Ifchecks

- `if inwilds <entity>`
- `if mapid <entity>`
- `if mapx <entity>`
- `if mapy <entity>`
- `if roomwilds <entity>`
- `if roomx <entity>`
- `if roomy <entity>`
- `if hasvlink <entity>`
- `if hasvlink <wuid> <x> <y>`

`hasvlink` now supports both entity-based and coordinate-based checks.

### New Script Commands

#### `wildstile`

Set a single runtime tile:

- `wildstile <wuid> <x> <y> <tile>`
- `wildstile <coord> <tile>`
- `wildstile <coord> <dx> <dy> <tile>`

Behavior:

- `<tile>` is a single terrain token.
- Token must exist in terrain definitions.
- `lastreturn = 1` on success, `0` on failure.

#### `wildsoverlay`

Manage temporary overlays:

- `wildsoverlay add <wuid> <x1> <y1> <x2> <y2> <tile> [region] [duration_seconds]`
- `wildsoverlay add <coord> <dx1> <dy1> <dx2> <dy2> <tile> [region] [duration_seconds]`
- `wildsoverlay remove <wuid> <zone_id>`
- `wildsoverlay clearregion <wuid> <region>`
- `wildsoverlay cleanup <wuid>`

Behavior:

- `add` returns new `zone_id` in `lastreturn`.
- `remove`, `clearregion`, `cleanup` return removed-count in `lastreturn`.
- `duration_seconds <= 0` means no expiry.

#### `wildsanchor`

Create/store a coordinate anchor variable (stored as wilderness-room coordinate tuple):

- `wildsanchor <varname> <wuid> <x> <y> [dx] [dy]`
- `wildsanchor <varname> <room|coord> [dx] [dy]`

Behavior:

- Writes a coordinate variable (wilds uid + x/y) for later relative placement.
- `lastreturn = 1` on success, `0` on failure.

#### `wildsvlink`

Manage temporary runtime vlinks (stored in wilderness state sidecar):

- `wildsvlink add <wuid x y|room|coord> <door> <dest_wnum> [to_wilds|from_wilds|two_way] [duration_seconds]`
- `wildsvlink remove <wuid> <runtime_vlink_uid>`
- `wildsvlink cleanup <wuid>`

Behavior:

- `add` returns runtime vlink uid in `lastreturn`.
- `remove`/`cleanup` return removed-count in `lastreturn`.

### Prog Type Availability

`wildstile` and `wildsoverlay` are available from:

- mob/object/room/token progs
- area/instance/dungeon/quest command spaces

For qprogs, use the `quest` command namespace (same command set as area command space).

## 4) Pattern: Instanced Bandit Camp via Wilderness Entry

Goal: wilderness camp footprint that routes into an instanced interior and cleans itself up.

### Build Setup

1. Mark the camp footprint with `wedit region`.
2. Create a `wedit vlink` entry tile that points to staging/entry logic.
3. Author blueprint/instance scripts for camp interior.

### Runtime Pattern

1. Player enters marked wilderness tile.
2. Script validates prerequisites (cooldown/faction/quest stage).
3. Script creates or loads instance (`loadinstanced` / instance flow).
4. Script transfers party into instance entry.
5. On completion/failure, call `instancecomplete`/`instancefailure` and cleanup.
6. Optionally apply `wildsoverlay add` during active encounter, then remove.

### Cleanup Rules

- Always remove temporary overlays by `zone_id` or `clearregion`.
- Keep encounter-specific state in script vars scoped to instance/quest context.
- Use completion/failure handlers to guarantee teardown on all exits.

## 5) Pattern: Scheduled Ship Transport

Goal: fixed route between continents with predictable windows.

### Data Setup

- Define departure/arrival wilderness coordinates.
- Use vlinks or dock rooms for embark/disembark transitions.
- Add ship-facing room messaging and map cues.

### Runtime Pattern

1. Scheduler script determines current leg/window.
2. During boarding window, show announcements and permit boarding.
3. Depart: lock boarding, transition travelers.
4. Travel phase: optional `wildsoverlay add` route marker.
5. Arrival: transfer to destination dock/landing, clear overlays.

Keep transport state explicit (phase + next transition time) so restarts are recoverable.

## 6) Pattern: On-Demand Goblin Airship

Goal: player-selected destination, one-shot transport, no fixed timetable.

### Runtime Pattern

1. Interaction script gathers destination choice.
2. Validate destination unlock conditions.
3. Reserve/lock trip slot.
4. Trigger travel sequence (announce, fade/transfer, arrival).
5. Land at destination coordinate or linked room.
6. Unlock slot + cleanup any temporary markers.

Use coordinate checks (`mapid/mapx/mapy`) and `hasvlink` for safe landing eligibility.

## 7) Region Spawns + Overlays Together

Use regions for baseline ecology and overlays for event spikes:

- Normal state: region spawn lists drive ambient populations.
- Event state: overlay changes effective tile/region perception for the event window.
- End event: remove overlays and let ambient rules resume.

This keeps high-frequency event behavior separate from long-term terrain authoring.

## 8) Operational Tips

- Prefer coordinate-based checks for deterministic behavior.
- Store overlay `zone_id` immediately after `wildsoverlay add` so cleanup is exact.
- Keep script side effects idempotent where possible (safe to re-run).
- For long flows, split into small scripts and use `xcall` for composition.
- Use `wedit <wilds_uid>` + `wildgen bake` to merge runtime tile mods into `staticmap` in-game (no export/import needed for tile mods).

## 9) Minimal Example Snippets

### Temporary blockade for 5 minutes

```
# area/quest script command space
wildsoverlay add 6 120 450 130 460 ^ 0 300
varset zone_id $lastreturn
```

### Remove that blockade later

```
wildsoverlay remove 6 $zone_id
```

### Runtime tile mutation (single point)

```
wildstile 6 124 455 ~
```

### Bake runtime tile mods into staticmap

```text
wedit 6
wildgen bake
```

`wildgen bake` behavior:

- Copies current runtime map tiles (`map`) into `staticmap`.
- Triggers wilderness save so `_mods` collapses to empty (or zero diff).
- Does **not** bake temporary overlays; overlays remain separate runtime records.

## 10) Command Runbook: Temporary Structure -> Instance

This is the complete command flow for a temporary wilderness structure (camp/portal/ruin) that links into an instance and then tears down cleanly.

### A) Builder prep (one-time)

1. Build your destination interior and spawn logic in blueprint/instance content.
2. Ensure you can load that interior through your existing instance script path (`loadinstanced`).
3. Add/verify terrain token(s) used for the temporary footprint.

### B) Spawn the temporary structure (script commands)

```text
# 1) Anchor to an absolute location
wildsanchor camp_anchor 6 120 450

# 2) Paint temporary footprint relative to the anchor
wildsoverlay add $camp_anchor -2 -2 2 2 ^ 0 1800
varset camp_zone_id number $lastreturn

# 3) Optional: mark center tile explicitly
wildstile $camp_anchor 0 0 X

# 4) Add runtime vlink from wilds -> room
# linkage: from_wilds | to_wilds | two_way
wildsvlink add $camp_anchor north 123#4501 two_way 1800
varset camp_vlink_uid number $lastreturn
```

### C) Enter flow: create/load instance and transfer players

Use your trigger condition (`if hasvlink`, proximity checks, interaction command, etc.) and then run:

```text
# 1) Create/load instance (your existing args pattern)
loadinstanced <your_instance_args_here>

# 2) Transfer player or group to instance entry room
transfer $PLAYER <instance_entry_room_wnum> silent
# or
gtransfer $PLAYER <instance_entry_room_wnum> silent
```

### D) Exit/completion flow

When encounter ends, mark result and move players out:

```text
# success path
instancecomplete $INSTANCE
gtransfer $PLAYER 123#4500 silent

# failure path
instancefailure $INSTANCE
gtransfer $PLAYER 123#4500 silent
```

### E) Guaranteed cleanup (always run)

```text
# remove runtime vlink
wildsvlink remove 6 $camp_vlink_uid

# remove overlay footprint
wildsoverlay remove 6 $camp_zone_id

# optional safety cleanup for expired leftovers
wildsvlink cleanup 6
wildsoverlay cleanup 6
```

### F) Validation checks

Use ifchecks before and after cleanup:

```text
if hasvlink 6 120 450
if mapid $PLAYER
if mapx $PLAYER
if mapy $PLAYER
```

If you need multiple temporary structures at once, store `zone_id` and `runtime_vlink_uid` per structure key and clean each pair explicitly.

---

If you add new wilderness-facing commands later, keep this guide aligned with:

- command registration tables (`script_*cmds.c`, `script_commands.c`)
- ifcheck definitions (`script_const.c`, `script_ifc.c`)
- builder UX (`editors/wilderness/wedit.c`)
