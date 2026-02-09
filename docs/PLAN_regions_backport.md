# Region System Backport Analysis and Plan

This document analyzes the "region" systems implemented in the `src_20_dev` branch and proposes a plan to backport them to the main `src` branch.

## 1. Summary of Region Systems in `src_20_dev`

The `src_20_dev` codebase contains two distinct but related "region" systems:

### 1.1. Area Regions (`AREA_REGION`)

This system allows for the creation of "sub-zones" within a standard, room-based area.

- **Data Structure (`struct area_region_data` in `merc.h`):**
    - Each area has a default `AREA_REGION` and a list (`LLIST`) of additional regions.
    - Each region has its own `uid`, `name`, `description`, `recall` location, `post_office`, `flags`, and coordinate data for wilderness mapping.
    - It maintains a list of rooms and players currently within it.
    - It separates OLC (reset-state `rs_`) data from live data, allowing for temporary, runtime changes.

- **Management (OLC in `olc_act.c`):**
    - The `aedit regions` command provides a comprehensive suite of tools for builders to manage these regions.
    - Builders can `add`, `remove`, `name`, and `describe` regions.
    - They can assign properties like `recall`, `airship landing spots`, `place_flags`, and `savage` levels.
    - The `redit region` command allows builders to assign a specific room to a defined area region.

- **Purpose:**
    - To logically group rooms within a larger area.
    - To create named sub-zones with their own characteristics (e.g., a "market district" within a city with a specific recall point).
    - To enable more granular control over area behavior through scripting and game logic.

### 1.2. Wilderness Regions (`WILDS_REGION`)

This system defines rectangular zones on a large, coordinate-based wilderness map.

- **Data Structure (`struct wilds_region` in `wilds.h`):**
    - Defines a rectangle using `startx`, `starty`, `endx`, and `endy` coordinates.
    - Assigns a `region` type (e.g., `REGION_FIRST_CONTINENT`, `REGION_NORTHERN_OCEAN`) from a global enum.
    - Assigns `area_place_flags` to the zone, influencing game mechanics within that rectangle.

- **Management (OLC in `olc_act.c`):**
    - The `wedit region` command is used to manage these regions on a specific wilderness map.
    - Builders can `add` new rectangular regions, specifying their coordinates and type.
    - They can `remove` existing regions or set a `default` region for the entire map.

- **Purpose:**
    - To give geographical and political identity to different parts of a large, seamless wilderness map.
    - To control game mechanics (like allowed travel or weather) based on a player's location.
    - The documentation also suggests this was intended to be used for performance optimization, allowing a web client to load the map in chunks (regions).

## 2. Current State of Regions in `src`

The `src` codebase has a significantly stripped-down implementation:

- **No `AREA_REGION` or `WILDS_REGION` Structs:** The core data structures for both systems are completely absent from `merc.h`.
- **Enum Only:** The `REGION_*` enum (e.g., `REGION_FIRST_CONTINENT`) exists, but it's used as a simple classification for an entire `AREA_DATA`, derived from the area's `place_flags` or its x/y coordinates.
- **No OLC Management:** The `aedit regions`, `redit region`, and `wedit region` commands do not exist. There is no way for builders to create or manage sub-zones or wilderness zones.
- **Legacy Hints:** Comments in `act_info.c` and documentation in `src/docs/` refer to a more advanced wilderness system, indicating that the feature was either abandoned, removed, or never fully ported from `src_20_dev`.
- **Specialized `shipyard_region`:** A simple integer array `shipyard_region[2][2]` exists on the `SHOP_DATA` struct to define a rectangular area for shipyards. This is a very limited, feature-specific implementation and not a general-purpose region system.

## 3. Backporting Plan

The region systems in `src_20_dev` are well-developed and align with the apparent design goals found in the `src` documentation. Backporting them would provide significant new functionality for builders and enable future features like a web-based map.

The following steps are proposed:

1.  **File & Structure Migration:**
    -   Copy `struct area_region_data` and `struct wilds_region` definitions from `src_20_dev/merc.h` to `src/merc.h`.
    -   Add `AREA_REGION region;` and `LLIST *regions;` to the `AREA_DATA` struct in `src/merc.h`.
    -   Add `WILDS_REGION *pRegion;` to the `wilds_data` struct in `src/wilds.h`.

2.  **Memory Management & Initialization:**
    -   Port `new_area_region()`, `free_area_region()`, `new_region()`, and `free_region()` functions from `src_20_dev/mem.c` and `src_20_dev/wilds.c` to their counterparts in `src`.
    -   Update the `new_area()` function in `src/mem.c` to initialize the default region and the regions list.

3.  **OLC Command Integration:**
    -   Copy the `aedit_regions`, `redit_region`, and `wedit_region` functions from `src_20_dev/olc_act.c` into the appropriate files in `src/editors/`.
    -   Add the new commands to the corresponding command tables in `olc.c`.

4.  **Persistence:**
    -   Port the save/load logic for area regions and wilds regions from `src_20_dev/olc_save.c` and `src_20_dev/wilds.c` to the JSON-based I/O functions in `src/io/json/`. This will require adapting the logic to work with the Jansson library instead of the old `fprintf`-based format.
    -   Update the area JSON schema to include a `regions: []` array and a default region object.
    -   Update the wilds JSON schema to include a `regions: []` array.

5.  **Supporting Functions:**
    -   Port `get_room_region()`, `get_area_region_by_uid()`, and `get_region_by_coors()` from `src_20_dev/handler.c` and `src_20_dev/wilds.c` to `src/handler.c`.
    -   Review all files that use the `REGION_*` enum and update them to use the new, more powerful region structs and functions where appropriate.

6.  **Build System Update:**
    -   Update `Makefile` and `CMakeLists.txt` to include any new `.c` files that are created or copied during this process.

By following this plan, we can re-introduce the powerful and flexible region systems, enabling more dynamic and interesting world-building possibilities.
