# Gemini Analysis: Class & Progression Systems (`src` vs. `src_20_dev`)

## 1. Overview

This document details the architectural differences between the class, skill, and progression systems in the current codebase (`/sentience/src`) and the experimental `src_20_dev` branch.

The findings are based on an analysis by the `codebase_investigator` agent and a direct comparison of the core data structures in each branch.

### Key Architectural Difference

-   **`src` (Current): A Hybrid, Data-Driven System.** The current system has been modernized to load class and skill definitions from external JSON files at boot time, rather than having them hardcoded in C. However, the execution of core abilities still follows a traditional, procedural model: an integer "skill number" (`sn`) is used to find a C function pointer in a global `skill_table` array, which is then executed.

-   **`src_20_dev` (Experimental): A Token-Centric System.** This version heavily expands the role of "tokens." While it retains the capacity for traditional C-based skills, the architecture is designed to treat abilities (skills, spells, affects, songs) as scriptable token instances. These tokens are live entities in the game world that contain their own logic and data. This is a more object-oriented and flexible approach, where the C code provides a framework for the scriptable-token-driven ability system.

The primary difference is the **method of dispatch**. `src` primarily calls C functions via an integer ID, while `src_20_dev` is built to act upon token objects that carry their own scripted logic.

---

## 2. Data Structure Comparison

The most direct way to understand the architectural divergence is to compare the core data structures.

### 2.1. The Player's Skill: `SKILL_ENTRY`

This struct represents an ability that a character has learned.

#### `src_20_dev/merc.h`

```c
typedef struct skill_entry_type
{
    struct skill_entry_type *next;
    char source;
    long flags;
    bool isspell;
    SKILL_DATA *skill;
    SONG_DATA *song;
    TOKEN_DATA *token; // <<< Live token
    int rating;
    int mod_rating;
} SKILL_ENTRY;
```

#### `src/merc.h`

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

#### Analysis

The `src_20_dev` version identifies an ability by a pointer to a `TOKEN_DATA` instance. The `src` version identifies it by an integer `sn`, which is an index for the global `skill_table`. This is the clearest illustration of the "object-oriented" vs. "procedural" dispatch models.

---

### 2.2. Master Skill Definition: `SKILL_DATA` vs. `skill_type`

This structure defines a skill in the master list for the whole game.

#### `src_20_dev/merc.h` (`struct skill_data`)

```c
struct skill_data
{
    SKILL_DATA *next;
    bool valid;
    int16_t uid;
    bool isspell;
    char *name;
    // ... other fields ...
    
    // Functions only used when the spell is a source ability:
    // Token abilities will call their respective triggers
    SPELL_FUN *prespell_fun;
    SPELL_FUN *spell_fun;
    // ... other function pointers ...
};
```

#### `src/merc.h` (`struct skill_type`)

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

#### Analysis

`src`'s `skill_type` is a classic MUD structure containing a direct `spell_fun` pointer. This is the function that gets called when a player uses the skill. In `src_20_dev`, the `skill_data` struct has function pointers too, but the comments explicitly state they are for "source abilities" (hardcoded C skills) and that "Token abilities will call their respective triggers." This shows `src_20_dev` was built to prioritize the token's scripted logic over a hardcoded C function.

---

### 2.3. The Class Definition: `CLASS_DATA`

-   **`src`**: The investigator found no `CLASS_DATA` struct in `src/merc.h`. This is because class definitions are loaded from JSON files into memory at boot time. This is a modern, data-driven approach.

-   **`src_20_dev`**: This version uses a C struct to define classes, which are then linked together. This is still data-driven, but defined at the C-level rather than purely in external files.

#### `src_20_dev/merc.h`

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

---

### 2.4. The Token: `TOKEN_DATA`

Both versions have a `struct token_data` that is remarkably similar. This suggests the concept of a "token" as a scriptable entity exists in both, but its integration into the core progression system is the key differentiator.

#### `src_20_dev/merc.h`

```c
struct token_data
{
    char __type;
    bool gc;
    TOKEN_INDEX_DATA *pIndexData;
    // ... pointers ...
    PROG_DATA *progs;
    EVENT_DATA *events;
    // ... other fields ...
    unsigned long id[2];
    long value[MAX_TOKEN_VALUES];
    SKILL_ENTRY *skill;
    LLIST *affects;
    REPUTATION_DATA *reputation;
    int tempstore[MAX_TEMPSTORE];
};
```

#### `src/merc.h`

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

#### Analysis

The structures are nearly identical. The `src_20_dev` version has an extra `REPUTATION_DATA` pointer, but the core components (`progs`, `events`, `value` array) that enable scripting are present in both. The difference is not in the token itself, but in how deeply it's integrated as the primary driver for skills and class abilities.

---

## 3. Conclusion for Backporting

A potential backport of the `src_20_dev` progression system would be a significant architectural refactor. It would require:

1.  Changing the `skill_table` from an array of `skill_type` to a list of `skill_data`.
2.  Modifying the `SKILL_ENTRY` on the player to prioritize the `token` pointer.
3.  Rewriting the skill dispatch logic (`do_cast`, etc.) to check for and execute token scripts first, before falling back to C function pointers.
4.  Creating a migration path for existing player skills from the `sn` integer system to the new token-based system.
5.  Integrating the `CLASS_DATA` struct and its associated functions for granting token-based abilities upon level-up, replacing or augmenting the current JSON-based loading.

The effort would be substantial but would result in a far more flexible and dynamic in-game system for creating and modifying abilities without recompiling.
