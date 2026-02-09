# Backport Analysis: Reputation System

This document provides an analysis of the Reputation system from the `src_20_dev` branch and outlines a plan for backporting it into the main `src` codebase.

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