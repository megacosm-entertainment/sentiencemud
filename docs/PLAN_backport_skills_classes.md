# Backport Analysis: Skill and Class System Refactors

This document provides an analysis of two major systems from the `src_20_dev` branch—the data-driven Skill System and the flexible Class/Job System—and outlines a plan for backporting them into the main `src` codebase.

---

## 1. Skill System Refactor

### 1.1. `src_20_dev` System Overview

The `src_20_dev` branch implements a modernized, data-driven skill system that replaces the legacy hardcoded `skill_table` array.

- **Core Components:**
    -   **JSON-based:** Each skill and spell is defined in its own JSON file (e.g., `fireball.json`), likely intended for a `data/skills/` directory. This decouples skill data from the C code.
    -   **Hash Table Backend:** On boot, the server reads all skill JSON files and populates a hash table for fast, O(1) lookups by skill name (ID). This eliminates the need for linear array scans (`skill_lookup`).
    -   **OLC Editor (`skedit`):** A comprehensive in-game editor allows builders to create, modify, and balance skills without recompiling the server. This includes setting everything from mana costs and target types to assigning C-level `spell_fun` pointers from a lookup table.
    -   **Decoupling from `sn`:** The ultimate goal of this refactor is to remove the integer "skill number" (`sn`) from the game entirely, passing `skill_t *` pointers directly to functions.

### 1.2. Current `src` System State

The `src` codebase uses the traditional, hardcoded Diku-style approach.

-   **`skill_table` Array:** All skills are defined in a massive, static `const struct skill_type skill_table[]` array in `const.c`.
-   **`gsn_` Globals:** Global integer variables (e.g., `gsn_fireball`) are used to hold the array index (`sn`) for each skill after being looked up at boot time.
-   **No In-Game Editor:** Adding or modifying skills requires changing `const.c` and recompiling the server.
-   **Brittle by Design:** The reliance on array indices (`sn`) is fragile. Changing the order of skills in the table or removing one can break player save files and hardcoded logic.

### 1.3. Skill System Backporting Plan

This plan follows the phases outlined in the original `PLAN_SKILL_REFACTOR.md`.

1.  **Phase 1: Hashed Skill Backend**
    -   Create `src/json_skill.c` and `src/json_skill.h`.
    -   Implement hash table utilities (`skill_hash`, `skill_hash_insert`).
    -   Create a `skill_load_json()` function to parse a single skill file.
    -   Convert the existing `skill_table` from `src/const.c` into individual JSON files in a new `data/skills/` directory.
    -   Implement `load_skills()` in `json_skill.c` to read the directory and populate the hash table and a global `skill_list`.
    -   In `db.c`, replace the old skill table initialization with a call to `load_skills()`.
    -   Move the `skill_type_lookup` function to `json_skill.c`, rename it to `skill_lookup`, and change it to use the new hash table.

2.  **Phase 2: In-Game Skill Editor (`skedit`)**
    -   Create `src/editors/skills/skedit.c`.
    -   Implement the `do_skedit` command and the `skedit()` interpreter function.
    -   Port the OLC sub-commands from `src_20_dev/skills.c` and `src_20_dev/olc.c` (e.g., `skedit show`, `create`, `name`, `target`, `spellfun`, etc.). This will require creating a lookup table to map function names (strings) to C function pointers.
    -   Implement `save_skills_json()` to write the in-memory skill data back to the individual JSON files.

3.  **Phase 3: Deprecation and Cleanup**
    -   Perform a codebase-wide refactor to replace all `int sn` parameters in spell functions with `const skill_t *skill`.
    -   Update all calls to these functions to pass the skill pointer instead of the `sn`.
    -   Once no code references the `skill_table` array or `gsn_` variables, remove them entirely from the codebase.

---

## 2. Class/Job System

### 2.1. `src_20_dev` System Overview

The `src_20_dev` branch features a flexible "job system" akin to those in Final Fantasy games, where classes are independent and can be switched.

- **Core Components:**
    -   **`CLASS_DATA`**: A struct defining a class template, including its name, flags, skill groups, and max level. These appear to be loaded from data files.
    -   **`CLASS_LEVEL`**: A linking struct on the player that stores their level and experience for a specific `CLASS_DATA`.
    -   **`PC_DATA` Modifications**: The `pc_data` struct contains `LLIST *classes` (a list of all `CLASS_LEVEL`s the player has) and `CLASS_LEVEL *current_class` (their active job).
    -   **OLC Editor (`clsedit`):** An in-game editor for creating and managing classes.

- **Functionality:**
    -   Players can switch their active class using the `do_setclass` command.
    -   Experience is gained only for the currently active class.
    -   The system is data-driven, removing hardcoded class limits and progression paths.
    -   It is designed to interact with other systems, like **Reputation**, for class requirements or benefits.

### 2.2. Current `src` System State

The `src` codebase uses a rigid, multi-tiered class system.

-   **Fixed Progression:** Characters progress through a fixed sequence: one of four base classes, followed by a subclass, then a second subclass, and finally a remort class.
-   **Hardcoded Integers:** The player's current class and subclass are stored as `int` fields (`class_current`, `sub_class_current`) in `pcdata`.
-   **`sub_class_table`**: A global array in `skills.c` defines all available subclasses, their prerequisites, and default skill groups.
-   **No Flexibility:** Adding new classes or changing the progression path requires significant code changes.

### 2.3. Class/Job System Backporting Plan

This is a major architectural change that must be carefully sequenced.

1.  **Core Data Structures:**
    -   Copy `CLASS_DATA` and `CLASS_LEVEL` struct definitions from `src_20_dev/merc.h` to `src/merc.h`.
    -   In `src/merc.h`, add `LLIST *classes;` and `CLASS_LEVEL *current_class;` to the `PC_DATA` struct.
    -   Use `#if 0` to comment out the old integer-based class fields (`class_current`, `sub_class_current`, etc.) in `PC_DATA`.

2.  **Class Loading and Management:**
    -   Create `src/classes.c` and `src/classes.h`.
    -   Implement a hash table-based lookup system for classes (`class_lookup`, `class_hash_table`), similar to the new skill and existing race systems.
    -   Implement `load_classes()` to read class definitions from a new `data/classes/` directory, which will contain individual JSON files for each class.
    -   Create a migration script or function to convert the data from the old `sub_class_table` in `src/skills.c` into the new JSON format.

3.  **Player Data and Commands:**
    -   Modify character saving/loading (`io/json/json_player.c`) to handle the new `classes` list, serializing and deserializing the `CLASS_LEVEL` data.
    -   Implement a migration path within the character loading function to convert a player's old integer-based class levels into the new `LLIST *classes` format.
    -   Port the `do_setclass` and `do_classes` commands from `src_20_dev` to `src/skills.c` and `src/act_info.c`, respectively.

4.  **OLC Editor (`clsedit`):**
    -   Create `src/editors/classes/clsedit.c`.
    -   Port the `do_clsedit` command and its associated functions and command table from `src_20_dev`, allowing in-game management of class templates.
    -   Add the new editor file to `Makefile` and `CMakeLists.txt`.

5.  **Logic Refactoring:**
    -   Perform a codebase-wide search for all uses of `sub_class_table`, `ch->pcdata->class_current`, and `ch->pcdata->sub_class_current`.
    -   Replace this logic with calls to new helper functions like `get_current_class(ch)` and `get_class_level(ch, class_data)`.
    -   Update `gain_exp()` to apply experience to `ch->pcdata->current_class`.
    -   Refactor all skill-granting logic (e.g., in `group_add`) to use the new `CLASS_DATA` skill lists instead of the old `group_table`.

This backport will enable a far more dynamic and expandable class system, laying the groundwork for a true "job" system and deeper integration with other game mechanics like reputation.
