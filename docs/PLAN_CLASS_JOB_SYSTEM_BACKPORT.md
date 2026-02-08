# PLAN: Class and Job System Backport

## Objective

To backport the "Final Fantasy style 'job' system" from `src_20_dev/` to the main `src/` directory, replacing the existing rigid class/subclass/remort system. This also includes modifying the concept of "remort" to be an in-play race change mechanism with account-level unlocking for new characters.

## Current System (`src/`) Overview

*   **Classes:** Characters have 4 base classes (Mage, Cleric, Thief, Warrior).
*   **Subclasses:** Each base class has 3 subclasses, each with 30 levels.
*   **Progression:** Characters level one class at a time, gaining abilities from one class to the next. Total level is 120 (30 in each of 4 classes).
*   **Remort:** At level 120, characters can "remort," resetting to level 1 with a new remort race and selecting new remort subclasses. This is a one-time class reset event.
*   **Implementation:** Heavily hardcoded class checks (`if (ch->class == CLASS_WARRIOR)`) and `IS_REMORT(ch)` macros are prevalent. Class and subclass levels are stored in individual `int` fields in `PC_DATA`.
*   **Persistence:** Uses JSON for character, NPC, race, and trait data. Key patterns observed in `src/io/json/json_char.c` and `src/io/json/json_race.c`:
    *   **JSON Library:** Utilizes `jansson.h` for all JSON parsing and generation.
    *   **Structure:** Data is organized into top-level objects with sections like "metadata", "character", "inventory", "equipment", "skills", etc.
    *   **Identifiers:** Human-readable names (e.g., skill names, flag names, race IDs) are often used as keys, alongside numeric UIDs where necessary.
    *   **Flags/Bitvectors:** Stored as JSON arrays of human-readable flag names (e.g., "affected_by_flags": ["invis", "fly"]) and often duplicated as numeric values for backward compatibility during transitions. Utility functions like `flags_to_json_array` and `flags_from_json_array` are used.
    *   **Linked Lists (`LLIST`):** Serialized as JSON arrays, with each element becoming a JSON object. This is seen for inventory, equipment, affects, and racial skills.
    *   **Widevnums (`WNUM`):** Used for unique identification of game entities (objects, rooms) across different areas, stored as strings ("area_id:vnum").
    *   **Migration:** Includes logic for `backup_old_pfile` if data is not in JSON format, demonstrating a pattern for handling format transitions.
*   **Key Files:** `merc.h`, `skills.c`, `save.c`, `act_info.c`, `interp.c`, `const.c`, `handler.c`, `io/json/json_char.c`, `io/json/json_race.c`, `io/json/json_persist.h`.
*   **Data Structures:** `struct class_type`, `struct sub_class_type`, `PC_DATA` (multiple `int` fields for class/subclass levels).

## New System (`src_20_dev/`) Overview

*   **"Job" System:** Flexible, data-driven system where "jobs" are referred to as "classes." Players can freely switch between active classes.
*   **Independent Leveling:** Jobs (classes) can have varying maximum levels, and players level them independently.
*   **Abilities:** Players gain abilities from their current active class and potentially "cross-class" skills/traits.
*   **Data-Driven:** Classes are defined in external JSON-like files (`classes.dat`) and are manageable via an Online Creation (OLC) editor.
*   **Implementation:** Replaces hardcoded class checks with flexible lookups and flag-based checks on `CLASS_DATA` objects.
*   **Key Files:** `merc.h`, `classes.h`, `skills.c`, `save.c`, `olc.c`, `act_info.c`.
*   **Data Structures:** `CLASS_DATA` (new, defines a class), `CLASS_LEVEL` (links character to a class and their level/XP), `PC_DATA` (contains `LLIST *classes;` and `CLASS_LEVEL *current_class;`), `ACCOUNT_DATA` (likely new field for unlocked remort races).

## Backporting Plan

This plan is divided into two major parts: **Part A: Backport the New Class/Job System** and **Part B: Implement New "Remort" Race Change System**.

### Part A: Backport the New Class/Job System

This part focuses on integrating the flexible class system from `src_20_dev/` into `src/`, replacing the current hardcoded class/subclass structure.

1.  **Introduce New Core Data Structures and Helper Functions:**
    *   **Copy Struct Definitions:** Transfer `CLASS_DATA` and `CLASS_LEVEL` struct definitions from `src_20_dev/merc.h` to `src/merc.h`. Note: The `CLASS_LEVEL` struct will be extended to include a `pVARIABLE custom_data;` field to store custom, class-specific data (e.g., a ranger's pets).
    *   **Update `PC_DATA`:** In `src/merc.h`, add `LLIST *classes;` (list of `CLASS_LEVEL` objects) and `CLASS_LEVEL *current_class;` to the `PC_DATA` struct.
    *   **Deprecate Old Fields:** Use `#if 0` to comment out or remove the old `class_mage`, `sub_class_thief`, `class_current`, `sub_class_current`, etc., fields within `src/merc.h`'s `PC_DATA` struct, mirroring `src_20_dev/`.
    *   **Global Class Lookup System:** Instead of global `CLASS_DATA` pointers, implement a lookup system mirroring that of `RACE_DATA`. This will involve:
        *   A global `LLIST *classes_list;` to hold all `CLASS_DATA` objects.
        *   A hash table (`CLASS_HASH_ENTRY *class_hash_table[CLASS_HASH_SIZE];`) for efficient string ID lookups.
        *   A UID index (`static CLASS_DATA **class_uid_index;`) for numeric UID lookups.
        *   Corresponding helper functions (`class_lookup`, `class_lookup_uid`, `class_lookup_name`).
        *   Remove `CLASS_DATA **gcl;` from the `CLASS_DATA` struct definition in `src_20_dev/merc.h` before copying.
    *   **Class Helper Functions:** Copy core class management helper functions (`get_class_data`, `get_class_uid`, `get_current_class`, `get_class_level`, `has_class_level`, `add_class_level`, `remove_class_level`) from `src_20_dev/skills.c` to `src/skills.c`.

2.  **Port Class Data Persistence and Management (JSON-based):**
    *   **JSON Serialization/Deserialization:** Develop new functions for serializing and deserializing `CLASS_DATA` and `CLASS_LEVEL` structs to and from JSON format. This will include handling the `pVARIABLE custom_data` field within `CLASS_LEVEL`, leveraging existing script variable serialization patterns (e.g., from `json_persist_scriptdata_to_json`).
    *   **Integrate with Existing JSON:** Integrate these new JSON functions into the existing JSON persistence system used for characters, NPCs, races, and traits. This means `CLASS_DATA` would be loaded/saved as part of relevant parent JSON objects (e.g., character data, or a standalone `classes.json` if appropriate within the existing JSON framework).
    *   **Bootstrapping:** The system should bootstrap default classes from `__sub_class_table` in `skills.c` if no class data is found in the JSON persistence system.
    *   **`CLASSES_FILE` Macro:** The `CLASSES_FILE` and `SYSTEM_DIR` macro definitions in `src/merc.h` will likely become obsolete or need to be re-purposed to point to a `classes.json` file, or the class data will be embedded within other existing JSON files. This will be determined upon investigation of the current JSON implementation.

3.  **Integrate Online Creation (OLC) Editor:**
    *   **`do_clsedit`:** Port the `do_clsedit` command from `src_20_dev/olc.c` (or `src_20_dev/skills.c` if its definition is there) to `src/olc.c`.
    *   **OLC Helpers:** Copy relevant OLC helper functions and data structures required for class editing (e.g., `OLC_FUN(clsedit_name)`, `olc_table` entries).

4.  **Adapt Character Loading, Saving, and Migration (Class System):**
    *   **Update `save.c` (JSON Integration):** Modify `src/save.c` (or relevant character JSON save/load functions) to correctly serialize and deserialize the new `LLIST *classes` and `CLASS_LEVEL *current_class` fields within `PC_DATA` using the existing JSON framework.
    *   **Migration Functionality (JSON Output):** Implement a robust migration path. When an older character pfile (using the deprecated integer class fields, potentially from a non-JSON format) is loaded, a function will convert these old values into the new JSON-based `LLIST *classes` structure. This will involve adapting functions like `__add_class_level` and `fix_broken_classes` found in `src_20_dev/save.c` to produce JSON-compatible output for the new class data. This is a critical step to ensure backward compatibility and data integrity during the transition.

5.  **Port Player-Facing Commands (Class System):**
    *   **`do_classes`:** Copy the `do_classes` command (for listing a character's known classes) from `src_20_dev/act_info.c` to `src/act_info.c`.
    *   **`do_setclass`:** Copy the `do_setclass` command (for switching the current active class) from `src_20_dev/skills.c` to `src/skills.c`.
    *   **Character Creation:** Update relevant character creation functions in `nanny.c` to utilize the new class selection process and data structures.

6.  **Refactor Core Game Logic (Class System - Extensive):**
    *   **Replace `sub_class_table` Accesses:** Identify and replace all direct accesses to the global `sub_class_table` and `class_table`. This logic must be re-implemented using `get_class_data` or by iterating through the `classes_list` and checking `CLASS_DATA` properties (`clazz->type`, `clazz->flags`).
    *   **Remove `MAX_SUB_CLASS`:** The constant `MAX_SUB_CLASS` (24) is specific to the old system and should be removed or made obsolete in favor of the dynamic `classes_list`.
    *   **Obsolete `get_profession`:** The `get_profession` function, which relies on hardcoded integer class types, should be replaced entirely by calls to `get_class_level` and checks against `CLASS_DATA` object properties.
    *   **Update Experience Gain:** Modify `gain_exp` (likely in `src/update.c` or `src/skills.c`) to operate on a `CLASS_DATA *clazz` and `CLASS_LEVEL *level` for targeted experience gain, reflecting the independent leveling of classes.
    *   **Custom Class Data Access:** Implement functions and mechanisms to access, modify, and manage the `pVARIABLE custom_data` stored within a `CLASS_LEVEL` object from various parts of the game logic (e.g., commands, triggers, scripts). This will enable features like a ranger's pet stable or other class-specific customizations.

### Part B: Implement New "Remort" Race Change System

This part redefines "remort" as an in-play race change mechanism and introduces account-level unlocking of remort races.

1.  **Account Setting for Unlocked Races:**
    *   **`ACCOUNT_DATA` Modification:** Add a new field to the `ACCOUNT_DATA` struct in `src/merc.h`. This could be an `LLIST *unlocked_remort_races;` (list of `RACE_DATA *` or race IDs) or a `long` bitvector if the number of remort races is limited and fits a bitmask.
    *   **Persistence (JSON Integration):** Modify account loading/saving mechanisms in `src/save.c` (or relevant account JSON save/load functions) to persist this new `unlocked_remort_races` field using the existing JSON framework.
    *   **Administration Tool:** Implement an OLC command or a new administrative command (e.g., `acctedit races unlock`) to allow staff to manage which remort races are unlocked at the account level.
    *   **Character Creation Check:** Modify the character creation process in `src/nanny.c` to check the `ACCOUNT_DATA`'s `unlocked_remort_races` when a player attempts to select a remort race, thereby gating access.

2.  **In-Play Race Change Mechanism:**
    *   **Define Remort Process:** Determine the specific conditions or achievements required for a character to become eligible for a race change (e.g., completing a special questline, reaching a certain `tot_level` across all classes, consuming a rare item).
    *   **`PC_DATA` Fields:** Add new fields to `PC_DATA` in `src/merc.h` to track eligibility for a race change (e.g., `bool can_remort_race_change;`, `RACE_DATA *pending_remort_race;` to store the race they can change into).
    *   **Race Change Command:** Implement a new player-facing command (e.g., `do_changerace` or `do_ascendrace`). This command, when invoked by an eligible player, will:
        *   Change `ch->race` to the target `RACE_DATA`.
        *   Trigger necessary updates: recalculate stats, skills, health/mana/move pools based on the new race.
        *   Potentially reset `ch->level` and `ch->exp` to 1 and 0, or apply other penalties/bonuses based on game design.
        *   Clear `can_remort_race_change` flag.

3.  **Refactor `IS_REMORT(ch)` and Related Logic:**
    *   **Review `IS_REMORT(ch)`:** Systematically review every instance of `IS_REMORT(ch)` throughout the codebase in `src/`.
    *   **Class-Specific Benefits:** If `IS_REMORT(ch)` was previously used to grant special abilities, bonuses, or access related to *remort classes*, this logic must be replaced. Instead, check the `flags` of the `CLASS_DATA` objects in the character's `LLIST *classes` (e.g., `if (IS_SET(current_class->clazz->flags, CLASS_REMORT_BONUS))`). New flags may need to be defined for this purpose within `CLASS_DATA`.
    *   **Race-Specific Checks:** If `IS_REMORT(ch)` was used to identify if the character's *race itself* is a "remort race" (e.g., for item restrictions, mob reactions), the underlying `race_is_remort((ch)->race)` call remains functionally relevant. However, the *context* and any associated game effects will need to be re-evaluated against the new "in-play race change" model.

### Testing and Validation

*   **Build Environment:** Ensure the project compiles successfully after all changes.
*   **Existing Tests:** Run all existing unit and integration tests from `src/tests/` (after building with `./build tests`) to catch regressions.
*   **New Tests (Unit & Integration):**
    *   Develop dedicated unit tests for the new `CLASS_DATA` and `CLASS_LEVEL` manipulation functions.
    *   Create integration tests for the full class system lifecycle: character creation, class leveling, class switching, skill acquisition, and interaction with various game mechanics.
    *   Implement tests for the race change mechanism: unlocking races via account settings, in-play eligibility, executing a race change, and verifying its effects (stats, skills, etc.).
    *   Test the character migration function thoroughly using old character pfiles to ensure data integrity.
*   **Comprehensive Functional Testing:** Perform manual walkthroughs of critical game loops: character creation, leveling, fighting, interacting with NPCs, using skills, accessing OLC editors, and attempting race changes under various conditions.

This will be a phased development process, beginning with the foundational data structures and persistence, then moving to core logic replacement, and finally implementing the new "remort" race change mechanics.

---
**File to be created:** `docs/PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`
**Content:** (The plan detailed above)
