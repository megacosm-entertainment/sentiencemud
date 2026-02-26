# PLAN: Class and Job System Backport (Comprehensive)

**Date:** February 9, 2026 (Updated)
**Status:** Comprehensive Draft for Review
**Source:** Merged from `src_20_dev` backport plans: `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`, `PLAN_backport_skills_classes.md`, `PLAN_SKILL_REFACTOR.md`, `TOKEN_AFFECT_COUPLING_CHANGES.md`, `TOKEN_MIGRATION_ANALYSIS.md`, and `MIGRATION_PLAN.md`, `GEMINI_CLASS_SYSTEM_ANALYSIS.md`, and `GEMINI_CLASS_PROGRESSION_ANALYSIS.md`.

---

## 1. Goals

1.  **Backport Data-Driven Class/Job System:** Migrate the flexible class ("job") system from `src_20_dev/` to `src/`, replacing the existing rigid class/subclass/remort system. This includes making classes fully data-driven (loaded from JSON files) and OLC-editable with pronoun-aware titles.
2.  **Backport Data-Driven Skill System:** Implement a JSON-based, hash table-driven skill and spell system, eliminating the `skill_table` array and introducing an in-game OLC editor (`skedit`).
3.  **Integrate Token System Enhancements:** Backport critical token system improvements from `src_20_dev`, including token-affect coupling, script commands for skill management, and event triggers. This is crucial as skills can now be granted via tokens.
4.  **Backport Data-Driven Race System:** Implement OLC-editing for race data and ensure preservation of pronoun integration, building upon the existing JSON-based race definitions.
5.  **Redefine "Remort":** Modify the concept of "remort" to be an in-play race change mechanism, gated by achievements and potentially account-level unlocking for new characters.
6.  **Add Script Commands:** Introduce new script commands for dynamic management of classes, skills, and tokens.
7.  **Update OLC Editors:** Enhance existing OLC editors (`clsedit`, `skedit`, `raceedit`, `tedit`) to support the new data-driven systems.

---

## 2. Current System (`src/`) Overview

### 2.1. Races

*   **JSON-based:** Races are defined in individual JSON files (`data/races/*.json`).
*   **Limited Customization:** Currently no in-game editor for race data.
*   **Pronoun Integration:** Uses 1.0's custom pronoun system, but race display names are not fully pronoun-agnostic.

### 2.2. Classes and Progression

This section details the architectural differences between the class and progression systems in the current codebase (`/sentience/src`) and the experimental `src_20_dev` branch.

#### 2.2.1. Current Class System (`src`)

In the original `src` directory, the class system is largely hardcoded, making it rigid and difficult to modify.

*   **Hardcoded Definitions:** Class and subclass definitions are stored in static arrays, `class_table` and `sub_class_table`, located in `src/const.c`.
*   **Limited Extensibility:** Adding new classes or changing existing ones requires modifying the source code and recompiling the server. This is a significant barrier to content development and iteration.
*   **Rigid Structure:** Player class information is stored in a series of integer fields within the `pcdata` struct, limiting a player to a small, fixed set of classes.
*   **Progression:** Characters progress through a fixed sequence: one of four base classes, followed by a subclass, then a second subclass, and finally a remort class.
*   **Titles:** Gender-based (`SEX_MALE`/`FEMALE`/`NEUTRAL`) for classes and church ranks, not pronoun-aware.
*   **Remort:** At level 120, characters can "remort," resetting to level 1 with a new remort race and selecting new remort subclasses. This is a one-time class reset event.
*   **Implementation:** Heavily hardcoded class checks (`if (ch->class == CLASS_WARRIOR)`) and `IS_REMORT(ch)` macros are prevalent. Class and subclass levels are stored in individual `int` fields in `PC_DATA`.

#### 2.2.2. New Class System (`src_20_dev`)

The `src_20_dev` directory implements a modern, data-driven approach to classes and progression.

*   **External Data File:** Class definitions are loaded at boot time from an external file: `system/classes.dat`. The `load_classes` function in `src_20_dev/skills.c` is responsible for parsing this file.
*   **New Data Structures:**
    *   `CLASS_DATA`: Defined in `src_20_dev/merc.h`, this struct holds the properties for a single class (e.g., name, stat modifiers, etc.).
        ```c
        // In src_20_dev/merc.h
        struct class_data
        {
            CLASS_DATA *next;
            bool valid;
            char *name;
            char *description;
            char *display[SEX_MAX];
            char *who[SEX_MAX];
            CLASS_DATA **gcl;
            int16_t uid;
            int16_t type;
            long flags;
            LLIST *groups;
            int16_t primary_stat;
            int16_t max_level;
            CLASS_LEAVE_FUN *leave;
            CLASS_ENTER_FUN *enter;
        };
        ```
    *   `CLASS_LEVEL`: Also in `src_20_dev/merc.h`, this struct tracks a player's level and experience within a specific class. Players have a list of these, enabling multi-classing.
*   **Migration Path:** The system is designed for a smooth transition. If `classes.dat` is not found, the `load_classes` function will bootstrap the data from the old hardcoded tables and create the new file.
*   **Extensibility:** The new system is highly extensible. New classes, including non-combat types such as `CLASS_CRAFTING` and `CLASS_GATHERING` (defined in `src_20_dev/tables.c`), can be added simply by editing the `classes.dat` file.

**Benefits of the New System**

The new data-driven system offers several key advantages:

*   **Flexibility:** Allows for easy addition, removal, and modification of classes without needing to recompile the codebase.
*   **Multi-classing:** The architecture naturally supports players having levels in multiple classes simultaneously.
*   **Maintainability:** Separating class data from the source code makes the system cleaner and easier to manage.

### 2.3. Skill System

The primary difference in skill systems is the **method of dispatch**. `src` primarily calls C functions via an integer ID, while `src_20_dev` is built to act upon token objects that carry their own scripted logic.

*   **`skill_table` Array:** All skills are defined in a massive, static `const struct skill_type skill_table[]` array in `const.c`.
*   **`gsn_` Globals:** Global integer variables (e.g., `gsn_fireball`) are used to hold the array index (`sn`) for each skill after being looked up at boot time.
*   **No In-Game Editor:** Adding or modifying skills requires changing `const.c` and recompiling the server.
*   **Brittle by Design:** The reliance on array indices (`sn`) is fragile. Changing the order of skills in the table or removing one can break player save files or hardcoded logic.
*   **Player's Skill: `SKILL_ENTRY`**
    The `src` version of `SKILL_ENTRY` identifies an ability by an integer `sn`, which is an index for the global `skill_table`.
    ```c
    typedef struct skill_entry_type {
    	struct skill_entry_type *next;
    	char source;
    	long flags;
    	bool practice;
    	bool improve;
    	bool isspell;
    	int16_t sn; // <<< Skill Number (index)
    	int16_t song;
    	TOKEN_DATA *token;
    } SKILL_ENTRY;
    ```
    In contrast, the `src_20_dev` version identifies an ability by a pointer to a `TOKEN_DATA` instance.

*   **Master Skill Definition: `SKILL_DATA` vs. `skill_type`**
    `src`'s `skill_type` is a classic MUD structure containing a direct `spell_fun` pointer. This is the function that gets called when a player uses the skill.
    ```c
    struct	skill_type
    {
        char *	name;
        int16_t	skill_level[MAX_CLASS];
        int16_t	rating[MAX_CLASS];
        SPELL_FUN *	spell_fun; // <<< Direct function pointer
        int16_t	target;
        int16_t	minimum_position;
        int16_t *	pgsn;
        int 	race;
        // ... other fields ...
    };
    ```
    In `src_20_dev`, the `skill_data` struct has function pointers too, but these are primarily for "source abilities" (hardcoded C skills), with "Token abilities" designed to call their respective triggers. This highlights `src_20_dev`'s prioritization of scripted logic over hardcoded C functions.

### 2.4. Token System

*   **Basic Functionality:** Tokens exist as temporary or permanent modifiers attached to characters, objects, or rooms. They can carry values and trigger scripts.
*   **Limited Coupling:** The current system lacks robust bidirectional coupling between tokens and affects they create. Affects do not inherently track their originating token, and tokens do not track affects they have applied.
*   **Scriptability:** Token scriptability is present but could be enhanced with more specific event triggers and skill management commands.
*   **Garbage Collection:** Basic garbage collection for tokens is present (`bool gc;` in `TOKEN_DATA`).
*   **The Token: `TOKEN_DATA`**
    Both versions have a `struct token_data` that is remarkably similar. This suggests the concept of a "token" as a scriptable entity exists in both, but its integration into the core progression system is the key differentiator. The difference is not in the token itself, but in how deeply it's integrated as the primary driver for skills and class abilities.
    ```c
    struct token_data
    {
    	char __type;
        bool    gc;
    	TOKEN_INDEX_DATA *pIndexData;
        // ... pointers ...
    	PROG_DATA *progs;
    	EVENT_DATA *events;
        // ... other fields ...
    	unsigned long id[2];
    	long value[MAX_TOKEN_VALUES];
    	SKILL_ENTRY *skill;
    	LLIST *affects;
        int tempstore[MAX_TEMPSTORE];
    };
    ```

### 2.5. JSON Persistence Patterns

*   **JSON Library:** Utilizes `jansson.h` for all JSON parsing and generation.
*   **Structure:** Data is organized into top-level objects with sections like "metadata", "character", "inventory", "equipment", "skills", etc.
*   **Identifiers:** Human-readable names (e.g., skill names, flag names, race IDs) are often used as keys, alongside numeric UIDs where necessary.
*   **Flags/Bitvectors:** Stored as JSON arrays of human-readable flag names (e.g., "affected_by_flags": ["invis", "fly"]) and often duplicated as numeric values for backward compatibility during transitions. Utility functions like `flags_to_json_array` and `flags_from_json_array` are used.
*   **Linked Lists (`LLIST`):** Serialized as JSON arrays, with each element becoming a JSON object. This is seen for inventory, equipment, affects, and racial skills.
*   **Widevnums (`WNUM`):** Used for unique identification of game entities (objects, rooms) across different areas, stored as strings ("area_id:vnum").
*   **Migration:** Includes logic for `backup_old_pfile` if data is not in JSON format, demonstrating a pattern for handling format transitions.

### 2.6. Key Files

`merc.h`, `skills.c`, `save.c`, `act_info.c`, `interp.c`, `const.c`, `handler.c`, `io/json/json_char.c`, `io/json/json_race.c`, `io/json/json_persist.h`, `db.c`, `update.c`, `nanny.c`, `magic_astral.c`, `church.c`.

---

## 3. New System (`src_20_dev/`) Overview and Backport Strategy

The backport aims to bring in several key advancements from `src_20_dev/`, replacing rigid, hardcoded systems with flexible, data-driven alternatives.

**Conclusion for Backporting (`src_20_dev` progression system analysis)**

A potential backport of the `src_20_dev` progression system would be a significant architectural refactor. It would require:

1.  Changing the `skill_table` from an array of `skill_type` to a list of `skill_data`.
2.  Modifying the `SKILL_ENTRY` on the player to prioritize the `token` pointer.
3.  Rewriting the skill dispatch logic (`do_cast`, etc.) to check for and execute token scripts first, before falling back to C function pointers.
4.  Creating a migration path for existing player skills from the `sn` integer system to the new token-based system.
5.  Integrating the `CLASS_DATA` struct and its associated functions for granting token-based abilities upon level-up, replacing or augmenting the current JSON-based loading.

The effort would be substantial but would result in a far more flexible and dynamic in-game system for creating and modifying abilities without recompiling.

### 3.1. New Class/Job System

*   **"Job" System:** Flexible, data-driven system where "jobs" are referred to as "classes." Players can freely switch between active classes.
*   **Independent Leveling:** Jobs (classes) can have varying maximum levels, and players level them independently.
*   **Abilities:** Players gain abilities from their current active class and potentially "cross-class" skills/traits.
*   **Data-Driven:** Classes are defined in external JSON files and are manageable via an Online Creation (OLC) editor (`clsedit`).
*   **Implementation:** Replaces hardcoded class checks with flexible lookups and flag-based checks on `CLASS_DATA` objects.
*   **Data Structures:** `CLASS_DATA` (new, defines a class), `CLASS_LEVEL` (links character to a class and their level/XP), `PC_DATA` (contains `LLIST *classes;` and `CLASS_LEVEL *current_class;`), `ACCOUNT_DATA` (new field for unlocked remort races).

### 3.2. New Skill System

*   **JSON-based Skill Definitions:** Each skill/spell is defined in its own JSON file.
*   **Hash Table Lookups:** Skills are loaded into a hash table for efficient lookup by name, eliminating `sn` reliance.
*   **In-Game Editor (`skedit`):** Full OLC functionality for creating, modifying, and saving skills.
*   **Decoupled `sn`:** The integer `sn` is deprecated, with functions instead receiving `const skill_t *skill` pointers.

### 3.3. Token System Enhancements

*   **Core Token Structures:** `TOKEN_INDEX_DATA` (template) and `TOKEN_DATA` (instanced) are present.
*   **SKILL_ENTRY Hybrid System:** `SKILL_ENTRY` modified to support token-based skills.
*   **Token-Affect Coupling:** Bidirectional linkage between `TOKEN_DATA` and `AFFECT_DATA`. Affects track their source token, and tokens track affects they've created. This enables proper cleanup of affects when a token is extracted.
*   **Token Integration with Magic System:** Spell casting checks for token-based spells, token value lookup for costs, and token script triggers.
*   **Script Commands:** New script commands (`GRANTSKILL`, `REVOKESKILL`, `AWARD`, `DEDUCT`) to manage token-granted skills and other token aspects.
*   **Event Triggers:** New triggers (`TRIG_TOKEN_GIVEN`, `TRIG_TOKEN_REMOVED`, `TRIG_PRACTICETOKEN`, `TRIG_PREPRACTICETOKEN`, `TRIG_PRETRAINTOKEN`) to enhance token scriptability.
*   **Token Editor (`tedit`):** OLC for creating, showing, and editing tokens.

### 3.4. New Data-Driven Race System

*   **JSON-based Race Definitions:** Races are defined in individual JSON files (`data/races/*.json`), OLC-editable.
*   **Dynamic Arrays:** `race_table[]` and `pc_race_table[]` converted to dynamic arrays/hash tables.
*   **Pronoun Integration:** Race display names are pronoun-agnostic.

### 3.5. Flexible "Remort" Race Change System

*   **Account-Level Unlocks:** Remort races are unlocked at the account level, allowing new characters on that account to select them.
*   **In-Play Race Change:** "Remort" becomes an in-play race change mechanism for existing characters, triggered by specific achievements or conditions.

### 3.6. Pronoun-Aware Church/Org Titles

*   **Modernized Titles:** Church rank titles are converted to a single `title` field, supporting custom pronouns rather than rigid male/female/neutral arrays.

---

## 4. Data Structure Changes (Add to `merc.h`)

### 4.1. New Class System Structures

```c
/* CLASS_DATA: Defines a class template (e.g., Warrior, Mage) */
typedef struct class_data CLASS_DATA;
struct class_data {
    CLASS_DATA *next;
    bool valid;
    long uid;                 /* Unique ID */
    char *name;               /* Class name (e.g., "Warrior") */
    char *description;        /* Detailed description */
    long flags;               /* CLASS_F_ flags */
    LLIST *skills;            /* SKILL_DATA * - skills granted by this class */
    // ... other class properties like max_level, prim_stat, etc.
};

/* CLASS_LEVEL: Represents a character's progress in a specific class */
typedef struct class_level CLASS_LEVEL;
struct class_level {
    CLASS_LEVEL *next;
    bool valid;
    CLASS_DATA *clazz;        /* Pointer to the class definition */
    long level;               /* Character's level in this class */
    long experience;          /* Experience points accumulated in this class */
    LLIST *custom_data;       /* pVARIABLE custom_data - class-specific variables */
};
```

### 4.2. `PC_DATA` Modifications

```c
struct pc_data {
    // ... existing fields ...
    LLIST *classes;           /* List of CLASS_LEVEL objects character has access to */
    CLASS_LEVEL *current_class; /* Pointer to the currently active class level */
    // Deprecate old fields:
    // int class_mage;
    // int sub_class_thief;
    // ... etc.
    // Ensure new account fields for unlocked races are added:
    LLIST *unlocked_remort_races; /* List of RACE_DATA * or race IDs unlocked on this account */
    // ... existing fields ...
};
```

### 4.3. Token-Affect Coupling Structures

```c
// In AFFECT_DATA (merc.h):
struct affect_data {
    // ... existing fields ...
    TOKEN_DATA *token;    /* Source token for this affect (if from TOKEN_AFFECT) */
};

// In TOKEN_DATA (merc.h):
struct token_data {
    // ... existing fields ...
    LLIST *affects;       /* List of affects created by this token */
    bool gc;              /* Boolean field for garbage collection (already present in 1.0) */
};
```

### 4.4. Skill System Structures (Update `skill_type`)

The `skill_type` struct will be updated to remove `sn` and incorporate `uid`, flags, and pointers for `spell_fun` lookup.

```c
// In skill_type (merc.h):
struct skill_type
{
    char * name;                /* Skill name                 */
    long uid;                   /* Unique ID (replaces sn)    */
    // ... other fields ...
    SPELL_FUN *spell_fun;       /* Spell function pointer     */
    long flags;                 /* SKILL_F_ flags             */
    bool isspell;               /* True if skill is a spell   */
    bool scripted;              /* True if skill is script-granted */
    // ... other fields ...
};

// In SKILL_ENTRY (merc.h):
typedef struct skill_entry_type {
    struct skill_entry_type *next;
    char source;                // Source of the skill
    long flags;                 // SKILL_F_ flags (e.g., SKILL_F_PRACTICE, SKILL_F_IMPROVE)
    int16_t rating;             // Rating for practice/train
    skill_t *skill;            // Pointer to the skill_type struct
    TOKEN_DATA *token;          // If this is a token skill, pointer to the token
} SKILL_ENTRY;
```

### 4.5. New Dungeon Flags (Add to `merc.h` and `tables.c`)

```c
// In merc.h (DUNGEON_* flag block):
#define DUNGEON_FAILED              (F)     /* Dungeon has failed */
#define DUNGEON_COMMENCED           (G)     /* Dungeon has officially started */
#define DUNGEON_GROUP_COMMENCE      (H)     /* Auto-commence when min_group is satisfied */
#define DUNGEON_FAILURE_ON_WIPE     (I)     /* Failure when all players die in boss encounter */
#define DUNGEON_FAILURE_ON_EMPTY    (J)     /* Failure if commenced and everyone leaves */
#define DUNGEON_SHARED              (Y)     /* One instance for all players */
#define DUNGEON_SOLO_INSTANCE       (X)     /* On-demand, player-owned instance with idle timeout */

// In tables.c (dungeon_flags table):
{ "shared",              DUNGEON_SHARED,            true    },
{ "solo_instance",       DUNGEON_SOLO_INSTANCE,     true    },
// ... other flags
```

### 4.6. Death Release Constants (Add to `merc.h` and `tables.c`)

```c
#define DEATH_RELEASE_NORMAL        0   /* Normal: go to death plane */
#define DEATH_RELEASE_TO_START      1   /* Stay with corpse, release → dungeon start */
#define DEATH_RELEASE_TO_FLOOR      2   /* Stay with corpse, release → current floor start */
#define DEATH_RELEASE_TO_CHECKPOINT 3   /* Stay with corpse, release → last checkpoint */
#define DEATH_RELEASE_FAILURE       4   /* Respawn at death location, dungeon fails */
```

### 4.7. New Trigger Types (Add to `scripts.h` and `tables.c`)

```c
// In scripts.h (trigger_type enum):
TRIG_DUNGEON_COMMENCED,
TRIG_TOKEN_GIVEN,
TRIG_TOKEN_REMOVED,
TRIG_PRACTICE, // For general skill practice
TRIG_PRACTICETOKEN, // For practicing token-granted skills
TRIG_PREPRACTICETOKEN, // Pre-practice validation for token skills
TRIG_PRETRAINTOKEN, // Pre-train validation for token skills

// In tables.c (trigger_table):
{ "dungeon_commenced", TRIG_DUNGEON_COMMENCED,     PRG_DPROG,  "DungeonCommenced", 0 },
{ "token_given",      TRIG_TOKEN_GIVEN,         PRG_TPROG,  "TokenGiven",       0 },
{ "token_removed",    TRIG_TOKEN_REMOVED,       PRG_TPROG,  "TokenRemoved",     0 },
{ "practice",         TRIG_PRACTICE,            PRG_MPROG,  "Practice",         0 },
{ "practice_token",   TRIG_PRACTICETOKEN,       PRG_TPROG,  "PracticeToken",    0 },
{ "pre_practice_token", TRIG_PREPRACTICETOKEN,  PRG_TPROG,  "PrePracticeToken", 0 },
{ "pre_train_token",    TRIG_PRETRAINTOKEN,     PRG_TPROG,  "PreTrainToken",    0 },
```

---

## 5. Backporting Plan - Phased Implementation

This plan integrates the backport of the Class/Job system, Skill System, Token System enhancements, and Data-Driven Races/Titles.

### Phase 1: Foundational Data Structures & Global Lookups

**Objective:** Introduce all new structs and global lookup mechanisms without altering existing game logic.
**Dependencies:** None.
**Risk:** LOW — additive changes, minimal behavioral impact if done carefully.

*   **1.1. `merc.h` Updates:**
    *   Add `RACE_DATA` struct (with fields for `race_id`, `name`, `display_names`, `playable`, `stats`, `max_stats`, `max_vitals`, `size`, `alignment_bias`, `bonus_skills`, `starting_eq`, `act_flags`, `affect_flags`, `immunity_flags`, `resistance_flags`, `vulnerability_flags`, `form_flags`, `body_parts`, `remort`, `remort_race`).
    *   Add `CLASS_DATA`, `CLASS_LEVEL` structs.
    *   Modify `PC_DATA` to add `LLIST *classes;`, `CLASS_LEVEL *current_class;`, and `LLIST *unlocked_remort_races;`.
    *   Modify `AFFECT_DATA` (`TOKEN_DATA *token;`).
    *   Modify `TOKEN_DATA` (`LLIST *affects;`).
    *   Update `skill_type` to include `uid`, `flags`, `isspell`, `scripted`, and `spell_fun *`. Remove `sn`.
    *   Update `SKILL_ENTRY` to use `skill_t *skill` instead of `sn`.
    *   Add `TOKEN_INDEX_DATA` (template for tokens) and `TOKEN_DATA` (instanced tokens).
    *   Update `church_rank_data` to remove gender-specific titles and add a single `char *title;` field.
*   **1.2. `mem.c` Updates:**
    *   Implement `new_race_data()`, `free_race_data()`, `new_class_data()`, `free_class_data()`, `new_class_level()`, `free_class_level()`.
    *   Implement `new_token_index_data()` and `free_token_index_data()`.
    *   Implement `new_token_data()` and `free_token_data()`.
    *   Initialize new fields in `new_affect()` (`af->token = NULL;`).
    *   Initialize new fields in `new_token_data()` (`token->affects = NULL;`, `token->gc = false;`).
    *   Add cleanup in `free_token_data()` (`list_destroy(token->affects);`).
*   **1.3. `tables.c` & `scripts.h` Updates:**
    *   Add `BSTYPE_MAZE` and "maze" to `blueprint_section_types`. (This is now out of scope for this document, but retained for historical context in the combined doc.)
    *   Add `DUNGEON_SHARED`, `DUNGEON_SOLO_INSTANCE`, etc. to `dungeon_flags`. (This is now out of scope for this document, but retained for historical context in the combined doc.)
    *   Add `DEATH_RELEASE_*` constants and their respective table entry. (This is now out of scope for this document, but retained for historical context in the combined doc.)
    *   Add new trigger types (`TRIG_DUNGEON_COMMENCED`, `TRIG_TOKEN_GIVEN`, `TRIG_TOKEN_REMOVED`, `TRIG_PRACTICE`, `TRIG_PRACTICETOKEN`, `TRIG_PREPRACTICETOKEN`, `TRIG_PRETRAINTOKEN`) and their entries in `trigger_table`.
*   **1.4. Global Lookup Systems (`race_data.c`, `classes.c`, `json_skill.c`, `tedit.c`):**
    *   **Races:** Create `src/race_data.c` and `src/race_data.h`. Implement hash table and dynamic arrays for race lookups (`race_lookup()`). Implement `load_races()` to load race definitions from `data/races/<race_id>.json`.
    *   **Classes:** Create `src/classes.c` and `src/classes.h`. Implement hash table (`class_hash_table`) and UID index (`class_uid_index`) for `CLASS_DATA` lookups. Implement `load_classes()` to load class definitions from `data/classes/<class_id>.json`.
    *   **Skills:** Create `src/json_skill.c` and `src/json_skill.h`. Implement hash table (`skill_hash_table`) for `skill_type` lookups. Implement `load_skills()` to load skill definitions from `data/skills/<skill_id>.json`.
    *   **Tokens:** Implement `load_tokens()` to load token definitions from area files (`db.c`).
    *   **Boot Process:** In `db.c` `boot_db()`, call `load_races()`, `load_classes()`, `load_skills()`, and `load_tokens()`. Remove initialization of old `race_table` and `skill_table`.
*   **1.5. Build System:** Update `Makefile` and `CMakeLists.txt` for `race_data.c`, `classes.c`, `json_skill.c`.

### Phase 2: JSON Serialization, Deserialization & Data Migration

**Objective:** Enable persistence for new data structures and migrate old data formats.
**Dependencies:** Phase 1.
**Risk:** HIGH — involves character pfile migration, must be robust.

*   **2.1. `src/io/json/json_char.c` & `src/io/json/json_persist.h`:**
    *   **Character JSON:** Modify character serialization/deserialization to handle `LLIST *classes` and `CLASS_LEVEL *current_class`.
    *   **Class_Level Data:** Implement `json_serialize_class_level()` and `json_deserialize_class_level()`, including custom variable data.
    *   **Migration (PC_DATA):** Implement logic in `json_char_deserialize()` to convert old `PC_DATA` integer class fields into the new `LLIST *classes` format. This includes `__add_class_level` and `fix_broken_classes` logic.
    *   **Migration (Race):** Update race deserialization to use `race_lookup()` for ID-based lookup and remove hardcoded arrays.
*   **2.2. `src/io/json/json_account.c`:**
    *   Implement serialization/deserialization for `LLIST *unlocked_remort_races` in `ACCOUNT_DATA`.
*   **2.3. `src/io/json/json_race.c`:**
    *   Implement `json_serialize_race()` and `json_deserialize_race()` for individual race definitions.
*   **2.4. `src/io/json/json_skill.c`:**
    *   Implement `json_serialize_skill()` and `json_deserialize_skill()` for individual skill definitions.
*   **2.5. `src/io/json/json_token.c`:**
    *   Implement `json_serialize_token_index_data()` and `json_deserialize_token_index_data()`.
*   **2.6. `src/io/json/json_area.c`:**
    *   Implement `json_area_serialize_token_index_data()` and `json_area_deserialize_token_index_data()` for tokens embedded in area JSON.

### Phase 3: Core Game Logic Refactoring - Classes, Skills, Races & Tokens

**Objective:** Replace hardcoded class/skill/race/token logic with data-driven lookups and new behaviors.
**Dependencies:** Phase 1, 2.
**Risk:** VERY HIGH — touches almost every part of the game.

*   **3.1. Class System (`skills.c`, `handler.c`, `act_info.c`, `nanny.c`):**
    *   Replace `sub_class_table` with `CLASS_DATA` lookups.
    *   Update `get_profession()` to use new class system.
    *   Refactor all skill-granting logic to use `CLASS_DATA` skill lists.
    *   Update `do_who()` and character creation (`nanny.c`) for pronoun-aware class titles (use single title).
*   **3.2. Race System (`handler.c`, `act_info.c`, `nanny.c`):**
    *   Replace hardcoded race logic with `RACE_DATA` lookups.
    *   Update `do_who()` and character creation (`nanny.c`) for pronoun-agnostic race names.
    *   Remove hardcoded male/female race variants.
*   **3.3. Token System (`handler.c`, `magic.c`, `act_wiz.c`, `skills.c`):**
    *   Modify `affect_to_char()` and `affect_to_obj()` to link new `AFFECT_DATA` to `TOKEN_DATA`.
    *   Modify `affect_remove()` to unlink affects from `TOKEN_DATA`'s `affects` list.
    *   Modify `extract_token()` to break all `paf->token` links.
    *   Update spell casting logic (`magic.c`) to check for token-based spells and use token value lookup for costs.
    *   Update `skill_entry_rating()` to check token vs hardcoded.
    *   Ensure `PREPRACTICETOKEN`/`PRETRAINTOKEN` triggers exist.
*   **3.4. Codebase-Wide Search & Replace:**
    *   Replace direct accesses to old `PC_DATA` class/race/skill integer fields (e.g., `ch->pcdata->class_mage`, `ch->race`) with `get_class_level(ch, CLASS_DATA *)` and `ch->race_data` (new `RACE_DATA *` field in `CHAR_DATA`).
    *   Replace all `gsn_` global uses with `skill_lookup("skill_name")` or `get_skill_data(skill_id)` to get `skill_type *`.
    *   Refactor `SPELL_FUN` parameters from `int sn` to `const skill_t *skill`. Update all calls to spell functions.
    *   Update `do_cast()` to handle token spells.
*   **3.5. `update.c`:**
    *   Modify `gain_exp()` to operate on `CLASS_LEVEL *current_class`.
    *   Port 2.0 logic for `TOKEN_DATA` cleanup when `token->affects` becomes empty.

### Phase 4: Token Event Triggers & Script Commands

**Objective:** Enhance token scriptability and enable script-driven skill management.
**Dependencies:** Phase 1 (structs), Phase 3 (new skill system for GRANTSKILL).
**Risk:** MEDIUM.

*   **4.1. `script_commands.c`:**
    *   Implement `GRANTSKILL` and `REVOKESKILL` commands, using the new skill lookup system.
    *   Implement `AWARD` and `DEDUCT` commands (consolidated token management).
*   **4.2. `handler.c` & `token_*.c`:**
    *   Call `TRIG_TOKEN_GIVEN` when a token is granted to an entity (`token_to_char`).
    *   Call `TRIG_TOKEN_REMOVED` when a token is removed from an entity (`token_from_char`/`extract_token`).
*   **4.3. `skills.c`:**
    *   Implement `TRIG_PRACTICE`, `TRIG_PRACTICETOKEN`, `TRIG_PREPRACTICETOKEN`, `TRIG_PRETRAINTOKEN` in skill practice/training logic.

### Phase 5: OLC Editor Updates

**Objective:** Provide in-game tools for managing the new data-driven systems.
**Dependencies:** Phase 1, 2, 3.
**Risk:** LOW — editor-only changes.

*   **5.1. `tedit` (Token Editor):** Create `src/editors/tokens/tedit.c` and port 2.0 functionality.
*   **5.2. `raceedit` (Race Editor):** Create `src/editors/races/raceedit.c` for OLC of `RACE_DATA`.
*   **5.3. `clsedit` (Class Editor):**
    *   Create `src/editors/classes/clsedit.c`.
    *   Implement `do_clsedit` command and interpreter for `CLASS_DATA` management (name, flags, skill lists).
*   **5.4. `skedit` (Skill Editor):**
    *   Create `src/editors/skills/skedit.c`.
    *   Implement `do_skedit` command and interpreter for `skill_type` management (name, mana, target, `spell_fun` string lookup).
*   **5.5. Show Display Updates:** Editors should use `widevnum_string_*()` for all room/vnum references.

### Phase 6: New "Remort" Race Change System

**Objective:** Implement account-level race unlocks and in-play race changes.
**Dependencies:** Phase 1, 2.
**Risk:** MEDIUM.

*   **6.1. `ACCOUNT_DATA` Modification:**
    *   Add `LLIST *unlocked_remort_races;` to `ACCOUNT_DATA` in `merc.h`.
    *   Update `json_account.c` to serialize/deserialize this list.
*   **6.2. Administration Tool:** Implement OLC or admin command (`acctedit races unlock`) to manage unlocked remort races.
*   **6.3. Character Creation Check (`nanny.c`):** Modify character creation to validate remort race selection against `ACCOUNT_DATA`.
*   **6.4. In-Play Race Change:**
    *   Add `bool can_remort_race_change;`, `RACE_DATA *pending_remort_race;` to `PC_DATA`.
    *   Implement `do_changerace` command: changes `ch->race`, updates stats/skills, clears flag.
*   **6.5. Refactor `IS_REMORT(ch)`:**
    *   Replace uses related to *remort classes* with checks against `CLASS_DATA` flags.
    *   Re-evaluate uses related to *remort races* for consistency with the new model.

### Phase 7: Pronoun-Aware Church/Org Titles

**Objective:** Modernize church rank titles to support custom pronouns.
**Dependencies:** Phase 1.
**Risk:** LOW.

*   **7.1. `church_rank_data` Modification (`merc.h`):**
    *   Remove gender-specific titles (`title_male`, `title_female`, `title_neutral`).
    *   Add single `char *title;` field.
*   **7.2. Church Title Functions (`church.c`):**
    *   Update `get_member_title()` to return single title, potentially using pronoun templates for parsing.
*   **7.3. Church Rank Loading/Saving:** Update `json_church.c` (or equivalent) to handle new title structure.

### Phase 8: Migration Tools

**Objective:** Preserve world data during 1.0→1.5 transition.
**Dependencies:** Phases 2, 3, 4, 6, 7.
**Risk:** LOW.

*   **8.1. Race/Class Export Script:**
    *   Script to read `const.c` and generate JSON files for races and classes.
    *   Preserve all existing race/class data, ensure JSON output validates.
*   **8.2. Save File Migration (Optional):**
    *   If save format changes, create migration tool.
    *   Convert old sex-based class references to new system.
    *   Preserve character data.
*   **8.3. Old pfile Migration:** Update character loading in `save.c` to handle old pfile formats and migrate them to the new JSON-based `LLIST *classes` structure.

### Phase 9: Cleanup

**Objective:** Remove deprecated code and data.
**Dependencies:** All previous phases.
**Risk:** MEDIUM.

*   **9.1. Hardcoded Lookups:** Remove hardcoded class/race/skill tables from `const.c`.
*   **9.2. `gsn_` Globals:** Remove all `gsn_` global integer variables.
*   **9.3. Old Class Fields:** Remove old integer class fields from `PC_DATA` in `merc.h`.

---

## 10. Files Affected

### New or Modified Source Files

| File | Changes |
|------|---------|
| `merc.h` | New structs (`RACE_DATA`, `CLASS_DATA`, `CLASS_LEVEL`, `TOKEN_INDEX_DATA`, `TOKEN_DATA`), new fields on existing structs (`PC_DATA`, `AFFECT_DATA`, `skill_type`, `SKILL_ENTRY`, `church_rank_data`), new constants (new trigger types, token types/values) |
| `mem.c` | Allocation/deallocation for new structs, initialization for `AFFECT_DATA` and `TOKEN_DATA` new fields |
| `db.c` | Call `load_races()`, `load_classes()`, `load_skills()`, token loading. Remove old `race_table` and `skill_table` init. |
| `tables.c` | New trigger type entries, new token value tables, death release table. |
| `scripts.h` | New trigger types declarations |
| `skills.c` | New class helper functions, refactor skill granting, `get_profession` replacement, token practice/train triggers. |
| `race_data.c` | **NEW:** Hashed race backend, `load_races()`, `race_lookup()`, `json_serialize_race()`, `json_deserialize_race()`. |
| `classes.c` | **NEW:** Global class lookup system, `load_classes()`, class management helpers. |
| `json_skill.c` | **NEW:** Hashed skill backend, `load_skills()`, `skill_lookup()`, `json_serialize_skill()`, `json_deserialize_skill()`. |
| `io/json/json_token.c` | **NEW:** JSON serialization/deserialization for token index data. |
| `handler.c` | Affect/token coupling logic (`affect_to_char`, `affect_remove`, `extract_token`), token event triggers (`token_to_char`, `token_from_char`), `return_from_maze`. |
| `script_commands.c` | New script commands (`GRANTSKILL`, `REVOKESKILL`, `AWARD`, `DEDUCT`), enhanced `spawndungeon`, layout/links/levels/specialexits/specialrooms commands. |
| `script_ifc.c` | New IFCHECKs (`dungeonflag`, `instanceflag`, `indungeon`, `ininstance`). |
| `io/json/json_char.c` | Character JSON serialization/deserialization for new `PC_DATA` fields (`classes`, `current_class`), migration logic for old class fields, race loading/saving. |
| `io/json/json_account.c` | Account JSON serialization/deserialization for `unlocked_remort_races`. |
| `src/io/json/json_area.c` | **Crucially update for embedded JSON serialization/deserialization of Blueprint Sections, Blueprints, and Dungeons.** |
| `editors/tokens/tedit.c` | **NEW:** OLC editor for `TOKEN_INDEX_DATA`. |
| `editors/races/raceedit.c` | **NEW:** OLC editor for `RACE_DATA`. |
| `editors/classes/clsedit.c` | **NEW:** OLC editor for `CLASS_DATA`. |
| `editors/skills/skedit.c` | **NEW:** OLC editor for `skill_type`. |
| `magic.c` | Token integration for spell system (`do_cast`). |
| `magic_astral.c` | Updated `spell_maze` to use dungeon system. |
| `update.c` | `gain_exp()` for new class system, token auto-extraction, maze timer integration. |
| `nanny.c` | Character creation for new class/race selection and remort race selection. |
| `act_info.c` | Pronoun-neutral `do_who()`. |
| `act_wiz.c` | Skill/spell granting commands. |
| `church.c` | Single-title rank system. |
| `const.c` | Remove hardcoded tables (races, classes, skills). |
| `Makefile` | Update for new `.c` files. |
| `CMakeLists.txt` | Update for new `.c` files. |

### Data Files

| File | Changes |
|------|---------|
| `data/races/<race_id>.json` | **NEW:** Individual JSON files for race definitions. |
| `data/classes/<class_id>.json` | **NEW:** Individual JSON files for class definitions. |
| `data/skills/<skill_id>.json` | **NEW:** Individual JSON files for skill definitions. |

---

## 11. Testing Strategy

### Unit Tests
- `RACE_DATA`, `CLASS_DATA`, `CLASS_LEVEL` creation, manipulation, lookup.
- `TOKEN_INDEX_DATA`, `TOKEN_DATA` creation, manipulation, lookup.
- Maze generation with various grid sizes, fixed rooms, and templates.
- Shared/solo dungeon creation, ownership, idle timeout, group size validation.
- JSON serialization/deserialization for all new/modified data structures.

### Integration Tests
- Full class system lifecycle: character creation, class leveling, class switching, skill acquisition.
- Race change mechanism: unlocking races via account, in-play eligibility, execution, effects.
- Token-affect coupling: affect application, removal, token extraction, persistency across reboots.
- Script commands (`GRANTSKILL`, `REVOKESKILL`, `AWARD`, `DEDUCT`, `dungeoncommence`, `spawndungeon` etc.) functionality.
- Full instance creation with `BSTYPE_MAZE` section.
- Multi-floor dungeon with maze floors and `PREVFLOOR`/`NEXTFLOOR` connections.
- Shared dungeon (`DUNGEON_SHARED`): multiple players entering same dungeon.
- Solo dungeon (`DUNGEON_SOLO_INSTANCE`): separate instances per player/group, verifies idle timeout and destruction.
- JSON round-trip: serialize area with embedded blueprints/dungeons/classes/skills → deserialize → verify identical structure and data.

### Gameplay Tests (Manual, on dev server)
- Character creation: verify new class/race selection, pronoun setup.
- Leveling: verify experience gain for active class.
- Class switching: verify active class changes, skills update.
- Skill acquisition: verify skills granted by class or tokens.
- `skedit`, `clsedit`, `raceedit`, `tedit` OLC functionality.
- Walk through generated mazes — verify connectivity, no dead unreachable areas.
- Cast maze spell — verify player lands in instance, timeout ejects correctly.
- Multiple players in shared PoA dungeon — verify same instance.
- Group enters Geldoff's Maze — verify solo instance and proper cleanup.
- Death in dungeon — verify `death_release` behavior.
- Dungeon idle timeout — verify cleanup after all players leave a solo instance.
- Church ranks: verify pronoun-aware titles display correctly.

---

## 12. Open Questions

1.  **Church Title Approach**: `MIGRATION_PLAN.md` asks about "Simple single title (Option A)" or "Pronoun template system (Option B)" for church titles. This needs a decision on the level of dynamic pronoun handling desired for non-player titles.
2.  **Migration Scope**: Do we want to migrate world area files now, or just prepare the system?
3.  **Token Priority**: Should we port ALL 2.0 token features, or just core skill/spell tokens? `MIGRATION_PLAN.md` had a "Priority Migration List" that should be followed.
4.  **Editor Permissions**: Who should have access to race/class/token editors (imms only, builders, etc.)?
5.  **Grid size for existing mazes:** Need to analyze the actual room layouts in `geldmaze.are` and `maze[1-5].are` to determine appropriate grid dimensions.
6.  **Template room extraction:** How many distinct room "types" exist in each maze area?
7.  **Maze spell UX change:** Currently the spell picks a random room in the area. With instances, the player would get a fresh maze each time (or their existing one). Is this the desired behavior?
8.  **Backward compatibility for `find_dungeon_byplayer()`:** When `DUNGEON_SHARED` is added, does the check need to also verify dungeon hasn't been completed/failed before allowing new players in?
9.  **Scripted layout commands priority:** The `layout`/`links`/`levels` commands are complex. Should they be deferred to a later phase if initial maze areas use static configuration?
10. **Binary `.dat` → `.json` migration:** This plan *assumes* that blueprint/dungeon/class/skill data will be stored directly in area JSON files (for blueprints/dungeons) or individual JSON files (for classes/skills/races). The migration of existing `.dat` binary formats to this embedded/individual JSON format is a prerequisite or a concurrent task that needs a dedicated migration script.

---

## 13. Reference: Key Dev Branch Locations (for Backporting)

| Component | File | Lines |
|-----------|------|-------|
| Maze structs | `src_20_dev/merc.h` | 8137-8189 |
| Dungeon flags | `src_20_dev/merc.h` | 8427-8445 |
| Dungeon index fields | `src_20_dev/merc.h` | 8569-8575 |
| Maze generation | `src_20_dev/blueprint.c` | 1569-1840 |
| Maze OLC | `src_20_dev/blueprint.c` | 3926-4324 |
| Maze file I/O | `src_20_dev/blueprint.c` | 215-250, 833-854 |
| Instance creation | `src_20_dev/blueprint.c` | 2663-2723 |
| Dungeon creation | `src_20_dev/dungeon.c` | 1452-1615 |
| Shared dungeon lookup | `src_20_dev/dungeon.c` | 1679-1695 |
| Player spawn | `src_20_dev/dungeon.c` | 1733-1835 |
| Script commands | `src_20_dev/script_commands.c` | Various |
| Gate types | `src_20_dev/merc.h` | 3702-3715 |
| Instance persistence | `src_20_dev/` + `docs/persist_instances.md` | — |
| Class structs | `src_20_dev/merc.h` | ~lines for `CLASS_DATA`, `CLASS_LEVEL` |
| Skill structs | `src_20_dev/merc.h` | `skill_type` modifications |
| Token-Affect coupling | `src_20_dev/merc.h` | `AFFECT_DATA`, `TOKEN_DATA` modifications |
| `clsedit` | `src_20_dev/editors/classes/clsedit.c` | OLC for classes |
| `skedit` | `src_20_dev/editors/skills/skedit.c` | OLC for skills |
| `tedit` | `src_20_dev/editors/tokens/tedit.c` | OLC for tokens |
| `raceedit` | `src_20_dev/editors/races/raceedit.c` | OLC for races |
| Token structs | `src_20_dev/merc.h` | `TOKEN_INDEX_DATA`, `TOKEN_DATA` |
| `SKILL_ENTRY` | `src_20_dev/merc.h` | `SKILL_ENTRY` modifications |
| Token script commands | `src_20_dev/script_commands.c` | `GRANTSKILL`, `REVOKESKILL`, `AWARD`, `DEDUCT` |
| Token triggers | `src_20_dev/scripts.h`, `src_20_dev/tables.c` | `TRIG_TOKEN_GIVEN`, `TRIG_TOKEN_REMOVED`, `TRIG_PRACTICE`, `TRIG_PRACTICETOKEN`, `TRIG_PREPRACTICETOKEN`, `TRIG_PRETRAINTOKEN` |