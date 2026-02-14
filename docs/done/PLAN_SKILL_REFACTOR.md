# Plan: Skill System Refactor

This document outlines a phased plan to refactor the MUD's skill and spell system. The goal is to eliminate the legacy, array-based `skill_table` and its integer `sn` identifiers, replacing it with a modern, data-driven system based on hash table lookups and an in-game editor.

This plan is modeled directly on the implementation of the JSON-based race system (`json_race.c`). The in-game editor portion will be modeled on the `skedit` and `songedit` OLC from the `src_20_dev` branch.

---

## **Benefits and Drawbacks of This Refactor**

### Benefits

1.  **Maintainability & Stability:** Eliminates "magic numbers" (`sn`). Adding or removing skills will no longer risk breaking player save files or hardcoded logic that depends on a specific array index.
2.  **Flexibility & Speed of Development:** An in-game editor allows builders and designers to create, balance, and test new abilities without needing to recompile the MUD and reboot. This dramatically speeds up content creation.
3.  **Data Integrity & Version Control:** Storing each skill as a separate JSON file makes them easy to manage with version control (git). It's simple to track changes, revert a problematic edit, or have multiple people work on different skills without creating conflicts in one giant file.
4.  **Performance:** Using a hash table for lookups provides faster, more efficient access to skill data compared to the current linear scan of the `skill_table` array.
5.  **Robustness:** Prevents bugs where an invalid `sn` could access the wrong part of the `skill_table` array, leading to crashes or unpredictable game behavior.

### Drawbacks

1.  **Implementation Effort:** This is a large-scale refactoring project. It will touch many critical parts of the codebase, and the risk of introducing bugs during the transition is significant.
2.  **Initial Boot Time:** Reading and parsing hundreds of individual JSON files at server startup will be slightly slower than loading a single, pre-compiled array. This is a minor, one-time cost on boot.
3.  **Loss of Compile-Time Validation:** When linking a spell function in the editor (e.g., `skedit spellfun fireball_spell`), a typo in the function name will only be caught at runtime when the skill is used. The current system catches this at compile time. This requires careful implementation of the function lookup to handle errors gracefully.

---

## **Phase 1: Implement a Hashed Skill Backend**

This phase focuses on replacing the underlying data structure and loading mechanism without changing how the rest of the game interacts with skills.

1.  **Create `src/json_skill.c` and Header:**
    *   Create a new file, `src/json_skill.c`, to house the new skill system logic.
    *   Create a corresponding `src/json_skill.h` for declarations.
    *   This file will define a global `skill_hash_table` and a `skill_list` linked list, mirroring the race system's structure.

2.  **Implement Hash Table Utilities:**
    *   In `json_skill.c`, create a `skill_hash(const char *id)` function (FNV-1a algorithm, like the race hash).
    *   Create a `skill_hash_insert(skill_t *skill)` function to add skills to the hash table.

3.  **Implement a JSON-to-Skill Loader:**
    *   Create a `skill_load_json(const char *filename)` function. This will be responsible for opening a single skill JSON file, parsing it with `jansson`, and populating a `skill_type` struct.
    *   This function will need to be robust, handling missing fields and providing default values.

4.  **Create `data/skills/` Directory Structure:**
    *   A new directory, `data/skills/`, will be created.
    *   We will need to convert the existing skills from `const.c` into individual JSON files within this directory (e.g., `fireball.json`, `armor.json`). Each file will contain the data for one skill.

5.  **Implement `load_skills()`:**
    *   Create the main `load_skills()` function in `json_skill.c`.
    *   This function will be responsible for:
        a. Initializing the `skill_hash_table`.
        b. Opening and reading the `data/skills/` directory.
        c. Calling `skill_load_json()` for each `.json` file found.
        d. Populating the `skill_list` linked list.
        e. Iterating through the completed `skill_list` to populate the `skill_hash_table` for fast lookups.

6.  **Integrate into Server Boot:**
    *   In `db.c`, inside the `boot_db` function, comment out or remove the initialization of the old hardcoded `skill_table`.
    *   Add a call to the new `load_skills()` function.

7.  **Upgrade the Lookup Function:**
    *   Move the `skill_type_lookup` function (which I previously added to `magic.c`) to `json_skill.c`.
    *   Rename it to `skill_lookup` to standardize the naming convention.
    *   **Crucially, change its implementation** from a linear array scan to a fast lookup using the new `skill_hash_table`.

    *At the end of this phase, the server will boot up using the new JSON-based system. The rest of the game code will be unaffected, as it will still call `skill_lookup()` and get a valid pointer, unaware that the backend has been completely replaced.*

---

## **Phase 2: Create the In-Game Skill Editor (SKEDIT)**

This phase focuses on creating the tools for in-game management of skills, using `src_20_dev/olc_skill.c` as a reference.

1.  **Create `src/olc_skill.c`:**
    *   Create a new file for the OLC (Online Creation) editor.
    *   This file will contain the core logic for the `skedit` command.

2.  **Implement `do_skedit` and the Interpreter:**
    *   Add `do_skedit` to `interp.c`, allowing Immortals to start an editing session.
    *   Write the main `skedit(CHAR_DATA *ch, char *argument)` interpreter function in `olc_skill.c`. This function will be a large `switch` or `if/else if` block that handles all the sub-commands.

3.  **Implement Core Editor Commands:**
    *   **`skedit show`**: Displays the current values of all fields for the skill being edited.
    *   **`skedit create <id>`**: Creates a new, blank skill, adds it to the hash table and skill list in memory, and attaches the editor to it. The `id` will be the unique string identifier used as its filename (e.g., "new_sword_skill").
    *   **`skedit <field> <value>`**: Implement the setter for every field in the `skill_type` struct. This will be the largest part of the work. Examples:
        *   `skedit name <string>`
        *   `skedit target <target_flag>`
        *   `skedit mana <cost>`
        *   `skedit level <class> <level>`
        *   `skedit spellfun <function_name>` (This will require a lookup table to map string names to C function pointers).

4.  **Implement Saving to JSON:**
    *   Create a `save_skills_json()` function in `json_skill.c`.
    *   This function will iterate through the `skill_list` or `skill_hash_table` and write each `skill_type` struct out to its corresponding file in `data/skills/`, overwriting the old file.
    *   The `skedit save` command (and a global `asave skills` command) will call this function.

---

## **Phase 3: Deprecation and Final Cleanup**

1.  **Refactor Away from `sn`:**
    *   The ultimate goal is to remove the `int sn` parameter from all `SPELL_FUN` functions and replace it with `const skill_t *skill`.
    *   All calls to spell functions will need to be updated to pass the skill pointer instead of the now-meaningless `sn` integer.
    *   The `SKILL_ENTRY` struct on players will be changed to remove the `sn` field entirely.

2.  **Remove `skill_table`:**
    *   Once no code directly references the `skill_table` array, it can be safely removed from `const.c` and `merc.h`, completing the refactor.