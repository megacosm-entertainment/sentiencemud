# Backport Analysis: Reputation and Object Multityping

This document provides an analysis of two major systems from the `src_20_dev` branch—Reputation and Object Multityping—and outlines a plan for backporting them into the main `src` codebase.

---

## 1. Reputation System

### 1.1. System Overview (`src_20_dev`)

The reputation system in `src_20_dev` is a complete faction system allowing players to gain and lose standing with various groups in the world.

- **Core Components:**
    -   **`REPUTATION_INDEX_DATA`**: The template for a faction, created by builders in OLC (`repedit`). It defines the faction's name, flags, and a series of ranks.
    -   **`REPUTATION_INDEX_RANK_DATA`**: Defines a specific rank within a faction (e.g., "Friendly", "Hated"), including its point capacity and behavioral flags (`REPUTATION_RANK_HOSTILE`).
    -   **`REPUTATION_DATA`**: A player-specific data structure that tracks their current points and rank within a faction. This is stored on the `CHAR_DATA` struct.
    -   **`MOB_REPUTATION_DATA`**: Attached to mob prototypes to define reputation point gains/losses upon killing them.

- **Functionality:**
    -   Players gain or lose reputation through mob kills, quest rewards, and script actions.
    -   As reputation points cross rank capacity thresholds, the player's rank changes.
    -   Reputation rank is used as a gate for various actions, including purchasing items from shops, learning skills, and accepting quests.
    -   It is deeply integrated with the scripting engine, allowing for dynamic NPC behavior based on player reputation.

### 1.2. Current State (`src`)

The `src` codebase **lacks this entire system**. The concept of reputation is limited to a few hardcoded legacy values for continent-based factions, with no underlying framework for expansion, player tracking, or OLC management.

### 1.3. Backporting Plan: Reputation

1.  **Data Structures:**
    -   Copy the `REPUTATION_DATA`, `REPUTATION_INDEX_DATA`, `REPUTATION_INDEX_RANK_DATA`, and `MOB_REPUTATION_DATA` struct definitions from `src_20_dev/merc.h` to `src/merc.h`.
    -   Add the `LLIST *reputations;` and `MOB_REPUTATION_DATA *mob_reputations;` fields to the `CHAR_DATA` and `MOB_INDEX_DATA` structs, respectively.

2.  **Core Logic:**
    -   Create a new `src/reputation.c` and port the core functions from `src_20_dev/reputation.c`, including `gain_reputation`, `set_reputation_rank`, `find_reputation_char`, and the `do_reputations` command.

3.  **Persistence:**
    -   Integrate `load_reputation_index` and `save_reputation_indexes` into the area loading/saving process in `src/io/json/json_area.c`. This will require creating JSON counterparts for the old file format.
    -   Port `fread_reputation` and `fwrite_reputations_char` logic from `src_20_dev/save.c` to the player save/load functions in `src/io/json/json_player.c`.

4.  **OLC Integration:**
    -   Create a new `src/editors/reputation/repedit.c` and port the `repedit` OLC functions.
    -   Port the `medit_addreputation` and `medit_delreputation` commands to `src/editors/mobiles/medit.c`.
    -   Add the new `repedit` command to the `olc_cmds` table.

5.  **Build System:**
    -   Add the new `reputation.c` and `editors/reputation/repedit.c` files to `Makefile` and `CMakeLists.txt`.

---

## 2. Object Multityping

### 2.1. System Overview (`src_20_dev`)

The object multityping system is a major architectural refactor that moves object-specific data out of the overloaded `value[5]` integer array and into dedicated, type-specific structures.

- **Core Concept:**
    -   The generic `long value[MAX_OBJVALUES]` array in `OBJ_INDEX_DATA` and `OBJ_DATA` is deprecated.
    -   Instead, these structs contain a series of pointers to new, specific data structs (e.g., `ARMOR_DATA *_armor;`, `WEAPON_DATA *_weapon;`, `CONTAINER_DATA *_container;`).
    -   This makes the code more readable, self-documenting, and significantly easier to extend with new object types and properties.

- **Example (`WEAPON_DATA`):**
    -   Instead of `obj->value[1] = 10;` and `obj->value[2] = 4;` (for 10d4), the code would use `obj->_weapon->dice_num = 10;` and `obj->_weapon->dice_size = 4;`.

### 2.2. Current State (`src`)

The `src` codebase still uses the legacy `long value[8]` array on both `OBJ_INDEX_DATA` and `OBJ_DATA`. All object-specific logic is implemented by accessing these integer indices, which is error-prone and difficult to maintain.

### 2.3. Backporting Plan: Object Multityping

This is a more invasive change than the reputation system and must be handled carefully.

1.  **Data Structures:**
    -   Define and copy all the type-specific object data structs (`ARMOR_DATA`, `WEAPON_DATA`, etc.) from `src_20_dev/merc.h` into `src/merc.h`.
    -   Add the corresponding `*_armor`, `*_weapon`, etc. pointers to `struct obj_index_data` and `struct obj_data`.

2.  **Memory Management:**
    -   Implement `new_*()` and `free_*()` functions for each new object data type in `src/mem.c`.
    -   Update `new_obj_index()` and `new_obj()` to allocate memory for these new structs based on the object's `item_type`.
    -   Update `free_obj_index()` and `free_obj()` to free the associated memory.

3.  **Data Migration & Logic Refactor (The Hard Part):**
    -   This is the most critical step. A comprehensive, codebase-wide search-and-replace must be performed.
    -   Every instance of `obj->value[X]` must be identified and replaced with the correct access to the new struct pointer (e.g., `obj->value[1]` for a weapon becomes `obj->_weapon->dice_num`).
    -   This requires careful analysis for each `item_type` to ensure the correct mapping.
    -   Automated scripting for this refactoring should be considered, but manual review will be essential.

4.  **OLC Refactor:**
    -   The `oedit` functions in `src/editors/objects/oedit.c` must be completely overhauled.
    -   Instead of commands like `value0`, `value1`, etc., new commands must be created to modify the fields of the specific data structs (e.g., `oedit damage <dice>`, `oedit charges <#>`, `oedit capacity <#>`, etc.).

5.  **Persistence:**
    -   The `io/json/json_objects.c` file must be updated.
    -   The `json_write_obj_values` function will be replaced. Instead of writing a simple `values` array, it will need to serialize the appropriate type-specific struct to a JSON object.
    -   The `json_read_obj_values` function will need to be updated to read this new JSON object and populate the corresponding struct.

This backport will be a significant undertaking but will result in a much cleaner, more robust, and more maintainable object system for the future.
