# Plan: Maze Layout Type & Shared Dungeon Backport

**Date:** February 7, 2026
**Status:** Draft for Review
**Source:** Backport from `src_20_dev` with adaptations for current production codebase

---

## 1. Goals

1. **Backport `BSTYPE_MAZE`** - procedural maze generation for blueprint sections, from `src_20_dev`
2. **Add shared dungeon support** - `DUNGEON_SHARED` flag and group management fields
3. **Migrate existing maze areas** to the dungeon/instance system:
   - `geldmaze.are` (Geldoff's Maze) → solo/party dungeon instance
   - `maze-level[1-5].are` (Planes of Agony) → shared multi-floor dungeon
4. **Add script commands** for spawning and managing dungeons/instances
5. **Update OLC editors** (bsedit, dngedit) to support maze configuration and shared dungeon settings

---

## 2. Current State

### What exists in production

| Component | Status | Notes |
|-----------|--------|-------|
| Blueprint system | Working | `BSTYPE_STATIC` only, no maze |
| Instance system | Working | Create, destroy, idle management, persistence |
| Dungeon system | Working | Multi-floor, levels, special rooms/exits |
| OLC editors | Working | bpedit, bsedit, dngedit - no maze or shared config |
| JSON serialization | Working | Blueprints/sections serialize, no maze fields |
| Script commands | Partial | `spawndungeon`, `instancecomplete`, `dungeoncomplete`, `loadinstanced`, `makeinstanced` |
| Maze spell | Working | Hardcoded to `find_area("Geldoff's Maze")` / `find_area("Maze-Level1")`, picks random rooms |
| Maze areas | Legacy .are | 6 pre-built maze areas with hand-crafted rooms |

### What `src_20_dev` adds (not yet in production)

| Component | Location in dev | Notes |
|-----------|----------------|-------|
| `BSTYPE_MAZE` | `merc.h:8137+`, `blueprint.c:1569+` | Full maze generation with DFS algorithm |
| Maze data structures | `merc.h` | `MAZE_WEIGHTED_ROOM`, `MAZE_FIXED_ROOM` |
| `DUNGEON_SHARED` flag | `merc.h:8437` | Single shared instance per dungeon index |
| Group management | `merc.h:8569-8575` | `min_group`, `max_group`, `max_players`, `death_release` |
| Dungeon lifecycle flags | `merc.h:8429-8434` | `FAILED`, `COMMENCED`, `GROUP_COMMENCE`, `FAILURE_ON_WIPE`, `FAILURE_ON_EMPTY` |
| Script: `dungeoncommence` | `script_commands.c` | Initiates a dungeon encounter |
| Script: `dungeonfailure` | `script_commands.c` | Marks dungeon as failed |
| Script: `instancefailure` | `script_commands.c` | Marks instance as failed |
| Blueprint schematic trigger | `TRIG_BLUEPRINT_SCHEMATIC` | Script-driven layout generation |
| Layout/links script cmds | `script_commands.c` | `layout`, `links`, `levels`, `specialexits`, `specialrooms` |
| Enhanced `spawndungeon` | `script_commands.c` | Supports room-by-name, room-by-number |
| Enhanced gate types | `merc.h:3711-3715` | `GATETYPE_BLUEPRINT_SECTION_MAZE`, `GATETYPE_DUNGEON_*` |
| OLC maze commands | `blueprint.c:3926+` | `bsedit maze size/templates/fixed/recall` |

---

## 3. Data Structure Changes

### 3A. New Structures (add to `merc.h`)

```c
/* Maze weighted room template - for random room selection during maze generation */
typedef struct blueprint_maze_weighted_room MAZE_WEIGHTED_ROOM;
struct blueprint_maze_weighted_room {
    MAZE_WEIGHTED_ROOM *next;
    bool valid;
    int weight;              /* Probability weight for selection */
    union {
        WNUM_LOAD load;      /* During load: area_uid + vnum */
        long vnum;           /* Legacy: bare vnum */
    } room_ref;
    ROOM_INDEX_DATA *room;   /* Resolved room pointer */
};

/* Maze fixed room - anchored at specific grid coordinates */
typedef struct blueprint_maze_fixed_room MAZE_FIXED_ROOM;
struct blueprint_maze_fixed_room {
    MAZE_FIXED_ROOM *next;
    bool valid;
    int x, y;                /* Grid coordinates (1-based) */
    union {
        WNUM_LOAD load;      /* During load: area_uid + vnum */
        long vnum;           /* Legacy: bare vnum */
    } room_ref;
    ROOM_INDEX_DATA *room;   /* Resolved room pointer */
    bool connected;          /* true = participates in maze carving; false = isolated pocket */
};

/* Internal: maze cell used during generation only */
typedef struct maze_cell_data MAZE_CELL;
struct maze_cell_data {
    int x, y;                /* Grid position */
    ROOM_INDEX_DATA *room;   /* Cloned room */
    int options[6];          /* Available carving directions (NSEW + diagonals as needed) */
    int num_options;          /* Count of remaining options */
    bool visited;            /* DFS visited flag */
};
```

### 3B. Extend `BLUEPRINT_SECTION` (merc.h)

Add these fields to the existing `struct blueprint_section_data`:

```c
    /* Maze generation parameters (BSTYPE_MAZE only) */
    long maze_x;                /* Grid width */
    long maze_y;                /* Grid height */
    LLIST *maze_templates;      /* MAZE_WEIGHTED_ROOM * - weighted room pool */
    int total_maze_weight;      /* Sum of all template weights */
    LLIST *maze_fixed_rooms;    /* MAZE_FIXED_ROOM * - anchored rooms */
```

### 3C. Add `BSTYPE_MAZE` constant

```c
#define BSTYPE_MAZE     2       /* Procedurally generated maze grid */
```

Add to the `blueprint_section_types` table in `tables.c`:

```c
{ "maze", BSTYPE_MAZE, true },
```

### 3D. Extend `DUNGEON_INDEX_DATA` (merc.h)

Add these fields to the existing `struct dungeon_index_data`:

```c
    int min_group;              /* Minimum group size (excl. pets/mounts), 0 = no minimum */
    int max_group;              /* Maximum group size (excl. pets/mounts), 0 = unlimited */
    int max_players;            /* Maximum total players in dungeon, 0 = unlimited */
    int death_release;          /* How deaths are handled (DEATH_RELEASE_* constants) */
```

### 3E. New Dungeon Flags (merc.h)

Add to the existing `DUNGEON_*` flag block:

```c
#define DUNGEON_FAILED              (F)     /* Dungeon has failed */
#define DUNGEON_COMMENCED           (G)     /* Dungeon has officially started */
#define DUNGEON_GROUP_COMMENCE      (H)     /* Auto-commence when min_group is satisfied */
#define DUNGEON_FAILURE_ON_WIPE     (I)     /* Failure when all players die in boss encounter */
#define DUNGEON_FAILURE_ON_EMPTY    (J)     /* Failure if commenced and everyone leaves */
#define DUNGEON_SHARED              (Y)     /* One instance for all players */
```

Note: Production currently uses `(E)` for `DUNGEON_SCRIPTED_LEVELS` and dev uses `(F)`. Need to reconcile — dev shifted flags up one position. Production has `(A)-(E)` and `(Z)` used. The above assignments use the next available bits `(F)-(J)` and `(Y)`.

### 3F. Death Release Constants

```c
#define DEATH_RELEASE_NORMAL        0   /* Normal: go to death plane */
#define DEATH_RELEASE_TO_START      1   /* Stay with corpse, release → dungeon start */
#define DEATH_RELEASE_TO_FLOOR      2   /* Stay with corpse, release → current floor start */
#define DEATH_RELEASE_TO_CHECKPOINT 3   /* Stay with corpse, release → last checkpoint */
#define DEATH_RELEASE_FAILURE       4   /* Respawn at death location, dungeon fails */
```

### 3G. New Trigger Type

```c
TRIG_DUNGEON_COMMENCED,     /* Fires when dungeon commences (DUNGEON_GROUP_COMMENCE) */
```

Add to the `trigger_table` in `tables.c` and the trigger type enum in `scripts.h`.

---

## 4. Maze Generation Algorithm

Backport `blueprint_section_generate_maze()` from `src_20_dev/blueprint.c:1569-1840`.

### Algorithm Summary

1. **Allocate grid** — `maze_x * maze_y` cells, each with room pointer and direction options `{N, E, S, W}`
2. **Remove boundary options** — top row loses N, bottom row loses S, left column loses W, right column loses E
3. **Place fixed rooms** — clone rooms from `maze_fixed_rooms` at their `(x, y)` coordinates. If `connected == false`, mark visited with 0 options (creates isolated pockets)
4. **Fill remaining cells** — weighted random selection from `maze_templates`, clone each room
5. **Link pre-existing exits** — fixed rooms that already have exits in their source template get those exits linked to adjacent cells
6. **DFS maze carving** — stack-based depth-first search:
   - For each unvisited cell, push onto stack
   - While stack not empty: pick random available direction from top cell
   - If neighbor is unvisited and has reverse direction available, carve passage (create bidirectional exits), push neighbor
   - If no options remain, pop (backtrack)
   - Multiple start points handle disconnected regions from fixed room isolation

### Key Design Points

- Creates a **perfect maze** (all connected cells reachable, no cycles)
- Fixed rooms with `connected=false` create isolated rooms/pockets within the grid
- Each cell is a cloned virtual room with a unique instance room ID
- Exits are standard `EXIT_DATA` with `EX_ENVIRONMENT` flag for vertical connections
- The algorithm handles fragmented grids from fixed room boundaries by iterating all cells

### Production Adaptation Notes

- Dev uses `create_virtual_room_nouid()` for room cloning — verify this exists in production or adapt
- Room UID assignment via `get_virtual_id()` — verify production equivalent
- The `__maze_link_room()` helper creates bidirectional exits — uses `new_exit()` from production's existing exit system

---

## 5. Shared vs Solo Instance Behavior

### Solo/Party Instances (Default — no `DUNGEON_SHARED`)

This is the existing production behavior. Each player or group gets their own dungeon:

```
Player A enters → create_dungeon() → new dungeon, A is owner
Player B (in A's group) enters → find_dungeon_byplayer(leader=A) → joins A's dungeon
Player C (solo) enters → create_dungeon() → separate new dungeon, C is owner
```

**Use case:** Geldoff's Maze — each player/group gets a unique maze instance

### Shared Instances (`DUNGEON_SHARED` flag set)

One instance exists per dungeon index. All players share it:

```
Player A enters → create_dungeon() → new dungeon (checks DUNGEON_SHARED, none exists yet)
Player B enters → find_dungeon_byplayer() → finds existing shared dungeon → joins it
Player C enters → same → joins same dungeon
```

The key logic in `find_dungeon_byplayer()` from dev:
```c
if (IS_SET(dng->flags, DUNGEON_SHARED) || dungeon_isowner_player(dng, ch))
    break;
```

For shared dungeons, *any* player matches because the `DUNGEON_SHARED` check bypasses ownership.

**Use case:** Planes of Agony — one shared 5-floor maze that everyone enters

### Group Management (New Fields)

| Field | Purpose | Example |
|-------|---------|---------|
| `min_group` | Minimum party size to enter | PoA might require 2+ |
| `max_group` | Maximum party size | PoA might cap at 6 |
| `max_players` | Total players allowed simultaneously | Shared PoA might allow 20 |
| `death_release` | What happens when you die | PoA: `DEATH_RELEASE_TO_FLOOR` |

### Commence System (`DUNGEON_GROUP_COMMENCE`)

For dungeons that require a minimum group size before "starting":

1. Players enter, dungeon exists but hasn't commenced
2. When `min_group` players are present, dungeon auto-commences (or script calls `dungeoncommence`)
3. `TRIG_DUNGEON_COMMENCED` fires — scripts can spawn bosses, start timers, etc.
4. Late arrivals can still join (up to `max_players`)

---

## 6. Script Commands

### 6A. Already in Production

| Command | Signature | Purpose |
|---------|-----------|---------|
| `spawndungeon` | `$PLAYER <vnum> <floor> $ROOM_VAR` | Spawn player into dungeon floor, returns room |
| `dungeoncomplete` | `$DUNGEON` | Mark dungeon completed, fire TRIG_COMPLETED |
| `instancecomplete` | `$INSTANCE` | Mark instance completed, fire TRIG_COMPLETED |
| `loadinstanced` | `mobile\|object <vnum> [room] [$VAR]` | Load entity as instance-specific |
| `makeinstanced` | `$MOBILE\|$OBJECT` | Convert existing entity to instance-specific |

### 6B. To Backport from Dev

| Command | Signature | Purpose |
|---------|-----------|---------|
| `dungeoncommence` | `$DUNGEON` | Mark dungeon as commenced, fire TRIG_DUNGEON_COMMENCED |
| `dungeonfailure` | `$DUNGEON` | Mark dungeon as failed, fire TRIG_FAILED |
| `instancefailure` | `$INSTANCE` | Mark instance as failed, fire TRIG_FAILED |

### 6C. Enhanced `spawndungeon`

The dev version has a richer signature:

```
spawndungeon $PLAYER <widevnum> floor <#> $ROOM_VAR
spawndungeon $PLAYER <widevnum> room <#|name> $ROOM_VAR
```

Production currently only supports `<vnum> <floor>`. Enhancement adds:
- Widevnum support (area-scoped dungeon references)
- Room-by-name lookup for special rooms within the dungeon
- Room-by-number for specific room indexing

### 6D. Blueprint/Dungeon Configuration Script Commands (For Scripted Layouts)

These are used inside `TRIG_BLUEPRINT_SCHEMATIC` and `TRIG_DUNGEON_SCHEMATIC` triggers to dynamically configure layouts at instance creation time:

| Command | Trigger Context | Purpose |
|---------|----------------|---------|
| `layout` | `TRIG_BLUEPRINT_SCHEMATIC` | Configure which sections to generate (static, weighted, grouped) |
| `links` | `TRIG_BLUEPRINT_SCHEMATIC` | Configure how sections connect to each other |
| `levels` | `TRIG_DUNGEON_SCHEMATIC` | Configure which floors make up the dungeon levels |
| `specialexits` | `TRIG_DUNGEON_SCHEMATIC` | Configure cross-floor exit connections |
| `specialrooms` | Instance/Dungeon progs | Manage named special rooms within instances/dungeons |

**`layout` subcommands:**
```
layout clear
layout add static <section#>
layout add weighted
layout add group
layout weighted <#> <weight> <section#>
layout group <#> add static <section#>
layout group <#> add weighted
layout group <#> weighted <#> <weight> <section#>
```

**`links` subcommands:**
```
links clear
links add static <from-mode> <from-section#> <from-link#> <to-mode> <to-section#> <to-link#>
links add source <to-mode> <to-section#> <to-link#>
links add destination <from-mode> <from-section#> <from-link#>
links add weighted
links add group
links from <#> <weight> <from-mode> <from-section#> <from-link#>
links to <#> <weight> <to-mode> <to-section#> <to-link#>
```
Modes: `generated` (by generation order) or `ordinal` (by definition order)

**`levels` subcommands:**
```
levels clear
levels add static <floor#>
levels add weighted
levels add group
levels weighted <#> <weight> <floor#>
levels group <#> add static <floor#>
levels group <#> add weighted
levels group <#> weighted <#> <weight> <floor#>
```

### 6E. IFCHECKs (Script Conditionals)

Backport these conditional checks for use in scripts:

| IFCheck | Usage | Purpose |
|---------|-------|---------|
| `dungeonflag` | `if dungeonflag($DUNGEON, shared)` | Test dungeon flags |
| `instanceflag` | `if instanceflag($INSTANCE, completed)` | Test instance flags |
| `indungeon` | `if indungeon($MOBILE)` | Check if entity is in a dungeon |
| `ininstance` | `if ininstance($MOBILE)` | Check if entity is in an instance |

### 6F. Entity Fields for Scripts

Ensure scripts can access dungeon/instance properties:

```
$DUNGEON.flags          - dungeon flags bitmask
$DUNGEON.index          - dungeon index reference
$DUNGEON.floor(#)       - specific floor instance
$DUNGEON.players        - player count
$INSTANCE.flags         - instance flags bitmask
$INSTANCE.blueprint     - blueprint reference
$INSTANCE.dungeon       - parent dungeon (if any)
$INSTANCE.entrance      - entrance room
$INSTANCE.exit          - exit room
$ROOM.instance          - instance the room belongs to
$ROOM.dungeon           - dungeon the room belongs to (via instance)
```

---

## 7. OLC Editor Changes

### 7A. bsedit (Blueprint Section Editor)

Add `maze` subcommand with these operations:

```
bsedit maze size <width> <height>       Set maze grid dimensions (clears fixed rooms)
bsedit maze templates list              List weighted room templates
bsedit maze templates add <weight> <room_vnum>   Add template
bsedit maze templates remove <#>        Remove template by index
bsedit maze fixed list                  List fixed room placements
bsedit maze fixed add <x> <y> <room_vnum> [connected]   Add fixed room
bsedit maze fixed remove <#>            Remove fixed room by index
bsedit maze recall <x> <y>              Set recall point in maze grid
bsedit maze recall clear                Remove recall point
```

**Validation rules:**
- Templates must be `ROOM_BLUEPRINT` flagged rooms with no pre-existing lateral exits
- Fixed room coordinates must be within `1..maze_x`, `1..maze_y`
- Fixed rooms on grid edges can have lateral exits only pointing outward
- Fixed rooms anywhere can have UP/DOWN exits
- Resizing the maze clears all fixed rooms (coordinates may become invalid)

### 7B. dngedit (Dungeon Editor)

Add display and edit commands for new fields:

```
dngedit show             Extended to show: min_group, max_group, max_players, death_release
dngedit mingroup <#>     Set minimum group size (0 = no minimum)
dngedit maxgroup <#>     Set maximum group size (0 = unlimited)
dngedit maxplayers <#>   Set maximum total players (0 = unlimited)
dngedit deathrelease <type>   Set death handling (normal|start|floor|checkpoint|failure)
dngedit flags            Extended flag table to include new DUNGEON_* flags
```

The `dungeon_flags` table in `tables.c` needs the new flag entries.

### 7C. Show Display Updates

Both editors should use `widevnum_string_*()` for any room/vnum references displayed.

---

## 8. JSON Serialization

### 8A. Blueprint Section — Maze Data

Extend `json_area_serialize_blueprint_section()` and its deserializer in `json_area.c`:

```json
{
  "vnum": 100,
  "name": "PoA Floor 1 Maze",
  "type": 2,
  "flags": 0,
  "maze": {
    "width": 10,
    "height": 8,
    "templates": [
      { "weight": 50, "room": "923#1234" },
      { "weight": 30, "room": "923#1235" },
      { "weight": 20, "room": "923#1236" }
    ],
    "fixed_rooms": [
      { "x": 1, "y": 1, "room": "923#5000", "connected": true },
      { "x": 10, "y": 8, "room": "923#5001", "connected": true },
      { "x": 5, "y": 4, "room": "923#5002", "connected": false }
    ],
    "recall": { "x": 1, "y": 1 }
  }
}
```

Room references use widevnum format (`auid#vnum`) for cross-area safety.

### 8B. Dungeon Index — New Fields

Extend dungeon index serialization:

```json
{
  "vnum": 1,
  "name": "Planes of Agony",
  "flags": "shared",
  "min_group": 2,
  "max_group": 6,
  "max_players": 20,
  "death_release": "floor",
  "entry_room": "923#15000",
  "exit_room": "923#15001",
  "floors": [ ... ],
  "levels": [ ... ]
}
```

---

## 9. Maze Area Migration Plan

### 9A. Geldoff's Maze (`geldmaze.are`, UID 1299, vnums 300001-300500)

**Target:** Solo/party dungeon with one `BSTYPE_MAZE` floor

1. **Create blueprint area** with template rooms extracted from geldmaze
   - Identify distinct room "types" (corridors, dead ends, intersections, special rooms)
   - Create template rooms with appropriate descriptions, sector types, room flags
   - Mark templates as `ROOM_BLUEPRINT`

2. **Create blueprint section** (`BSTYPE_MAZE`)
   - Grid size based on current maze dimensions (analyze room count — ~500 rooms suggests roughly 20x25 or similar)
   - Weighted templates reflecting the distribution of room types
   - Fixed rooms for any special locations (entrance, boss room, treasure rooms)

3. **Create blueprint** referencing the section
   - Single section, single entry, single exit
   - Recall point at entrance

4. **Create dungeon index**
   - One floor (the maze blueprint)
   - No `DUNGEON_SHARED` flag (solo/party)
   - `death_release`: `DEATH_RELEASE_NORMAL` (standard death behavior)

5. **Update `spell_maze`** combat path:
   ```c
   // Old:
   area = find_area("Geldoff's Maze");
   while(!(room = get_room_index(area, number_range(area->min_vnum, area->max_vnum))));

   // New:
   ROOM_INDEX_DATA *room = spawn_dungeon_player(victim, geldmaze_wnum, 1);
   ```

6. **Deprecate `geldmaze.are`** once migration is validated

### 9B. Planes of Agony (`maze1-5.are`, UIDs 1294-1298, vnums 150000-150349)

**Target:** Shared 5-floor dungeon with `BSTYPE_MAZE` floors

1. **Create blueprint areas** for each maze level's template rooms
   - Each maze-level area has ~70 rooms — extract distinct templates
   - Progressively harder rooms at higher levels (different descriptions, mobs, objects)

2. **Create 5 blueprint sections** (`BSTYPE_MAZE`)
   - Each section uses templates from its corresponding maze-level area
   - Grid sizes: analyze current room counts (70 rooms each → ~8x9 or similar)
   - Fixed rooms for stairways up/down, special encounters

3. **Create 5 blueprints** (one per floor)
   - Each references its maze section
   - Entry/exit links for `PREVFLOOR`/`NEXTFLOOR` connections

4. **Create dungeon index**
   - 5 levels, one blueprint per level
   - `DUNGEON_SHARED` flag set
   - Entry room: static world location where players enter PoA
   - Exit room: static world location where players emerge
   - `min_group`: 0 (anyone can enter)
   - `max_players`: reasonable cap (e.g., 50)
   - `death_release`: `DEATH_RELEASE_TO_FLOOR`

5. **Update `spell_maze`** non-combat path:
   ```c
   // Old:
   area = find_area("Maze-Level1");
   while(!(room = get_room_index(area, number_range(area->min_vnum, area->max_vnum))));

   // New:
   ROOM_INDEX_DATA *room = spawn_dungeon_player(victim, poa_wnum, 1);
   ```

6. **Add scripts** for floor progression, mob spawning, etc.

7. **Deprecate `maze[1-5].are`** once migration is validated

---

## 10. Implementation Phases

### Phase 1: Data Structures & Constants
- Add maze structs to `merc.h` (`MAZE_WEIGHTED_ROOM`, `MAZE_FIXED_ROOM`, `MAZE_CELL`)
- Add maze fields to `BLUEPRINT_SECTION`
- Add `BSTYPE_MAZE` constant and table entry
- Add dungeon flags (`DUNGEON_SHARED`, `DUNGEON_FAILED`, `DUNGEON_COMMENCED`, etc.)
- Add dungeon index fields (`min_group`, `max_group`, `max_players`, `death_release`)
- Add `DEATH_RELEASE_*` constants
- Add `TRIG_DUNGEON_COMMENCED` trigger type
- Update `mem.c` for new struct allocation/deallocation
- Update both `Makefile` and `CMakeLists.txt` if new files are added

**Risk:** LOW — additive struct changes, no existing behavior modified

### Phase 2: Maze Generation Algorithm
- Backport `blueprint_section_generate_maze()` into `blueprint.c`
- Backport helper functions (`__maze_remove_option`, `__maze_has_option`, `__maze_link_room`)
- Integrate into `clone_blueprint_section()` — when `type == BSTYPE_MAZE`, call maze generator instead of room range cloning
- Add recall room handling for maze sections (by grid coordinate index)

**Risk:** MEDIUM — new code path in instance generation, needs testing

### Phase 3: Shared Dungeon Logic
- Update `find_dungeon_byplayer()` to check `DUNGEON_SHARED` flag
- Update `create_dungeon()` to return existing shared dungeon if one exists
- Add group size validation in `spawn_dungeon_player()` (`min_group`, `max_group`, `max_players` checks)
- Add `dungeon_commence()` and `dungeon_failed()` functions
- Add death release handling (integrate with existing death/resurrection code)

**Risk:** MEDIUM — modifies dungeon entry flow, needs careful testing with existing dungeons

### Phase 4: OLC Editor Updates
- Add `bsedit_maze()` command handler with size/templates/fixed/recall subcommands
- Update `bsedit_show()` to display maze configuration
- Add dngedit commands for `mingroup`, `maxgroup`, `maxplayers`, `deathrelease`
- Update `dngedit_show()` to display new fields
- Update `dungeon_flags` table with new flag entries

**Risk:** LOW — editor-only changes, no gameplay impact

### Phase 5: JSON Serialization
- Extend `json_area_serialize_blueprint_section()` for maze data
- Extend `json_area_deserialize_blueprint_section()` to parse maze data
- Extend dungeon index serialization for new fields
- Use WNUM_LOAD format for room references in maze templates/fixed rooms
- Add `resolve_wnum_load()` calls in `fix_rooms()` for maze room references

**Risk:** LOW — extends existing serialization patterns

### Phase 6: Script Commands
- Backport `dungeoncommence`, `dungeonfailure`, `instancefailure` commands
- Enhance `spawndungeon` with room-by-name and widevnum support
- Register new commands in mob/instance/dungeon command tables
- Add IFCHECKs: `dungeonflag`, `instanceflag`, `indungeon`, `ininstance`
- Add entity field accessors for dungeon/instance properties
- Backport layout/links/levels/specialexits/specialrooms (for scripted generation)
- Add `TRIG_BLUEPRINT_SCHEMATIC` and `TRIG_DUNGEON_COMMENCED` triggers

**Risk:** MEDIUM — script interface changes need backward compatibility

### Phase 7: Maze Area Migration
- Analyze existing maze areas to extract template rooms
- Create blueprint areas with template rooms
- Configure blueprint sections, blueprints, and dungeon indexes via OLC
- Update `spell_maze` to use `spawn_dungeon_player()`
- Test maze generation produces playable, connected mazes
- Test shared dungeon behavior for PoA
- Test solo/party behavior for Geldoff's Maze

**Risk:** HIGH — replacing live game content, needs thorough testing on dev/test server

### Phase 8: Cleanup
- Update `return_from_maze()` to work with instance-based mazes (eject from dungeon)
- Update `maze_time_left` timer to use dungeon idle system
- Remove hardcoded area name lookups from `spell_maze`
- Mark old maze .are files as deprecated
- Update area.lst if maze areas are removed

**Risk:** MEDIUM — touches existing maze spell flow

---

## 11. Files Affected

### New or Modified Source Files

| File | Changes |
|------|---------|
| `merc.h` | New structs, new fields on existing structs, new constants |
| `blueprint.c` | Maze generation algorithm, clone_blueprint_section maze path |
| `dungeon.c` | Shared dungeon logic, commence/failure, group validation |
| `mem.c` | Allocation/deallocation for new structs |
| `tables.c` | `BSTYPE_MAZE` table entry, dungeon flag entries, death release table, trigger entries |
| `scripts.h` | New trigger types, new command declarations |
| `script_commands.c` | New script commands, enhanced spawndungeon |
| `script_ifc.c` | New IFCHECKs |
| `io/json/json_area.c` | Maze serialization/deserialization |
| `editors/blueprints/bsedit.c` | Maze OLC subcommands |
| `editors/dungeons/dngedit.c` | Group/shared/death-release OLC commands |
| `magic_astral.c` | Updated spell_maze to use dungeon system |
| `handler.c` | Updated return_from_maze for instance-based mazes |
| `update.c` | Maze timer integration with dungeon idle |
| `Makefile` | Update if new .c files added |
| `CMakeLists.txt` | Update if new .c files added |

### Data Files

| File | Changes |
|------|---------|
| `area/blueprints.are` (or new .json) | New blueprint template rooms for mazes |
| `data/world/blueprints.dat` → `.json` | Maze blueprint section definitions |
| `data/world/dungeons.dat` → `.json` | Shared dungeon index with new fields |

---

## 12. Testing Strategy

### Unit Tests
- Maze generation with various grid sizes (small 3x3, medium 10x10, large 20x25)
- Maze with fixed rooms (connected and isolated)
- Maze template weight distribution
- Shared dungeon creation and lookup
- Group size validation

### Integration Tests
- Full instance creation with BSTYPE_MAZE section
- Multi-floor dungeon with maze floors and PREVFLOOR/NEXTFLOOR connections
- Shared dungeon: multiple players entering same dungeon
- Solo dungeon: separate instances per player/group
- Script-driven dungeon spawning via `spawndungeon`
- Dungeon commence/completion/failure lifecycle
- JSON round-trip: serialize maze blueprint → deserialize → verify identical

### Gameplay Tests (Manual, on dev server)
- Walk through generated mazes — verify connectivity, no dead unreachable areas
- Cast maze spell — verify player lands in instance, timeout ejects correctly
- Multiple players in shared PoA dungeon — verify same instance
- Group enters Geldoff's Maze — verify solo instance
- Death in dungeon — verify death_release behavior
- Dungeon idle timeout — verify cleanup after all players leave

---

## 13. Open Questions

1. **Grid size for existing mazes:** Need to analyze the actual room layouts in `geldmaze.are` and `maze[1-5].are` to determine appropriate grid dimensions. The rooms exist as pre-connected graphs — we need to understand their topology to map to a grid.

2. **Template room extraction:** How many distinct room "types" exist in each maze area? Should we keep all ~500 rooms as templates or distill them to a smaller representative set?

3. **Maze spell UX change:** Currently the spell picks a random room in the area. With instances, the player would get a fresh maze each time (or their existing one). Is this the desired behavior, or should the spell just teleport to a random room in an existing shared maze?

4. **Backward compatibility:** When `DUNGEON_SHARED` is added to `find_dungeon_byplayer()`, does the check need to also verify dungeon hasn't been completed/failed before allowing new players in?

5. **Scripted layout commands priority:** The `layout`/`links`/`levels` commands are complex. Should they be deferred to a later phase if the initial maze areas use static (non-scripted) configuration?

6. **`.dat` → `.json` migration:** Should the blueprint/dungeon data format migration to JSON happen as part of this work, or separately? Currently `blueprints.dat` and `dungeons.dat` are binary format.

---

## 14. Reference: Key Dev Branch Locations

| Component | File | Lines |
|-----------|------|-------|
| Maze structs | `src_20_dev/merc.h` | 8137-8189 |
| Dungeon flags | `src_20_dev/merc.h` | 8427-8445 |
| Dungeon index fields | `src_20_dev/merc.h` | 8569-8575 |
| Maze generation | `src_20_dev/blueprint.c` | 1569-1840 |
| Maze OLC | `src_20_dev/blueprint.c` | 3926-4324 |
| Maze file I/O | `src_20_dev/blueprint.c` | 215-250, 833-854 |
| Instance creation | `src_20_dev/blueprint.c` | 2663-2723 |
| Dungeon creation | `src_20_dev/dungeon.c` | 1452-1615 |
| Shared dungeon lookup | `src_20_dev/dungeon.c` | 1679-1695 |
| Player spawn | `src_20_dev/dungeon.c` | 1733-1835 |
| Script commands | `src_20_dev/script_commands.c` | Various |
| Gate types | `src_20_dev/merc.h` | 3702-3715 |
| Instance persistence | `src_20_dev/` + `docs/persist_instances.md` | — |
