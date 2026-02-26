# Region System Backport Analysis and Plan

## Status (2026-02-19)

This backport is **complete for the current milestone** and is being called done for now.

Completed in this milestone:
- Wilderness region core model and legacy wilds persistence path
- `AREA_REGION` core structs, lifecycle, and runtime lookup helpers
- OLC region management commands (`aedit regions`, `redit region`, `wedit region`, `wedit placetype`)
- JSON persistence for area regions and room-to-region assignment in `io/json/json_area.c`

Deferred by design (future work, optional):
- Additional region gameplay semantics not currently supported in runtime (e.g. savage behavior)
- Expanded per-region OLC fields beyond currently backported command set
- Broader system adoption of area/wilds region data in unrelated subsystems

This document records the analysis that guided the region backport from `src_20_dev` into `src`.

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

## 2. Current State in `src` (After Backport)

The region system is now present and operational in the active branch:

- `AREA_REGION` and `WILDS_REGION` data models are available.
- Areas support a default region plus custom region lists.
- Rooms support explicit region assignment.
- Wilderness supports rectangular region definitions and defaults.
- OLC surfaces for region editing are in place.
- JSON area persistence includes region definitions and room `region_uid` assignment.

## 3. Milestone Outcome

### Completed

1. **Core model and lifecycle**
   - Region structs/fields, allocation/free, and runtime lookup helpers were backported.

2. **Editor support**
   - Region editing commands are available in area/room/wilderness editors for current milestone needs.

3. **Persistence**
   - Area JSON now persists default/custom regions and room region assignments.
   - Wilds region persistence exists in the wilds serialization path.

4. **Compatibility posture**
   - Existing behavior remains intact where region-aware logic is not yet adopted.

### Deferred / Future Candidates

- Add additional region-aware gameplay semantics only where runtime support exists and is desired.
- Expand region command surface if needed by builders.
- Introduce wider subsystem adoption of region data (selectively, based on concrete feature goals).

This plan is now considered **executed for this tranche**.
