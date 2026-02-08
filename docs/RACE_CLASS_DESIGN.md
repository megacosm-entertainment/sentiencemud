# Modern Race/Class System Design - Sentience 1.5

**Date**: 2026-01-06
**Goal**: Create a truly data-driven, framework-ready race/class system with JSON storage and no global pointers

---

## Design Philosophy

### Core Principles:

1. **No Global Pointers** - Eliminate `gr_human`, `gc_mage`, etc.
2. **UID-Based References** - All lookups by unique identifier
3. **JSON Storage** - Human-readable, version-controllable data
4. **Runtime Editable** - Full OLC support without recompilation
5. **Pronoun-Aware** - Native support for custom pronouns
6. **Framework-Ready** - Easy to create completely different settings

---

## Race System Architecture

### Data Structure

```c
typedef struct race_data RACE_DATA;

struct race_data {
    RACE_DATA *next;        // Linked list
    bool valid;

    // Identity
    char *id;               // Unique string identifier (e.g., "human", "elf")
    int16_t uid;            // Numeric UID for binary references
    char *name;             // Display name (e.g., "Human", "High Elf")
    char *description;      // Long description
    char *comments;         // Builder notes

    // Flags
    bool playable;          // Can players choose this?
    bool starting;          // Available at character creation?
    bool remort;            // Remort-only?
    long flags;             // Future expansion

    // Combat/Inherent Properties
    long act[2];            // ACT flags
    long aff[2];            // Permanent affects
    long off;               // Offensive flags
    long imm;               // Immunities
    long res;               // Resistances
    long vuln;              // Vulnerabilities
    long form;              // Body form
    long parts;             // Body parts

    // Player Character Data
    char *who_name;         // Who list display (pronoun-neutral)
    LLIST *skills;          // Racial skill list (skill IDs, not pointers)

    int stats[MAX_STATS];        // Starting stats
    int max_stats[MAX_STATS];    // Maximum stats
    int max_vitals[3];           // Max HP/Mana/Move

    int min_size;
    int max_size;
    int default_alignment;

    // Remort System
    char *remort_race_id;   // ID of prerequisite race (not pointer!)

    // Starting Equipment (VNUMs)
    long starting_eq[5];
};
```

### Key Changes from 2.0_dev:

1. **String ID instead of global pointer**: `char *id` (e.g., "human", "elf", "dragon")
2. **Skills stored as ID strings**: Not pointers to skill objects
3. **Remort race by ID**: `char *remort_race_id` instead of `RACE_DATA *premort`
4. **Pronoun-neutral who_name**: Single string, not sex-based array
5. **No pgr field**: No global race pointer at all

### Lookup System

```c
// Primary lookup methods
RACE_DATA *race_lookup(const char *id);           // By string ID
RACE_DATA *race_lookup_uid(int16_t uid);          // By numeric UID
RACE_DATA *race_lookup_name(const char *name);    // By display name (fuzzy)

// Hash table for O(1) lookup
typedef struct {
    char *key;              // Race ID
    RACE_DATA *value;       // Race data
} RACE_HASH_ENTRY;

extern RACE_HASH_ENTRY *race_hash_table[RACE_HASH_SIZE];
```

### JSON Storage Format

**File**: `data/races/<race_id>.json`

```json
{
  "id": "human",
  "uid": 1,
  "name": "Human",
  "description": "Humans are the most adaptable and widespread race...",
  "comments": "Standard baseline race for new players",

  "playable": true,
  "starting": true,
  "remort": false,

  "display": {
    "who_name": "{WHuman "
  },

  "attributes": {
    "stats": {
      "str": 0, "int": 0, "wis": 0, "dex": 0, "con": 0, "luk": 0
    },
    "max_stats": {
      "str": 18, "int": 18, "wis": 18, "dex": 18, "con": 18, "luk": 18
    },
    "max_vitals": {
      "hp": 100, "mana": 100, "move": 100
    }
  },

  "physical": {
    "size": { "min": 2, "max": 2 },
    "alignment": 0,
    "form": ["edible", "sentient", "biped", "mammal"],
    "parts": ["head", "arms", "legs", "heart", "brains", "guts", "hands", "feet", "fingers", "ear", "eye", "skull"]
  },

  "combat": {
    "act": [],
    "affects": [],
    "offensive": [],
    "immunities": [],
    "resistances": [],
    "vulnerabilities": []
  },

  "skills": [
    "common"
  ],

  "starting_equipment": [3700, 3701, 3702, 3703, 3704],

  "remort": {
    "prerequisite_race": null
  }
}
```

### Code Usage Pattern (No Global Pointers!)

**OLD WAY (2.0_dev):**
```c
if (ch->race == gr_human) {
    // Do human-specific thing
}
```

**NEW WAY (1.5):**
```c
if (!str_cmp(ch->race->id, "human")) {
    // Do human-specific thing
}

// Or even better - use traits/flags instead:
if (IS_SET(ch->race->flags, RACE_HUMANOID)) {
    // Works for ANY humanoid race
}
```

---

## Class System Architecture

### Understanding 2.0_dev's Class System

**2.0_dev has TWO levels:**
1. **CLASS_TYPE** - Base archetype (Mage, Cleric, Thief, Warrior)
2. **CLASS_DATA** - Specific class (Sorcerer, Necromancer, Paladin, etc.)

**Important**: In 2.0_dev, you choose BOTH a type and a specific class.

### Our 1.5 Design

We'll flatten this to a single CLASS_DATA system with optional parent/archetype relationships:

```c
typedef struct class_data CLASS_DATA;

struct class_data {
    CLASS_DATA *next;
    bool valid;

    // Identity
    char *id;               // Unique identifier (e.g., "sorcerer", "paladin")
    int16_t uid;            // Numeric UID
    char *name;             // Display name (e.g., "Sorcerer", "Paladin")
    char *description;      // Long description
    char *comments;         // Builder notes

    // Hierarchy (optional)
    char *archetype_id;     // Parent archetype (e.g., "mage", "warrior")

    // Display
    char *who_name;         // Who list display (pronoun-neutral!)

    // Flags
    bool playable;
    bool starting;
    bool remort;
    long flags;

    // Class Properties
    int primary_stat;       // STAT_INT, STAT_STR, etc.
    int hp_min;             // HP gain per level (min)
    int hp_max;             // HP gain per level (max)
    bool gains_mana;        // Does class gain mana?

    // Skills & Abilities
    LLIST *skill_groups;    // List of skill group IDs
    LLIST *skills;          // Individual skills (ID strings)

    // Starting Equipment
    long weapon_vnum;       // Starting weapon
    long starting_eq[5];    // Additional equipment

    // Multiclass/Remort
    char *prereq_classes[2]; // Prerequisite class IDs
    int max_level;           // Level cap for this class

    // Alignment Restrictions
    int alignment_min;
    int alignment_max;
};
```

### JSON Storage Format

**File**: `data/classes/<class_id>.json`

```json
{
  "id": "sorcerer",
  "uid": 5,
  "name": "Sorcerer",
  "description": "Masters of raw magical power...",
  "comments": "Primary mage subclass",

  "archetype": "mage",

  "display": {
    "who_name": " Sorcerer "
  },

  "playable": true,
  "starting": true,
  "remort": false,

  "attributes": {
    "primary_stat": "int",
    "hp_gain": { "min": 6, "max": 8 },
    "gains_mana": true,
    "max_level": 100
  },

  "skills": {
    "groups": ["mage basics", "sorcerer skills"],
    "individual": []
  },

  "starting_equipment": {
    "weapon": 3717,
    "items": [3700, 3701]
  },

  "restrictions": {
    "alignment": { "min": -1000, "max": 1000 },
    "prerequisites": []
  },

  "remort": {
    "is_remort": false,
    "prerequisite_classes": []
  }
}
```

---

## Pronoun Integration

### Removing Sex-Based Arrays

**OLD (2.0_dev):**
```c
char *who_name[SEX_MAX];  // Different names for male/female/neutral
char *display[SEX_MAX];   // Different displays
```

**NEW (1.5):**
```c
char *who_name;   // Single, pronoun-neutral name
```

**Examples:**
- "Sorcerer" (not "Sorcerer"/"Sorceress")
- "Warrior" (not "Warrior"/"Warrior")
- "Witch" (not "Warlock"/"Witch")

**If gender-specific titles are desired**, use trait system:
```json
{
  "name": "Sorcerer",
  "alternate_names": {
    "feminine": "Sorceress"
  }
}
```

Then in code:
```c
char *get_class_name(CHAR_DATA *ch) {
    // Use alternate name if character prefers it
    if (ch->pcdata->prefer_gendered_titles &&
        ch->body_type == BODY_TYPE_FEMALE &&
        ch->class->alt_name_feminine)
        return ch->class->alt_name_feminine;
    return ch->class->name;
}
```

---

## Eliminating Hardcoded Checks

### Trait/Tag System

Instead of:
```c
if (ch->race == gr_vampire || ch->race == gr_lich) {
    // Undead-specific code
}
```

Use:
```json
{
  "id": "vampire",
  "traits": ["undead", "nocturnal", "blood_drinker"]
}
```

```c
if (race_has_trait(ch->race, "undead")) {
    // Works for ANY undead race
}
```

### Skill/Spell Restrictions

Instead of:
```c
if (ch->class == gc_mage || ch->class == gc_sorcerer) {
    // Can cast fireball
}
```

Use skill groups or direct skill checks:
```c
if (has_skill(ch, "fireball")) {
    // Can cast fireball
}
```

---

## Hash Table Implementation

### Fast Lookups Without Global Pointers

```c
#define RACE_HASH_SIZE 256

// FNV-1a hash function
unsigned int race_hash(const char *id) {
    unsigned int hash = 2166136261u;
    while (*id) {
        hash ^= (unsigned char)(*id++);
        hash *= 16777619u;
    }
    return hash % RACE_HASH_SIZE;
}

RACE_DATA *race_lookup(const char *id) {
    unsigned int hash_idx = race_hash(id);
    RACE_HASH_ENTRY *entry = race_hash_table[hash_idx];

    while (entry) {
        if (!str_cmp(entry->key, id))
            return entry->value;
        entry = entry->next;  // Handle collisions with chaining
    }
    return NULL;
}
```

**Performance**: O(1) average case, same as global pointer access!

---

## Migration Strategy

### Phase 1: Dual System (Compatibility)

**Keep legacy tables temporarily:**
```c
// Legacy system (for reference during migration)
extern const struct race_type race_table[];
extern const struct pc_race_type pc_race_table[];

// New system
extern RACE_DATA *race_list;
extern RACE_HASH_ENTRY *race_hash_table[RACE_HASH_SIZE];
```

**Wrapper for legacy code:**
```c
// Temporary helper during migration
RACE_DATA *race_from_legacy_index(int index) {
    return race_lookup(race_table[index].name);
}
```

### Phase 2: Convert Character References

**Old character files:**
```
Race 5~   // Array index
```

**New character files:**
```json
{
  "race": "human",  // ID string
  "race_uid": 1     // Numeric UID for faster load
}
```

**Load with fallback:**
```c
// Try UID first (faster)
ch->race = race_lookup_uid(race_uid);
if (!ch->race) {
    // Fall back to ID string
    ch->race = race_lookup(race_id_string);
}
if (!ch->race) {
    // Ultimate fallback
    ch->race = race_lookup("human");
}
```

### Phase 3: Convert Code References

**Find all code that uses global pointers:**
```bash
grep -r "gr_[a-z]*" --include="*.c" . | wc -l
```

**Replace patterns:**
```c
// OLD:
if (ch->race == gr_vampire)

// NEW:
if (!str_cmp(ch->race->id, "vampire"))

// BEST:
if (race_has_trait(ch->race, "undead"))
```

### Phase 4: Remove Legacy Code

**Delete:**
- Global race pointer declarations
- `gr_table[]` in tables.c
- Legacy `race_table[]` and `pc_race_table[]`
- `pgr` field from RACE_DATA

---

## File Organization

```
data/
  races/
    human.json
    elf.json
    dwarf.json
    vampire.json
    ...
  classes/
    warrior.json
    mage.json
    sorcerer.json
    paladin.json
    ...
  skill_groups/
    mage_basics.json
    warrior_skills.json
    ...
```

---

## Bootstrap/Import Process

### Converting from Legacy

```c
void bootstrap_races_from_legacy() {
    for (int i = 0; race_table[i].name; i++) {
        RACE_DATA *race = new_race_data();

        // Generate ID from name
        race->id = str_dup(race_table[i].name);
        str_tolower(race->id);  // "Human" -> "human"

        race->uid = i;  // Assign UIDs sequentially
        race->name = str_dup(race_table[i].name);

        // Copy all fields...

        // Save to JSON
        save_race_json(race);

        // Add to hash table
        race_hash_insert(race);
    }
}
```

---

## OLC Editor Integration

### Commands

```
raceedit create <id> <name>     # Create new race
raceedit <id>                   # Edit existing race
raceedit show                   # Show current race
raceedit list                   # List all races
```

### Editor Fields

All fields editable with pronoun-neutral design:
- Identity: id, name, description
- Display: who_name (single, not array)
- Stats: all numeric fields
- Skills: add/remove by ID string
- Traits: add/remove trait tags
- Remort: set by race ID, not pointer

Same pattern for class editor.

---

## Benefits of This Design

### 1. True Framework

- No hardcoded race/class assumptions
- Easy to create sci-fi, fantasy, modern settings
- All data external and versionable

### 2. Performance

- Hash table lookups: O(1) like global pointers
- No penalty for doing it right

### 3. Maintainability

- JSON files easy to edit by hand
- No recompilation for new races/classes
- Git-friendly (one file per race)

### 4. Modern

- Pronoun-neutral from the start
- JSON storage (already in use in 1.0)
- Traits/tags for flexible categorization

### 5. Extensible

- Easy to add new fields (JSON schema evolution)
- Backward compatible with versioning
- Can split into multiple files per setting

---

## Implementation Checklist

### Race System
- [ ] Define RACE_DATA structure (no global pointers!)
- [ ] Implement hash table and lookup functions
- [ ] Create JSON save/load functions
- [ ] Port memory management (new_race_data, free_race_data)
- [ ] Create bootstrap from legacy tables
- [ ] Implement RACEEDIT OLC
- [ ] Update character creation to use race_lookup()
- [ ] Update character save/load for JSON
- [ ] Remove all gr_* references from code
- [ ] Add trait system for flexible checks

### Class System
- [ ] Define CLASS_DATA structure (flattened, no sex arrays)
- [ ] Implement hash table and lookup functions
- [ ] Create JSON save/load functions
- [ ] Port memory management
- [ ] Create bootstrap from legacy tables
- [ ] Implement CLASSEDIT OLC
- [ ] Update character creation
- [ ] Update character save/load
- [ ] Remove all gc_* references
- [ ] Integrate with skill groups

### Pronoun Integration
- [ ] Remove sex-based who_name arrays
- [ ] Single who_name field per class/race
- [ ] Optional gender variant support (alt_name_*)
- [ ] Update display code to use pronouns

### Testing
- [ ] Unit tests for hash table
- [ ] Load/save JSON integrity
- [ ] Character creation with new system
- [ ] Character migration from old format
- [ ] OLC editor functionality
- [ ] Performance benchmarks

---

**End of Design Document**
