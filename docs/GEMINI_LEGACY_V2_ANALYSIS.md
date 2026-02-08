# Analysis of `src_20_dev` vs. `src`

This document outlines the findings from an analysis of the `src_20_dev` directory, comparing it to the current `src` directory to identify potential improvements and changes to be integrated.

## `widevnum` vs. `vnum`

The `widevnum` system in `src_20_dev` is a significant architectural improvement over the `vnum` system in `src`. It introduces a globally unique identifier for objects, rooms, and mobs, which will be crucial for more robust scripting and data persistence.

*   **Implementation:**
    *   The core of the `widevnum` system is defined in `src_20_dev/merc.h` with the `WNUM` (runtime) and `WNUM_LOAD` (saved data) structs.
    *   This system combines an `area_id` with a `vnum` to create a unique reference (e.g., `area_id#vnum`).
    *   Key functions for handling these new identifiers include `parse_widevnum`, `fread_widevnum`, and `widevnum_string`.

*   **Benefit:**
    *   Prevents `vnum` conflicts between different areas.
    *   Provides a more stable and reliable way to reference entities across the game world.

## Data-Driven Race System

A surprising finding is that the `src` directory *already* contains a modern, data-driven race system. This was not a legacy hardcoded system as initially thought.

*   **Implementation:**
    *   The file `src/race.c` contains functions like `load_races_json` and `save_race_json`.
    *   These functions manage race definitions from JSON files, allowing for easy modification and expansion of races without recompiling the server.

*   **Conclusion:**
    *   The task for race systems is not to upgrade a legacy system, but to compare the two different data-driven implementations in `src` and `src_20_dev` and decide on the best approach forward.

## Further Analysis

Further analysis for Class and Progression Systems, Scripting Changes, and other useful changes can be found in their respective `GEMINI_<topic>.md` files in this directory.
