# Integrating Procedural Maze Generation into Sentience MUD

This document outlines the necessary steps and code modifications to integrate dynamic maze generation directly into the game's existing blueprint and dungeon system. This will replace the outdated external `mazer` tool and static `.are` files for Geldoff's Maze and the Pyramid of the Abyss.

## 1. Project Overview and Goals

**Current State:**
*   **Legacy Maze Generation:** The game currently uses an external C program (`maze/mazer`) and shell scripts (`scripts/create_maze`, `area/startup`) to generate static `.are` files (`area/geldmaze.are`, `area/maze1.are` through `maze5.are`) at startup.
*   **Blueprint/Dungeon System:** There is an existing system in `src/blueprint.c` and `src/dungeon.c` for creating dynamic instances of areas based on `BLUEPRINT` and `DUNGEON_INDEX_DATA` definitions. This system supports `BSTYPE_STATIC` for cloning pre-defined room ranges.

**Goal:**
*   To integrate the maze generation logic directly into the MUD engine.
*   To allow `BLUEPRINT_SECTION`s to be defined with a `BSTYPE_MAZE`, enabling dynamic maze generation as part of the instance creation process.
*   To configure Geldoff's Maze as a solo instance and the Pyramid of the Abyss as a multi-floor shared dungeon using this new system.
*   To deprecate and remove the legacy `maze/` directory, `scripts/create_maze`, and the static `.are` maze files.

**Key Insight:**
The `src_20_dev` directory contains a partially implemented version of this feature within `src_20_dev/blueprint.c` and `src_20_dev/dungeon.c`, specifically including a `BSTYPE_MAZE` blueprint section type and the `blueprint_section_generate_maze` function. This provides a strong starting point.

## 2. Core Code Modifications (Conceptual)

The primary code changes will involve integrating the maze generation logic from `src_20_dev/blueprint.c` into the main `src/blueprint.c` file and updating core definitions.

### 2.1. `src/merc.h`

This file will need new definitions for the `BSTYPE_MAZE` enum and associated data structures.

**Proposed Changes:**

1.  **Add `BSTYPE_MAZE` to `BLUEPRINT_SECTION_TYPE` enum:**
    *   Locate the `BLUEPRINT_SECTION_TYPE` enum (likely near `struct blueprint_section_data`).
    *   Add a new entry: `BSTYPE_MAZE`.

2.  **Define `MAZE_FIXED_ROOM` and `MAZE_WEIGHTED_ROOM` structs:**
    *   These structs are found in `src_20_dev/blueprint.c` and are essential for defining maze layouts within blueprints.
    *   **`MAZE_FIXED_ROOM`**: Represents a specific room placed at a fixed X/Y coordinate within a maze.
        ```c
        typedef struct maze_fixed_room_data MAZE_FIXED_ROOM;
        struct maze_fixed_room_data
        {
            MAZE_FIXED_ROOM *next;
            bool valid;

            int x;
            int y;
            long vnum;
            bool connected; // If false, this room is treated as already visited and won't be part of the maze generation pathfinding
        };
        ```
    *   **`MAZE_WEIGHTED_ROOM`**: Represents a room template that can be randomly selected for maze cells.
        ```c
        typedef struct maze_weighted_room_data MAZE_WEIGHTED_ROOM;
        struct maze_weighted_room_data
        {
            MAZE_WEIGHTED_ROOM *next;
            bool valid;

            int weight;
            long vnum;
        };
        ```
    *   Integrate these new types into `struct blueprint_section_data`:
        ```c
        struct blueprint_section_data
        {
            // ... existing members ...
            long maze_x; // Width of the maze (for BSTYPE_MAZE)
            long maze_y; // Height of the maze (for BSTYPE_MAZE)
            long total_maze_weight; // Sum of weights for maze_templates
            LLIST *maze_templates; // List of MAZE_WEIGHTED_ROOMs
            LLIST *maze_fixed_rooms; // List of MAZE_FIXED_ROOMs
            // ... existing members ...
        };
        ```

### 2.2. `src/tables.c` and `src/tables.h`

The `blueprint_section_types` flag table needs to be updated.

**Proposed Changes:**

1.  **`src/tables.c`**:
    *   Add a new entry to the `blueprint_section_types` array for "maze".
        ```c
        const struct flag_type blueprint_section_types[] =
        {
            { "static",      BSTYPE_STATIC,       true    },
            { "maze",        BSTYPE_MAZE,         true    }, // Add this line
            { NULL,          0,                   0       }
        };
        ```
2.  **`src/tables.h`**: No direct changes are typically needed here, as it only declares the external `blueprint_section_types` array.

### 2.3. `src/blueprint.c`

This file will undergo the most significant changes, incorporating the maze generation logic.

**Proposed Changes:**

1.  **Merge `load_blueprint_section` logic:**
    *   Find the `load_blueprint_section` function in `src/blueprint.c`.
    *   Integrate the `case 'M'` block from `src_20_dev/blueprint.c` into this function to handle parsing maze-specific attributes when loading blueprint sections from disk.
    *   This will involve reading `MazeW`, `MazeH`, `MazeFixedRoom`, and `MazeTemplate`.
    *   Ensure proper list management (`list_appendlink`) for `maze_templates` and `maze_fixed_rooms`.

2.  **Port `blueprint_section_generate_maze` function:**
    *   Copy the entire `blueprint_section_generate_maze` function from `src_20_dev/blueprint.c` into `src/blueprint.c`.
    *   This function uses a `MAZE_CELL` temporary structure to perform the generation and then links the rooms.
    *   Ensure any helper functions (`__purge_maze_cells`, `__maze_link_room`, `__maze_remove_option`, `__maze_has_option`) are also ported or integrated as static helpers within `blueprint.c`.

3.  **Modify `clone_blueprint_section`:**
    *   Locate the `clone_blueprint_section` function in `src/blueprint.c`.
    *   Add an `else if` condition to handle `BSTYPE_MAZE`. This block will call the newly ported `blueprint_section_generate_maze` function.
    ```c
    INSTANCE_SECTION *clone_blueprint_section(BLUEPRINT_SECTION *parent)
    {
        // ... existing BSTYPE_STATIC logic ...

        else if(parent->type == BSTYPE_MAZE)
        {
            if (!blueprint_section_generate_maze(section, parent))
            {
                free_instance_section(section);
                return NULL;
            }
        }
        else
        {
            // Handle unsupported types or errors
            free_instance_section(section);
            return NULL;
        }

        return section;
    }
    ```

4.  **Merge `save_blueprint_section` logic:**
    *   Integrate the `else if (bs->type == BSTYPE_MAZE)` block from `src_20_dev/blueprint.c` into the `save_blueprint_section` function.
    *   This ensures that maze-specific attributes (`MazeW`, `MazeH`, `MazeFixedRoom`, `MazeTemplate`) are correctly written back to disk when blueprints are saved.

### 2.4. `src/dungeon.c`

The logic in `src_20_dev/dungeon.c` is crucial for supporting different instancing models. It should be ported to `src/dungeon.c`.

**Proposed Changes:**
*   **Port Dungeon Flags:** Add `DUNGEON_SHARED` and `DUNGEON_SOLO_INSTANCE` flags to `merc.h` and the `dungeon_flags` table in `tables.c`.
*   **Port Core Functions:** Port key functions from `src_20_dev/dungeon.c`, including:
    *   `find_dungeon_byplayer()`: To locate existing dungeons a player might be in.
    *   `spawn_dungeon_player()`: The main entry point for a player to enter a dungeon, which handles logic for creating a new solo instance or finding an existing shared one.
    *   `dungeon_addowner_player()` and `dungeon_removeowner_player()`: To manage ownership for solo instances.
    *   `dungeon_update()`: To handle the idle timeout and cleanup of expired solo instances.

## 3. Dungeon Instancing Models

The enhanced dungeon system will support two primary instancing models, controlled by flags in the `DUNGEON_INDEX_DATA`.

### 3.1. Shared Instances (`DUNGEON_SHARED`)

*   **Use Case:** For persistent, public dungeons like the **Pyramid of the Abyss**.
*   **Lifecycle:**
    1.  **Creation:** The dungeon instance is created only once, when the first player enters it after a reboot.
    2.  **Persistence:** The instance remains loaded in memory as long as the MUD is running. All players who enter the Pyramid will be sent to this single, shared instance.
    3.  **Destruction:** The instance is destroyed only on a server reboot or manual intervention by an immortal.

### 3.2. On-Demand Solo Instances (`DUNGEON_SOLO_INSTANCE`)

*   **Use Case:** For temporary, private dungeons like **Geldoff's Maze**.
*   **Lifecycle:**
    1.  **Creation:** A new, unique instance of the dungeon is created on-demand whenever a player (or group leader) enters it, provided they don't already have one active.
    2.  **Ownership:** The instance is "owned" by the player or group that triggered its creation. Only they can enter it.
    3.  **Idle Timeout:** The instance monitors for player presence. Once the last "owner" leaves the instance, an idle timer begins. If the timer expires before an owner returns, the instance is automatically destroyed, freeing up memory.

## 4. Data File Creation (Conceptual JSON)

**IMPORTANT NOTE:** Blueprints, blueprint sections, and dungeons are now *area-scoped* and are serialized directly within their respective area's main JSON file (e.g., `PoA.json` or `geldmaze.json`). They do not exist as separate top-level files or in dedicated `area/blueprints/` or `area/dungeons/` folders.

These definitions will be embedded as JSON arrays within the overall area definition.

### 4.1. Blueprint Sections for Mazes

These define the characteristics of the maze sections themselves. They would be part of the `blueprint_sections` array within an area's JSON.

#### 4.1.1. Geldoff's Maze Blueprint Section (within `geldmaze_area.json`)

This section defines the dynamic characteristics of Geldoff's Maze. The specific `vnum` values for `MazeTemplate` should refer to existing room `vnum`s that define the "Within a Misty Maze" description and general room properties.

```json
// Example of how this would appear within geldmaze_area.json
{
  "blueprint_sections": [
    {
      "vnum": <new_vnum_for_geldmaze_section>,
      "name": "Geldoff's Misty Maze Section",
      "description": "A procedurally generated misty maze.",
      "type": "maze",
      "flags": [],
      "maze_w": 10,
      "maze_h": 10,
      "recall_room": "<area_uid_of_this_area>#<existing_recall_vnum>",
      "maze_templates": [
        { "weight": 100, "vnum": 300001 } // Use room vnum 300001 as the template for all rooms
      ],
      "maze_fixed_rooms": [
        { "x": 1, "y": 1, "vnum": 300001, "connected": true } // Fix starting room (1,1)
      ],
      "links": [
        // Define exit links if needed for section connections
      ]
    }
  ],
  // ... other area data (rooms, mobiles, objects, etc.)
}
```

#### 4.1.2. Pyramid of the Abyss Level Blueprint Sections (within `pyramid_area.json`)

Five similar sections would be created, one for each level (1-5), embedded within the `pyramid_area.json`. Each would specify its unique `maze_w`, `maze_h`, and `maze_templates` rooms.

**Example for Level 1 (within `pyramid_area.json`):**
```json
// Example of a blueprint section for Pyramid Level 1
{
  "blueprint_sections": [
    {
      "vnum": <new_vnum_for_pyramid_level_1_section>,
      "name": "Pyramid of the Abyss Level 1 Section",
      "description": "The first level of the dark pyramid maze.",
      "comments": "Dynamically generated level 1 of the Pyramid of the Abyss.",
      "type": "maze",
      "flags": [],
      "maze_w": 14, // From maze.h level_table
      "maze_h": 5,  // From maze.h level_table
      "recall_room": "<area_uid_of_this_area>#150000", // A general recall point for this level
      "maze_templates": [
        { "weight": 33, "vnum": 150000 }, // Dark Tunnel
        { "weight": 33, "vnum": 150001 }, // Damp Room
        { "weight": 34, "vnum": 150002 }  // Sudden End
      ],
      "maze_fixed_rooms": [
        { "x": 1, "y": 1, "vnum": 150000, "connected": true } // Fixed starting room (1,1)
      ]
    },
    // ... blueprint sections for Level 2, 3, 4, 5 ...
  ],
  // ... other area data
}
```
*(Repeat for levels 2-5, adjusting `maze_w`, `maze_h`, `recall_room`, and `maze_templates` vnums accordingly based on the values in `maze/maze.h` and the original `.are` files.)*

### 4.2. Dungeon Definitions for Mazes

These define how the blueprint sections are used to construct full dungeons. They would be part of the `dungeons` array within an area's main JSON file.

#### 4.2.1. Geldoff's Maze Dungeon (within `geldmaze_area.json`)

This defines Geldoff's Maze as a solo instance. The `DUNGEON_SOLO_INSTANCE` flag is key.

```json
// Example of a dungeon definition for Geldoff's Maze
{
  "dungeons": [
    {
      "vnum": <new_vnum_for_geldmaze_dungeon>,
      "name": "Geldoff's Misty Maze",
      "description": "A magical misty maze, inhabited by Geldoff the Warlock.",
      "flags": ["solo_instance"], // Flag as an on-demand, private instance
      "entry_room": "<area_uid_of_this_area>#<existing_entry_room_vnum>",
      "exit_room": "<area_uid_of_this_area>#<existing_exit_room_vnum>",
      "zone_out": "<destination_vnum>~",
      "floors": [
        "<area_uid_of_this_area>#<vnum_of_geldmaze_section>" // Link to the blueprint section
      ]
    }
  ],
  // ... other area data
}
```

#### 4.2.2. Pyramid of the Abyss Dungeon (within `pyramid_area.json`)

This defines the Pyramid as a shared, multi-level dungeon. The `DUNGEON_SHARED` flag is key.

```json
// Example of a dungeon definition for Pyramid of the Abyss
{
  "dungeons": [
    {
      "vnum": <new_vnum_for_pyramid_dungeon>,
      "name": "The Pyramid of the Abyss",
      "description": "A multi-level, procedurally generated pyramid dungeon.",
      "flags": ["shared"], // Flag as a persistent, shared instance
      "entry_room": "<area_uid_of_this_area>#<existing_entry_room_vnum>",
      "exit_room": "<area_uid_of_this_area>#<existing_exit_room_vnum>",
      "zone_out": "<destination_vnum>~",
      "floors": [
        "<area_uid_of_this_area>#<vnum_of_pyramid_level_1_section>",
        "<area_uid_of_this_area>#<vnum_of_pyramid_level_2_section>",
        "<area_uid_of_this_area>#<vnum_of_pyramid_level_3_section>",
        "<area_uid_of_this_area>#<vnum_of_pyramid_level_4_section>",
        "<area_uid_of_this_area>#<vnum_of_pyramid_level_5_section>"
      ],
      "special_exits": [
        {
          "name": "level_1_down",
          "mode": "static",
          "from": [ { "weight": 1, "level": 1, "door": 5 } ], // From floor 1, exit 5
          "to": [ { "weight": 1, "level": 2, "door": 4 } ]    // To floor 2, entrance 4
        }
        // ... additional special_exits entries to link all levels ...
      ]
    }
  ],
  // ... other area data
}
```

## 5. Cleanup (Post-Implementation)

Once the new system is fully implemented and tested, the legacy components can be removed.

**Proposed Script: `scripts/cleanup_legacy_maze.sh`**

```bash
#!/bin/bash

# Remove the entire legacy maze directory
rm -rf maze/

# Remove old static maze area files
rm -f area/geldmaze.are
rm -f area/maze1.are
rm -f area/maze2.are
rm -f area/maze3.are
rm -f area/maze4.are
rm -f area/maze5.are

# Remove the legacy maze creation script
rm -f scripts/create_maze

# Modify area/startup to remove the call to create_maze
sed -i '/echo "Regenerating the mazes..."/,/.\/create_maze/d' area/startup
```

## 6. Testing Strategy (Conceptual)

After implementing these changes, thorough testing will be required:

*   **Unit Tests:** Ensure `blueprint_section_generate_maze` correctly generates mazes.
*   **Integration Tests:**
    *   **Geldoff's Maze:** Verify it generates a new instance for each player/group. Confirm it times out and gets destroyed after being empty.
    *   **Pyramid of the Abyss:** Verify it generates only once. Confirm that multiple players entering the portal are put into the same instance.
*   **Performance Tests:** Monitor performance during maze generation and ensure it does not introduce significant lag.