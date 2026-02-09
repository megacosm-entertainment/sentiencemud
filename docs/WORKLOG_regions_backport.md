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

## Next Steps

-   Await approval of the backporting plan.
-   Begin implementation as outlined in `src/docs/PLAN_regions_backport.md`.
