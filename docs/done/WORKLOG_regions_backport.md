# Worklog: Region System Backport

**Date:** 2026-02-09

## Objective

Analyze the "region" systems in `src_20_dev`, compare them to the current implementation in `src`, and document a plan for backporting the functionality.

## Work Performed

1.  **Initial Investigation:**
    -   Searched the `src_20_dev` directory for the term "region" to identify all relevant files.
    -   Key files identified: `merc.h`, `olc_act.c`, `wilds.h`, `wilds.c`, and various `script_*.c` files.

2.  **Data Structure Analysis (`src_20_dev`):**
    -   Examined `src_20_dev/merc.h` to understand the core data structures.
    -   Identified `struct area_region_data` (`AREA_REGION`), which defines sub-zones within static areas.
    -   Identified `struct wilds_region` (`WILDS_REGION`), which defines rectangular zones on wilderness maps.

3.  **OLC Management Analysis (`src_20_dev`):**
    -   Reviewed `src_20_dev/olc_act.c`.
    -   Analyzed the `aedit regions` command, which manages `AREA_REGION`s (add, remove, name, describe, set properties).
    -   Analyzed the `redit region` command, which assigns rooms to a specific `AREA_REGION`.
    -   Analyzed the `wedit region` command, which manages `WILDS_REGION`s on wilderness maps (add, remove, set default).

4.  **Current State Analysis (`src`):**
    -   Searched the `src` directory for "region".
    -   Confirmed that `AREA_REGION` and `WILDS_REGION` structs are **not** present.
    -   Found that the `REGION_*` enum is used for basic geographical classification of areas.
    -   Noted the absence of the advanced OLC commands (`aedit regions`, `wedit region`, etc.).
    -   Identified a specialized, limited `shipyard_region` feature that is separate from the main region concept.

5.  **Documentation and Planning:**
    -   Synthesized findings into a cohesive analysis.
    -   Created the `src/docs/PLAN_regions_backport.md` document.
    -   This document details the `src_20_dev` implementation, the deficiencies in `src`, and a comprehensive, step-by-step plan for porting the functionality.

## Next Steps (Historical)

These steps were fully completed in subsequent updates below.

---

## Implementation Update (2026-02-19)

Completed a **Phase 1 wilderness-focused backport** to establish Wilds V3 groundwork without pulling in the full legacy AREA_REGION scripting surface yet.

### Implemented

1. **Wilderness region core type/model restored**
    - Added `WILDS_REGION` type wiring and wilds-side storage fields:
      - `WILDS_DATA.defaultRegion`
      - `WILDS_DATA.defaultPlaceFlags`
      - `WILDS_DATA.pRegion`

2. **Wilds region parsing and persistence in legacy wilds blocks**
    - Added load support for:
      - `DefaultRegion`
      - `DefaultPlace`
      - `#REGION ... #-REGION`
    - Added save support for those same fields/records.

3. **Runtime APIs restored**
    - Added region helpers in `wilds.c`:
      - `get_region_by_coors()`
      - `fread_region()` / `fwrite_region()`
      - `add_region()` / `del_region()`
      - `new_region()` / `free_region()`

4. **Region flag table restored**
    - Added `wilderness_regions[]` in `tables.c` + declaration in `tables.h`.

5. **Region lookup now data-driven first**
    - Updated `get_region_wyx()` in `handler.c` to:
      1. Check loaded wilds region rectangles/defaults first
      2. Fall back to existing hardcoded `wuid == 6` mapping for compatibility

### Validation

- Performed Debug build/test pass from task runner with no compile errors reported.

### Remaining (Planned at the time)

This list is retained for historical context; all items were completed in later phases documented below.

---

## Implementation Update (2026-02-19, Phase 2A/2B)

Completed the editor/runtime backport steps for area + wilderness region editing, with broad command parity where the current branch architecture supports it cleanly.

### Implemented

1. **AREA_REGION core wiring (Phase 2A)**
    - Added `AREA_REGION` type and struct in `merc.h`.
    - Added `AREA_DATA.region`, `AREA_DATA.regions`, `AREA_DATA.top_region_uid`.
    - Added `ROOM_INDEX_DATA.region`.

2. **AREA_REGION lifecycle (Phase 2A)**
    - Added `new_area_region()` / `free_area_region()` in `mem.c`.
    - Initialized default area region + custom region list in `new_area()`.
    - Added cleanup in `free_area()`.

3. **Runtime helpers (Phase 2A)**
    - Added `get_room_region()` and `get_area_region_by_uid()` in `handler.c`.
    - Added `area_region_add_room()` / `area_region_remove_room()` helpers for safe room reassignment.

4. **Area editor backport (Phase 2B)**
    - Added `aedit regions` command in `editors/areas/aedit.c` with:
      - `list`
      - `add`
      - `remove`
      - `name`
      - `description`
      - `comments`
      - `flags`
      - `who`
      - `place`
    - Added region-default summary to `aedit show`.

5. **Room editor backport (Phase 2B)**
    - Added `redit region` command in `editors/rooms/redit.c`.
    - Added current region display in `redit show` (General tab).

6. **Wilderness editor backport (Phase 2B)**
    - Added `wedit region` command in `editors/wilderness/wedit.c` with:
      - `list`
      - `default`
      - `add`
      - `remove`
    - Added `wedit placetype` for wilds default place type.
    - Added default region/place display in `wedit show` (General tab).

7. **Flag/help table parity for OLC**
    - Added `area_region_flags[]` in `tables.c` + declaration in `tables.h`.
    - Registered `area_region_flags` and `wilderness_regions` in `olc_act.c` help lookup table (`?` queries).

### Validation

- Built Debug successfully after integration.
- Diagnostics show no errors in touched files.

---

## Implementation Update (2026-02-19, Phase 3 Persistence)

Implemented JSON persistence for `AREA_REGION` in the area JSON pipeline (`io/json/json_area.c`).

### Implemented

1. **Area-level region persistence**
    - Added `default_region` object serialization/deserialization.
    - Added `regions` array serialization/deserialization for custom regions.
    - Added `top_region_uid` persistence.

2. **Room-to-region persistence**
    - Added `region_uid` on room JSON records for non-default region assignment.
    - On load, rooms are assigned to default region first, then remapped to custom region when `region_uid` is present.

3. **Persisted region fields (JSON)**
    - `uid`, `name`, `description`, `comments`
    - `area_who`, `flags`, `place_flags`
    - `recall`
    - `coordinates` (`x`, `y`, `land_x`, `land_y`)
    - `airship_land`, `post_office`

### Scope decisions

- Did **not** introduce/expand unsupported gameplay semantics (e.g., savage mechanics).
- Persistence covers data model + assignment round-trip only; gameplay behavior remains tied to existing runtime systems.

### Validation

- Debug build completes successfully after persistence changes.
- `json_area.c` diagnostics report no errors.

---

## Closeout (2026-02-19)

Region backport work is being **called done for now**.

### Final state for this tranche

- Core wilderness and area region models are backported.
- OLC editing paths for regions are available.
- JSON persistence for area regions and room region assignment is in place.
- Build validation passed for implemented changes.

### Explicitly deferred (not required for this closeout)

- Region gameplay semantics that are not currently supported in runtime systems (example: savage behavior).
- Any additional region command parity beyond the currently implemented set.
- Broad region adoption across unrelated systems unless tied to a concrete feature request.

### Follow-up trigger

Resume only when a concrete feature requires it (for example, a specific gameplay subsystem or editor workflow that must become region-aware).
