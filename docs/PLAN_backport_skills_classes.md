# Backport Plan: Skill, Class, and Supporting System Overhaul

**Date:** February 13, 2026
**Status:** Active — Phases 0-8 Complete, Phase 9 pending
**Supersedes:** Previous version of this doc, relevant sections of `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`

---

## 1. Design Principles

1. **No global pointers.** All `gsn_*` globals and any proposed `gsk_*` replacements are eliminated. Skills, spells, songs, and classes are accessed via lookup functions (like the existing race system), not compile-time globals.
2. **String-based primary IDs.** The canonical identifier for a skill/spell/song/class is its name string. Integer UIDs exist for internal/serialization efficiency but are never the primary lookup key.
3. **Individual JSON files.** Each skill, spell, song, and class is stored as its own JSON file in a dedicated data directory, consistent with the race system.
4. **Single spell function with invocation method.** Instead of separate function pointers per invocation method (cast/quaff/recite/brandish/zap/etc. as in src_20_dev), each spell has one `SPELL_FUN` that receives an invocation method parameter. Spells can branch on invocation type when needed, or ignore it for uniform behavior.
5. **Incremental migration.** The system bootstraps from existing hardcoded tables on first run, generating JSON files. Existing code continues to work during the transition via compatibility shims.

---

## 2. Current System Summary

### 2.1. Skill/Spell System

| Component | Detail |
|-----------|--------|
| `skill_table[MAX_SKILL]` | Static const array in `const.c`, ~300 entries |
| `gsn_*` globals | 272 `extern int16_t` declarations in `merc.h`, ~1700 usages across codebase |
| `skill_table[]` references | ~435 direct array accesses |
| `SPELL_FUN` signature | `bool (int sn, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc)` |
| Spell functions | ~174 `DECLARE_SPELL_FUN` in headers |
| Player storage | `int learned[MAX_SKILL]` + `int mod_learned[MAX_SKILL]` arrays indexed by `sn` |
| Runtime tracking | `SKILL_ENTRY` linked list on `CHAR_DATA` with `int16_t sn` field |
| Lookup | `skill_lookup()` — linear scan with prefix match, returns `sn` |
| Song system | `music_table[MAX_SONGS]` static array, ~20 references |

### 2.2. Token-Affect System

| Component | Detail |
|-----------|--------|
| `TOKEN_DATA.affects` | `LLIST *` — tracks affects created by this token |
| `TOKEN_DATA.skill` | `SKILL_ENTRY *` — back-link to skill entry if token provides a skill |
| `AFFECT_DATA.token` | `TOKEN_DATA *` — source token (if affect came from a token) |
| `AFFECT_DATA.type` | `int16_t` — currently stores `sn` (skill number) |

### 2.3. Class System

| Component | Detail |
|-----------|--------|
| `class_table[MAX_CLASS]` | 4 base classes (mage/cleric/thief/warrior) |
| `sub_class_table[]` | Subclass definitions with prerequisites |
| `PC_DATA` fields | 16 integer fields for class/subclass tracking |
| References | ~131 across codebase |

---

## 3. Target Architecture

### 3.1. Skill/Spell Data Structure

```c
/* New SKILL_DATA — replaces const skill_type entries */
typedef struct skill_data SKILL_DATA;
struct skill_data {
    SKILL_DATA *    next;           /* Hash chain / list link */
    bool            valid;
    int16_t         uid;            /* Stable unique ID (for serialization, affects) */
    bool            isspell;        /* true = spell, false = skill */

    char *          name;           /* Canonical name (primary lookup key) */
    char *          display;        /* Display name (if different from name) */
    char *          help_keyword;   /* Help system keyword */
    char *          summary;        /* One-line summary for hints/MXP */

    long            flags;          /* SKILL_FLAG_* bitfield */

    LLIST *         class_levels;   /* LLIST of SKILL_CLASS_LEVEL — per-class availability */
    int16_t         default_level;  /* Fallback level when class_levels is empty */
    int16_t         difficulty;     /* Base difficulty rating */

    /* Invocation */
    SPELL_FUN *     spell_fun;      /* Spell function (NULL for non-spell skills) */
    int16_t         target;         /* TAR_* target type */
    int16_t         minimum_position; /* Minimum position to use */
    int16_t         min_mana;       /* Mana cost */
    int16_t         beats;          /* Lag after use */

    /* Racial restriction */
    RACE_DATA *     race;           /* NULL = any race, otherwise racial-only */

    /* Messages */
    char *          noun_damage;    /* Damage noun ("acid blast") */
    char *          msg_off;        /* Wear-off message */
    char *          msg_obj;        /* Object wear-off message */
    char *          msg_disp;       /* Display message */

    /* Crafting / ink */
    int             inks[3][2];

    /* Token coupling (for token-driven spells) */
    TOKEN_INDEX_DATA *token_index;  /* Associated token template */
    WNUM            token_wnum;     /* Widevnum for deferred resolution */

    /* Generic values for extensibility */
    int             values[MAX_SKILL_VALUES];
    char *          value_names[MAX_SKILL_VALUES];
};

/* Per-class skill level availability */
typedef struct skill_class_level SKILL_CLASS_LEVEL;
struct skill_class_level {
    CLASS_DATA *    clazz;          /* Which class */
    int16_t         level;          /* Level at which this class gets the skill */
    int16_t         rating;         /* Difficulty rating for this class */
};
```

### 3.2. Invocation Method Constants

```c
/* Spell invocation methods — passed to SPELL_FUN */
#define INVOC_CAST          0   /* Normal spellcasting */
#define INVOC_QUAFF         1   /* Drinking a potion */
#define INVOC_RECITE        2   /* Reading a scroll */
#define INVOC_BRANDISH      3   /* Brandishing a staff */
#define INVOC_ZAP           4   /* Zapping with a wand */
#define INVOC_BREW          5   /* Brewing into a potion */
#define INVOC_SCRIBE        6   /* Scribing onto a scroll */
#define INVOC_INK           7   /* Tattooing */
#define INVOC_IMBUE         8   /* Imbuing an item */
#define INVOC_EQUIP         9   /* Equipment-triggered effect */
#define INVOC_TOUCH         10  /* Contact/touch effect */
#define INVOC_TOKEN         11  /* Token script trigger */
#define INVOC_INTERNAL      12  /* Internal/system call */
#define MAX_INVOC           13
```

### 3.3. Updated SPELL_FUN Signature

```c
/* Old: */
typedef bool SPELL_FUN(int sn, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc);

/* New: */
typedef bool SPELL_FUN(skill_t *skill, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc, int invocation);

/* Updated macros: */
#define SPELL_FUNC(s) bool s(skill_t *skill, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc, int invocation)
#define DECLARE_SPELL_FUN(fun) SPELL_FUN fun
```

### 3.4. Updated SKILL_ENTRY (Per-Character)

```c
typedef struct skill_entry_type {
    struct skill_entry_type *next;
    char            source;     /* SKILLSRC_NORMAL, SKILLSRC_SCRIPT, etc. */
    long            flags;
    bool            practice;   /* Can be practiced? */
    bool            improve;    /* Can improve through use? */
    bool            isspell;

    /* New: direct pointer to SKILL_DATA (primary reference) */
    SKILL_DATA *    skill;      /* Pointer to master skill definition */

    /* Legacy compat: keep sn during migration, remove in Phase 9 */
    int16_t         sn;

    int16_t         song;       /* Song number (legacy, migrate later) */
    TOKEN_DATA *    token;      /* Token providing this skill (NULL = built-in) */

    /* New: rating stored per-entry instead of in learned[] array */
    int             rating;     /* Skill percentage (0-100+) */
    int             mod_rating; /* Rating modifier */

    /* Cross-class scope tracking (set when reward is applied) */
    int16_t         cross_class_scope;  /* REWARD_SCOPE_* — how this skill shares across classes */
    CLASS_DATA *    source_class;       /* Class that granted this skill (NULL = not class-granted) */
} SKILL_ENTRY;
```

### 3.5. Updated AFFECT_DATA

```c
struct affect_data {
    /* ... existing fields ... */
    SKILL_DATA *    skill;      /* NEW: Direct pointer to skill (replaces int16_t type) */
    int16_t         type;       /* LEGACY: kept during migration, remove in Phase 9 */
    TOKEN_DATA *    token;      /* Source token (already exists) */
    /* ... rest of existing fields ... */
};
```

### 3.6. Skill Lookup API

```c
/* Primary lookup — returns NULL if not found */
SKILL_DATA *    skill_find(const char *name);           /* Exact match */
SKILL_DATA *    skill_search(const char *prefix);       /* Prefix match */
SKILL_DATA *    skill_find_uid(int16_t uid);            /* UID lookup (for deserialization) */

/* Convenience */
const char *    skill_name(SKILL_DATA *skill);

/* Character skill access */
SKILL_ENTRY *   skill_entry_find(CHAR_DATA *ch, SKILL_DATA *skill);
int             get_skill_rating(CHAR_DATA *ch, SKILL_DATA *skill);

/* Global iteration */
SKILL_DATA *    skill_first(void);                      /* First in global list */
SKILL_DATA *    skill_next(SKILL_DATA *skill);          /* Next in global list */
int             skill_count(void);                      /* Total count */

/* Bootstrap and persistence */
void            load_skills(void);                      /* Boot: load from JSON or bootstrap */
void            save_skill(SKILL_DATA *skill);          /* Save single skill to JSON */
void            save_all_skills(void);                  /* Save all to JSON */
```

### 3.7. Skill Group Data Structure

The legacy `group_table[MAX_GROUP]` is a static const array with `rating[MAX_CLASS]`
per group and `spells[MAX_IN_GROUP]` as skill name strings. PC_DATA tracks membership
with `bool group_known[MAX_GROUP]`. In src_20_dev, groups became `SKILL_GROUP` objects
with `name + LLIST *contents` (skill name strings), stored in `skill_groups_list`,
and `group_known` became an `LLIST *` of `SKILL_GROUP *` pointers.

```c
/* New data-driven group (matches src_20_dev SKILL_GROUP) */
typedef struct skill_group_data SKILL_GROUP;
struct skill_group_data {
    SKILL_GROUP *   next;
    bool            valid;

    char *          name;               /* Group name ("warrior basics", etc.) */
    LLIST *         contents;           /* LLIST of char * — skill name strings */
};
```

Groups are the bridge between classes and skills:
- Each `CLASS_DATA` has `LLIST *groups` of group name strings (currently; migrating to `SKILL_GROUP *`)
- Each `SKILL_GROUP` lists skill names; iterating them yields `SKILL_CLASS_LEVEL` entries
- Class bootstrap creates `REWARD_GROUP` entries for each group assignment
- `resolve_class_rewards_to_skill_levels()` (Phase 6e) iterates
  class→rewards→groups→skills to build `SKILL_CLASS_LEVEL` entries on `SKILL_DATA`
- Player `PC_DATA.group_known` tracks which groups the character has acquired

**Implementation status:** SKILL_GROUP is implemented in `skill_group.c` / `skill_group.h`.
Groups are loaded from `data/skill_groups/` JSON files, or bootstrapped from `group_table[]`
on first run. Lookup: `skill_group_find()` (exact), `skill_group_search()` (prefix).

**Migration path:**
1. Bootstrap `SKILL_GROUP` from current `group_table[]` entries
2. Store as JSON alongside skills/classes (or in `data/skill_groups/`)
3. Eventually replace `bool group_known[MAX_GROUP]` with `LLIST *group_known`
4. Remove `group_table[]` from `const.c` in Phase 9

### 3.8. Class System Data Structures

#### Class Type Constants

```c
/* Class type categories — broader than the original 4 base classes */
#define CLASS_TYPE_NONE         -1
#define CLASS_TYPE_MAGE          0      /* Arcane caster */
#define CLASS_TYPE_CLERIC        1      /* Divine caster */
#define CLASS_TYPE_THIEF         2      /* Stealth/agility */
#define CLASS_TYPE_WARRIOR       3      /* Melee combat */
#define CLASS_TYPE_CRAFTING      4      /* Item creation (smithing, alchemy, etc.) */
#define CLASS_TYPE_GATHERING     5      /* Resource collection (mining, herbalism, etc.) */
#define CLASS_TYPE_EXPLORER      6      /* Discovery/navigation */
```

#### Class Flags

```c
#define CLASS_COMBATIVE         (A)    /* Class participates in combat */
#define CLASS_NO_LEVEL          (B)    /* Levels do NOT count toward tot_level */
#define CLASS_CASTER            (C)    /* Class uses mana */
#define CLASS_HIDDEN            (D)    /* Not shown in class lists by default */
#define CLASS_REMORT_ONLY       (E)    /* Requires remort to access */
#define CLASS_DEFAULT           (F)    /* Auto-assigned to new characters on creation */
```

#### CLASS_DATA

```c
typedef void CLASS_LEAVE_FUN(CHAR_DATA *ch);
typedef void CLASS_ENTER_FUN(CHAR_DATA *ch);

typedef struct class_data CLASS_DATA;
struct class_data {
    CLASS_DATA *    next;               /* Hash chain / global list link */
    bool            valid;

    /* Identity */
    int16_t         uid;                /* Stable unique ID */
    char *          name;               /* Canonical name (primary lookup key, e.g. "paladin") */
    char *          description;        /* Long description */

    /* Body-type-aware display names (indexed by body_type_t) */
    char *          display[BODY_TYPE_MAX]; /* Full display name per body type */
    char *          who[BODY_TYPE_MAX];    /* Short name for 'who' list per body type */

    /* Classification */
    int16_t         type;               /* CLASS_TYPE_* category */
    long            flags;              /* CLASS_* bitfields */

    /* Skills/Groups */
    LLIST *         groups;             /* Skill groups this class gets (LLIST of SKILL_GROUP *) */

    /* Progression */
    int16_t         primary_stat;       /* Primary attribute (STAT_STR, etc.) */
    int16_t         max_level;          /* Maximum level achievable in this class */
    int             hp_min;             /* Min HP gain per level */
    int             hp_max;             /* Max HP gain per level */
    bool            gains_mana;         /* Whether class gains mana on level */
    long            weapon;             /* Starting weapon vnum */

    /* Lifecycle callbacks */
    CLASS_ENTER_FUN *enter;             /* Called when player switches TO this class */
    CLASS_LEAVE_FUN *leave;             /* Called when player switches FROM this class */

    /* Global cached pointer (replaces gcl_* globals from src_20_dev) */
    CLASS_DATA **   gcl;                /* Pointer-to-pointer for fast lookup caching */
};
```

**Note on body types vs SEX:** The codebase has migrated from `SEX_NEUTRAL/MALE/FEMALE`
to `body_type_t` (`BODY_TYPE_NEUTRAL/MALE/FEMALE/OTHER/RANDOM`) with custom pronoun
strings per character. Class display names are indexed by `body_type_t` rather than the
legacy `SEX_*` constants. `BODY_TYPE_RANDOM` and `BODY_TYPE_OTHER` slots may be empty
(fall back to `BODY_TYPE_NEUTRAL`). Characters with custom pronouns use whatever body
type they selected; the class display name matches their body type, not their pronouns.

#### CLASS_LEVEL (Per-Character)

```c
typedef struct class_level CLASS_LEVEL;
struct class_level {
    CLASS_LEVEL *   next;
    bool            valid;

    CLASS_DATA *    clazz;              /* Which class */
    int             level;              /* Current level in this class */
    long            xp;                 /* Current XP in this class */

    /* Class-specific custom data (ranger pets, crafting mastery, etc.) */
    json_t *        custom_data;        /* Free-form JSON object, NULL if unused */
};
```

#### CLASS_LEVEL Custom Data

The `custom_data` field on `CLASS_LEVEL` is a Jansson `json_t *` object that stores
arbitrary class-specific state per character. Examples:

- **Ranger:** pet data, companion bonuses
- **Crafter:** mastery levels per recipe category, materials discovered
- **Explorer:** locations discovered, mapping progress

This data is saved/loaded as a nested JSON object within the character's class section.
Classes define their own schema; the core system treats it as opaque. The `enter`/`leave`
callbacks on `CLASS_DATA` can initialize or teardown custom_data.

#### Updated PC_DATA Class Fields

```c
struct pc_data {
    /* ... existing fields ... */

    /* New class system */
    LLIST *             classes;            /* LLIST of CLASS_LEVEL */
    CLASS_LEVEL *       current_class;      /* Active class (points into classes list) */

    /* Legacy class fields — kept during migration, removed in Phase 9 */
    int                 class_current;      /* index 0-3 */
    int                 sub_class_current;  /* active sub-class index */
    int                 class_mage;
    int                 second_class_mage;
    int                 sub_class_mage;
    int                 second_sub_class_mage;
    int                 class_cleric;
    int                 second_class_cleric;
    int                 sub_class_cleric;
    int                 second_sub_class_cleric;
    int                 class_thief;
    int                 second_class_thief;
    int                 sub_class_thief;
    int                 second_sub_class_thief;
    int                 class_warrior;
    int                 second_class_warrior;
    int                 sub_class_warrior;
    int                 second_sub_class_warrior;
};
```

### 3.9. Class Lookup API

```c
/* Primary lookups */
CLASS_DATA *    class_find(const char *name);           /* Prefix match */
CLASS_DATA *    class_find_exact(const char *name);     /* Exact match */
CLASS_DATA *    class_find_uid(int16_t uid);            /* By UID */

/* Convenience */
const char *    class_name(CLASS_DATA *clazz);
const char *    class_display(CLASS_DATA *clazz, body_type_t body_type);
const char *    class_who(CLASS_DATA *clazz, body_type_t body_type);
const char *    class_display_ch(CLASS_DATA *clazz, CHAR_DATA *ch);   /* Uses ch->body_type */
const char *    class_who_ch(CLASS_DATA *clazz, CHAR_DATA *ch);       /* Uses ch->body_type */

/* Character class access */
CLASS_DATA *    get_current_class(CHAR_DATA *ch);
CLASS_LEVEL *   get_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);  /* NULL clazz = current */
bool            has_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);
void            add_class_level(CHAR_DATA *ch, CLASS_DATA *clazz, int level);
void            remove_class_level(CHAR_DATA *ch, CLASS_DATA *clazz);
void            insert_class_level(CHAR_DATA *ch, CLASS_LEVEL *cl);
bool            is_current_class_combat(CHAR_DATA *ch);

/* Global iteration */
CLASS_DATA *    class_first(void);                      /* First in alphabetical list */
int             class_count(void);                      /* Total loaded count */

/* Boot / Persistence */
void            load_class_data(void);                  /* Load from JSON or bootstrap */
void            save_class_data(CLASS_DATA *clazz);     /* Save single class to JSON */
void            save_all_class_data(void);              /* Save all to JSON */
```

### 3.10. Class Reward System

Classes grant skills, titles, bonuses, tokens, and trigger scripts at specific levels
via a unified **reward** (unlock) system. This replaces the skill-centric
`SKILL_CLASS_LEVEL` approach (where each skill lists which classes get it) with a
class-centric approach (each class defines what it grants at each level).

#### Reward Type Constants

```c
#define REWARD_SKILL        0   /* Grant access to a skill at a given rating */
#define REWARD_GROUP        1   /* Grant all skills in a skill group */
#define REWARD_TITLE        2   /* Change class display/who name at this level */
#define REWARD_BONUS        3   /* Grant a stat or attribute bonus */
#define REWARD_TOKEN        4   /* Grant a token to the character */
#define REWARD_SCRIPT       5   /* Execute a script against the character */
#define REWARD_CUSTOM       6   /* Write to CLASS_LEVEL.custom_data */
#define REWARD_TRAIT        7   /* Grant or override a trait value */
#define MAX_REWARD_TYPE     8
```

#### Reward Flags

```c
#define REWARD_REVOKE_ON_LEAVE  (A)  /* Revoked when leaving this class */
#define REWARD_ONE_TIME         (B)  /* Only triggers once (not re-applied on reload) */
#define REWARD_HIDDEN           (C)  /* Not shown in class level list */
```

#### Reward Scope Constants

Each reward has a **scope** that controls cross-class availability. When a class
grants a skill via `REWARD_SKILL` or `REWARD_GROUP`, the scope determines whether
that skill is available only on the granting class, on all classes of the same type,
on all combative classes, or always.

```c
#define REWARD_SCOPE_CLASS      0   /* Only when the granting class is active */
#define REWARD_SCOPE_TYPE       1   /* Any class of the same CLASS_TYPE_* */
#define REWARD_SCOPE_COMBAT     2   /* Any class with CLASS_COMBATIVE flag */
#define REWARD_SCOPE_ALWAYS     3   /* Always available regardless of active class */
#define MAX_REWARD_SCOPE        4
```

The scope is stored on `CLASS_REWARD.scope` and propagated to `SKILL_ENTRY.cross_class_scope`
when the reward is applied. `SKILL_ENTRY.source_class` tracks which class granted the skill.
The `get_skill()` function checks these fields to determine if a skill is usable based on the
character's current active class:

- `SCOPE_CLASS`: `source_class == current_class`
- `SCOPE_TYPE`: `source_class->type == current_class->type`
- `SCOPE_COMBAT`: both `source_class` and `current_class` have `CLASS_COMBATIVE`
- `SCOPE_ALWAYS`: always available

When `REWARD_GROUP` has a scope, each skill expanded from the group inherits that scope.

#### CLASS_REWARD Struct

```c
typedef struct class_reward CLASS_REWARD;
struct class_reward {
    CLASS_REWARD *  next;
    bool            valid;

    int16_t         level;          /* Level at which this reward is granted */
    int16_t         type;           /* REWARD_* constant */
    int16_t         scope;          /* REWARD_SCOPE_* cross-class availability */
    char *          name;           /* What is being granted (skill/group/script name, etc.) */
    int             value;          /* Type-specific: skill rating, bonus amount, etc. */
    long            flags;          /* REWARD_* flags */

    /* Extended data for complex reward types (titles, token wnums, script args) */
    struct json_t * data;           /* Optional JSON object for type-specific parameters */
};
```

#### How Reward Types Use Fields

| Type | `name` | `value` | `data` |
|------|--------|---------|--------|
| `REWARD_SKILL` | Skill name | Rating (difficulty) | NULL |
| `REWARD_GROUP` | Group name | — | NULL |
| `REWARD_TITLE` | — | — | `{"display": {"neutral":..., "male":..., "female":..., "other":...}, "who": {...}}` |
| `REWARD_BONUS` | Stat name ("hp_max", "mana_max", etc.) | Amount | NULL |
| `REWARD_TOKEN` | Token name (display) | — | `{"wnum": "1:100"}` or `{"auid": 1, "vnum": 100}` |
| `REWARD_SCRIPT` | Script name/identifier | — | `{"args": {...}}` (optional arguments) |
| `REWARD_CUSTOM` | JSON path (key to set in custom_data) | — | Value to write (any JSON) |
| `REWARD_TRAIT` | Trait ID (e.g., "blood_feeding") | Int value (for TRAIT_INTEGER) | `{"bool": true}` or `{"string": "value"}` for non-int types |

#### CLASS_DATA Updates

```c
struct class_data {
    /* ... existing fields ... */

    /* Reward/unlock progression */
    LLIST *         rewards;            /* LLIST of CLASS_REWARD, sorted by level */
};
```

#### OLC Syntax (clsedit)

```
reward <level> skill <skill_name> [rating]
reward <level> group <group_name>
reward <level> title <display_name>             (sets neutral; use 'titleset' for per-body-type)
reward <level> titleset <body_type> <name>       (set specific body type display)
reward <level> bonus <stat_name> <amount>
reward <level> token <wnum>
reward <level> script <script_name>
reward <level> custom <key> <json_value>
reward <level> trait <trait_id> [value]          (grant/override a trait at this level)

rewardflags <level> <type> <flags>               (set REVOKE_ON_LEAVE, ONE_TIME, HIDDEN)
reward remove <level> <type> [name]               (remove a specific reward)
rewards                                            (list all rewards for this class)
```

#### Interaction with SKILL_CLASS_LEVEL

During boot, after classes and skills are loaded, class rewards of type `REWARD_SKILL`
and `REWARD_GROUP` are resolved into `SKILL_CLASS_LEVEL` entries on each `SKILL_DATA`:

```
resolve_class_rewards_to_skill_levels():
    for each CLASS_DATA:
        for each CLASS_REWARD where type == REWARD_SKILL:
            find SKILL_DATA by reward->name
            create/update SKILL_CLASS_LEVEL { clazz, level, rating }
        for each CLASS_REWARD where type == REWARD_GROUP:
            find SKILL_GROUP by reward->name
            for each skill name in group->contents:
                find SKILL_DATA by name
                create/update SKILL_CLASS_LEVEL { clazz, reward->level, 1 }
```

This means `SKILL_CLASS_LEVEL` becomes a derived/cached structure populated from class
rewards, rather than a manually maintained mapping. Skills no longer need to independently
list which classes get them — the class defines its own progression.

#### Applying Rewards

- **On level-up:** Iterate rewards for the new level, apply each:
  - `REWARD_SKILL`: Grant skill via `skill_entry_insert()`
  - `REWARD_GROUP`: Iterate group contents, grant each skill
  - `REWARD_TITLE`: Update `CLASS_LEVEL` display/who overrides
  - `REWARD_BONUS`: Apply bonus to character
  - `REWARD_TOKEN`: Create token on character via `create_token()`
  - `REWARD_SCRIPT`: Execute named script with character as target
  - `REWARD_CUSTOM`: Write JSON value to `CLASS_LEVEL.custom_data`

- **On class join:** Apply all rewards from level 1 through current level

- **On class leave:** Revoke rewards with `REWARD_REVOKE_ON_LEAVE` flag

- **On character load:** Reconstruct state from current level (skip `ONE_TIME` rewards
  unless they haven't been applied — tracked via a `"rewards_applied"` array in custom_data)

#### JSON Format

```json
"rewards": [
    { "level": 1,  "type": "group",  "name": "warrior basics" },
    { "level": 1,  "type": "skill",  "name": "sword", "value": 1 },
    { "level": 5,  "type": "skill",  "name": "second attack", "value": 3 },
    { "level": 10, "type": "title",  "data": {
        "display": { "neutral": "Champion", "male": "Champion", "female": "Champion" }
    }},
    { "level": 15, "type": "token",  "name": "Warrior's Mark", "data": { "auid": 1, "vnum": 100 } },
    { "level": 20, "type": "script", "name": "warrior_level_20_unlock" },
    { "level": 25, "type": "bonus",  "name": "hp_max", "value": 50 },
    { "level": 30, "type": "title",  "data": {
        "display": { "neutral": "Warlord", "male": "Warlord", "female": "Warlord" },
        "who": { "neutral": "Wrl", "male": "Wrl", "female": "Wrl" }
    }}
]
```

### 3.11. Security: Level vs. Staff Rank

With multi-classing across many class types (combat, crafting, gathering, exploration),
players can accumulate far more total levels than the current mortal cap of 120. The
`tot_level` field becomes a progression metric, NOT a permission/authority indicator.

**Critical invariant:** Staff authority is ONLY checked via `get_staff_rank(ch)` and the
`IS_STAFF(ch, rank)` / `IS_IMMORTAL(ch)` macros. The `get_trust(ch)` function and
`IS_HERO(ch)` / `IS_TRUSTED(ch, level)` macros must NEVER gate staff-level capabilities.

#### Sites that need audit/migration — COMPLETED

All items below were fixed. Additional `tot_level >= LEVEL_IMMORTAL` checks were also
found and fixed (handler.c drop check, handler.c count_players, act_info.c who/whois x3,
act_wiz.c ostat x2, special.c pickpocket, act_wiz.c force gods tot_level check).

| File | Line | Original Check | Fix Applied |
|------|------|----------------|-------------|
| `handler.c` | 913,930 | `ch->level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(ch)` |
| `handler.c` | 6548 | `ch->tot_level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(ch)` |
| `handler.c` | 11369 | `wch->tot_level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(wch)` |
| `update.c` | 807 | `ch->level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(ch)` |
| `update.c` | 3131 | `wch->level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(wch)` |
| `special.c` | 966 | `victim->tot_level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(victim)` |
| `act_info.c` | 5114,5122,5255 | `wch->tot_level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(wch)` |
| `act_wiz.c` | 3176,3181 | `ch->tot_level >= LEVEL_IMMORTAL` | ✅ `IS_IMMORTAL(ch)` |
| `act_wiz.c` | 9026 | `ch->tot_level < MAX_LEVEL - 1` | ✅ `!IS_STAFF(ch, STAFF_SUPREMACY)` |
| `act_wiz.c` | 9038 | `desc->character->level >= LEVEL_HERO` | ✅ `IS_IMMORTAL(desc->character)` |
| `fight.c` | 5982 | `ch->level > LEVEL_HERO` | ✅ `IS_IMMORTAL(ch)` |
| `pfile_migrate.c` | 640 | `get_trust(ch) < MAX_LEVEL` | ✅ `!IS_IMPLEMENTOR(ch)` |
| `act_wiz.c` | 6100 | `level > get_trust(ch)` | ✅ `level > MAX_LEVEL && !IS_IMPLEMENTOR(ch)` |
| `act_wiz.c` | 6276 | `level > get_trust(ch)` | ✅ `level > MAX_LEVEL && !IS_IMPLEMENTOR(ch)` |
| `editors/objects/oedit.c` | 664 | `i > get_trust(ch)` | ✅ `i > MAX_LEVEL` |
| `olc.c` | 2719 | `get_trust(ch) < MAX_LEVEL - 4` | Already commented out |
| `act_obj.c` | 7847 | `obj->level >= LEVEL_IMMORTAL` | OK — object level check |
| `interp.c` | 1445 | `cmd->level >= LEVEL_IMMORTAL` | OK — command level check |
| `act_obj.c` | 3333,9124 | `obj->level > LEVEL_HERO` | OK — object level check |
| `handler.c` | 2431 | `obj->level > LEVEL_HERO` | OK — object level check |
| `script_*pcmds.c` | various | `get_trust()` for NPC mob level | OK — Phase 9 rename |

**`get_trust()` itself** will be redefined. During migration it continues to return
`tot_level` for backward compatibility with scripts that use NPC mob levels for ordering.
Long-term, all player authority checks must use staff rank.

### 3.12. Level Overflow: Bonus Free Levels

When a class's `max_level` is lowered below what a player already has, the excess levels
become **free levels** stored as a character-scoped account bonus:

```c
#define BONUS_FREE_LEVELS     7  /* Free class levels (value = count) */
```

Mechanics:

1. On character load, for each `CLASS_LEVEL` where `cl->level > cl->clazz->max_level`:
   - Calculate overflow: `overflow = cl->level - cl->clazz->max_level`
   - Clamp: `cl->level = cl->clazz->max_level`
   - Add `overflow` to existing `BONUS_FREE_LEVELS` bonus (or create one)
   - Log the adjustment
2. Free levels can be applied to any class via a command (e.g., `freelevel <class>`)
3. Each application grants one level in the target class (if under max_level)
4. Free level bonuses are permanent (expires_at = 0), character-scoped
5. Display in `score` or `bonus` command output

### 3.13. Multi-Layer Trait System

Traits are data-driven properties that replace hardcoded race/class identity checks
(e.g., `IS_RACE(ch, "vampire")` or `class == CLASS_CLERIC_DRUID`). They exist at
three layers, queried in priority order:

1. **Personal** (`PC_DATA.trait_values`) — per-character overrides (quest rewards, admin grants)
2. **Class** (`CLASS_DATA.trait_values`) — inherent to the class definition
3. **Race** (`RACE_DATA.trait_values`) — racial defaults (existing system)

#### Merge Semantics

- **Boolean traits**: OR across layers — true at any layer = true
- **Integer traits**: highest-priority explicitly-set layer wins
- **String traits**: highest-priority explicitly-set layer wins

#### Unified Query API

```c
/* Layered queries: personal > class > race */
bool        ch_has_trait(CHAR_DATA *ch, const char *trait_id);
bool        ch_get_trait_bool(CHAR_DATA *ch, const char *trait_id);
int         ch_get_trait_int(CHAR_DATA *ch, const char *trait_id);
const char *ch_get_trait_string(CHAR_DATA *ch, const char *trait_id);

/* Per-layer queries remain available */
bool        race_get_trait_bool(RACE_DATA *race, ...);   /* existing */
bool        class_get_trait_bool(CLASS_DATA *clazz, ...); /* new */
```

#### Generic Trait Value Helpers

```c
TRAIT_VALUE *trait_values_alloc(void);                    /* Alloc + fill defaults */
void         trait_values_load_json(TRAIT_VALUE *tv, void *json_obj);
void *       trait_values_save_json(TRAIT_VALUE *tv);
```

These are used by all three layers (race, class, character) to avoid code duplication.

#### Integration with Rewards

`REWARD_TRAIT` (type 7) allows class rewards to grant traits at specific levels.
The trait is set on the class's `trait_values` array. For per-character trait grants
(one-time quest rewards), personal trait values are set directly on `PC_DATA.trait_values`.

#### Migration Path

Hardcoded class checks like:
```c
if (ch->pcdata->sub_class_cleric == CLASS_CLERIC_DRUID && is_in_nature(ch))
```
Become:
```c
if (ch_get_trait_bool(ch, "nature_affinity") && is_in_nature(ch))
```

This decouples behavior from class identity, allowing any class (or race, or personal
grant) to provide the same capability.

---

## 4. Phased Implementation

### Phase 0: Token-Affect Coupling Hardening ✅

**Goal:** Ensure bidirectional token<>affect lifecycle is robust before skills depend on it.
**Risk:** LOW — mostly audit and small fixes.
**Files:** `handler.c`, `mem.c`
**Completed:** 2026-02-13

- [x] Audit `affect_remove()` — ensure it NULLs `affect->token` back-link and removes affect from `token->affects` list
- [x] Audit `extract_token()` — ensure it removes all affects in `token->affects` from their targets
- [x] Audit `token_from_char()` / `token_from_obj()` / `token_from_room()` — ensure affect cleanup
- [x] Ensure `new_affect()` initializes `token` to NULL
- [x] Ensure `new_token_data()` initializes `affects` LLIST properly
- [x] Add `AFFECT_DATA.skill` field (as `SKILL_DATA *`, initially NULL — completed in Phase 5)

### Phase 1: Skill Data Backend ✅

**Goal:** Create `SKILL_DATA` struct, hash table, bootstrap from `skill_table[]`, JSON persistence.
**Risk:** LOW — additive, no behavioral changes yet.
**Files:** `merc.h`, `skill_data.c` (new), `skill_data.h` (new), `db.c`, `CMakeLists.txt`, `Makefile`
**Completed:** 2026-02-13

- [x] Define `SKILL_DATA`, `SKILL_CLASS_LEVEL` structs in `merc.h`
- [x] Define invocation method constants (`INVOC_*`) in `skill_data.h`
- [x] Define `MAX_SKILL_VALUES` (8) in `merc.h`
- [x] Create `skill_data.c` with:
  - Hash table (by name, case-insensitive, FNV-1a)
  - UID index (array for O(1) uid->skill lookup)
  - `skill_find()`, `skill_search()`, `skill_find_uid()`
  - `skill_first()`, `skill_count()`
  - `load_skill_data()` — reads `data/skills/*.json`, or bootstraps from `skill_table[]`
  - `save_skill_data()`, `save_all_skill_data()` — write to `data/skills/<name>.json`
  - `bootstrap_skills_from_table()` — one-time migration from `const.c` data
  - Complete spell function name<>pointer resolution table (152 entries from magic.h)
- [x] Create `skill_data.h` with public API declarations
- [x] In `db.c` `boot_db()`: call `load_skill_data()` after `load_races()`
- [x] Update `CMakeLists.txt` and `Makefile`
- [x] Verified clean build (zero warnings, zero errors)

### Phase 2: Compatibility Shim Layer ✅

**Goal:** Allow existing `gsn_*` / `skill_table[]` code to keep working while we migrate file by file.
**Risk:** LOW — shim is purely additive.
**Files:** `skill_data.c`, `merc.h`
**Completed:** 2026-02-13

- [x] Create `skill_resolve_gsn(const char *name)` — returns `int16_t uid` for a named skill (replaces `gsn_*` at call sites)
- [x] Create `skill_table_compat(int sn)` — returns `SKILL_DATA *` for legacy `sn` index (maps old indices to new UIDs)
- [x] Keep `skill_table[]` read-only during transition — it stays in `const.c` but is no longer the source of truth
- [x] `skill_lookup()` redirected to `skill_search()` returning UID for backward compat

### Phase 3: SPELL_FUN Signature Migration ✅

**Goal:** Change all spell function signatures to use `skill_t *` + invocation method.
**Risk:** HIGH — touches ~174 functions and ~332 references. Mechanical but vast.
**Files:** `merc.h`, all `magic_*.c`, `magic.c`, `magic2.c`, `music.c`, `effects.c`
**Completed:** 2026-02-13

- [x] Update `SPELL_FUN` typedef and `SPELL_FUNC`/`DECLARE_SPELL_FUN` macros
- [x] Update all ~174 spell function signatures (mechanical: replace `int sn` with `skill_t *skill`, add `int invocation`)
- [x] Update all spell function bodies that reference `sn` to use `skill->uid` or `skill` pointer
- [x] Update `obj_cast_spell()`, `do_cast()` and all invocation call sites to pass `INVOC_*`
- [x] Update `do_quaff`, `do_recite`, `do_brandish`, `do_zap` to pass appropriate `INVOC_*`

### Phase 4: Player Skill Storage Migration ✅

**Goal:** Move skill percentages from `learned[MAX_SKILL]` arrays to `SKILL_ENTRY.rating`.
**Risk:** MEDIUM — affects save/load, all skill checks.
**Files:** `merc.h`, `mem.c`, `skills.c`, `io/json/json_char.c`, `save.c`
**Completed:** 2026-02-13

- [x] Add `rating` and `mod_rating` to `SKILL_ENTRY`
- [x] Add `SKILL_DATA *skill_data` pointer to `SKILL_ENTRY`
- [x] Initialize new fields in `new_skill_entry()` (`mem.c`)
- [x] Update `skill_entry_insert()` to set `skill_data` pointer via UID lookup (`skill_find_uid(sn)`) 
- [x] Update character JSON save/load to populate `rating`/`mod_rating` from `learned[]`
- [x] Update dat load path to populate `rating`/`mod_rating` from `learned[]`
- [x] Add `VERSION_PLAYER_011` migration in `fix_character()` to sync existing characters
- [ ] Update `get_skill()` to use `SKILL_ENTRY.rating` instead of `learned[sn]` (deferred — too many read/write sites to switch atomically; rating fields shadow learned[] for now)
- [ ] Eventually remove `learned[MAX_SKILL]` and `mod_learned[MAX_SKILL]` from `PC_DATA` (Phase 9)

### Phase 5: AFFECT_DATA Migration ✅

**Goal:** Replace `affect.type` (int16_t sn) with `affect.skill` (SKILL_DATA *).
**Risk:** MEDIUM — affects all affect creation/checking.
**Files:** `merc.h`, `mem.c`, `handler.c`, all `magic_*.c`, `effects.c`, `fight.c`, `fight2.c`, `db.c`, `save.c`, `io/json/json_char.c`, `io/json/json_persist.c`, `script_*.c`, `act_*.c`, `update.c`, `weather.c`
**Completed:** 2026-02-13

- [x] Add `SKILL_DATA *skill` to `AFFECT_DATA` struct (before `type` field)
- [x] Initialize `af->skill = NULL` in `new_affect()`
- [x] Clear `af->skill = NULL` in `free_affect()`
- [x] Struct-copy in `affect_to_char()` / `affect_to_obj()` automatically copies `skill` pointer
- [x] Update all ~57 spell function `af.type = sn` sites (`magic_*.c`) to also set `af.skill = skill`
- [x] Update all ~4 `paf->type = sn` sites (`magic_cosmic.c`) to also set `paf->skill = skill`
- [x] Update all ~36 `af.type = gsn_*` sites (9 non-spell files) to also set `af.skill` via UID lookup
- [x] Update all ~4 `af.type = skill` sites (`script_*.c`) to also set `af.skill`
- [x] Update all load paths in `save.c` (6 sites), `db.c` (2 sites), `json_char.c` (2 sites), `json_persist.c` (2 sites) to populate `paf->skill` after setting `paf->type`
- [x] Add `#include "skill_data.h"` to 12 files that needed it
- [ ] Update affect comparison functions (`affect_strip`, `is_affected`, `affect_find`) to use `skill` pointer (deferred — READ sites can be migrated gradually)
- [ ] Eventually remove `int16_t type` from `AFFECT_DATA` (Phase 9)

### Phase 6: Class System

**Goal:** Data-driven classes with class types, multi-classing, custom data, and security hardening.
**Risk:** HIGH — core progression system; security-critical level check migration.
**Files:** `merc.h`, `class_data.c` (new), `class_data.h` (new), `skills.c`, `nanny.c`, `io/json/json_char.c`, `handler.c`, `fight.c`, `update.c`, `olc.c`, `act_wiz.c`, `account/penalty.h`, `CMakeLists.txt`, `Makefile`

#### 6a: Security Hardening (FIRST — before any level changes)

- [x] Audit and fix all `ch->level >= LEVEL_IMMORTAL` checks (4 in handler.c, update.c → `IS_IMMORTAL(ch)`)
- [x] Audit and fix all `ch->tot_level >= LEVEL_IMMORTAL` checks (7 in handler.c, act_info.c, act_wiz.c, special.c → `IS_IMMORTAL()`)
- [x] Audit and fix all `ch->level > LEVEL_HERO` checks (fight.c burgle → `IS_IMMORTAL(ch)`)
- [x] Audit and fix all `ch->level >= LEVEL_HERO` checks (act_wiz.c force gods → `IS_IMMORTAL()`)
- [x] Audit and fix `ch->tot_level < MAX_LEVEL - 1` staff gate (act_wiz.c → `!IS_STAFF(ch, STAFF_SUPREMACY)`)
- [x] Audit all `get_trust(ch)` usage for staff-gating vs. gameplay usage:
  - pfile_migrate.c: `get_trust(ch) < MAX_LEVEL` → `!IS_IMPLEMENTOR(ch)`
  - act_wiz.c advance: `level > get_trust(ch)` → `level > MAX_LEVEL && !IS_IMPLEMENTOR(ch)`
  - act_wiz.c trust: `level > get_trust(ch)` → `level > MAX_LEVEL && !IS_IMPLEMENTOR(ch)`
  - oedit.c: `i > get_trust(ch)` → `i > MAX_LEVEL` (object level cap, not authority)
  - script_*pcmds.c: `get_trust()` for NPC mob level — correct, left for Phase 9 rename
  - olc.c: already commented out
- [x] Align `IS_IMMORTAL` with src_20_dev: `!IS_NPC(ch) && pcdata->immortal != NULL` (removed `get_staff_rank` from check)
- [x] Align `IS_IMPLEMENTOR` with src_20_dev: `IS_IMMORTAL(ch) && get_staff_rank(ch) >= STAFF_IMPLEMENTOR`
- [x] Add `LEVEL_IMMORTAL` / `LEVEL_HERO` deprecation comments — constants only for NPC/object level comparison
- [x] Add `IS_HERO` / `IS_TRUSTED` deprecation comments — dead macros, never called
- [x] Verified: `obj->level >= LEVEL_IMMORTAL` (act_obj.c skulling) and `cmd->level >= LEVEL_IMMORTAL` (interp.c NPC block) are correct object/command level checks, left unchanged
- [x] Clean build: 176/176, zero errors, zero warnings

#### 6b: Class Data Backend

- [x] Define class type constants (`CLASS_TYPE_*`) in `class_data.h`
- [x] Define class flag constants (`CLASS_COMBATIVE`, `CLASS_NO_LEVEL`, etc.) in `class_data.h`
- [x] Define `CLASS_DATA` struct in `merc.h` with class types, body-type-aware display, callbacks
- [x] Define `CLASS_LEVEL` struct in `merc.h` with `json_t *custom_data` field
- [x] Define `CLASS_ENTER_FUN` / `CLASS_LEAVE_FUN` typedefs
- [x] Create `class_data.h` with public API declarations
- [x] Create `class_data.c` with:
  - Hash table (by name, case-insensitive, FNV-1a — same pattern as skill_data.c)
  - UID index (array for O(1) uid->class lookup)
  - `class_find()`, `class_find_exact()`, `class_find_uid()`
  - `class_first()`, `class_count()`
  - `load_class_data()` — reads `data/classes/*.json`, or bootstraps from `sub_class_table[]`
  - `save_class_data()`, `save_all_class_data()` — write to `data/classes/<name>.json`
  - `bootstrap_classes_from_table()` — one-time migration from const.c data
  - Enter/leave callback name<>pointer resolution table
- [x] Add `BONUS_FREE_LEVELS` (7) to `account/penalty.h` bonus types, increment `BONUS_MAX`
- [x] In `db.c` `boot_db()`: call `load_class_data()` after `load_skill_data()`
- [x] Update `CMakeLists.txt` and `Makefile`
- [x] Verified clean build (zero warnings, zero errors)
- [x] ~~Migrate display/who arrays from `SEX_MAX` to `BODY_TYPE_MAX` indexing~~ (already BODY_TYPE_MAX; superseded by titles)
- [x] ~~Add `class_display_ch()` / `class_who_ch()` convenience functions~~ (implemented with title awareness)
- [x] Add `CLASS_REWARD` struct and reward type constants to `merc.h`
- [x] Add `LLIST *rewards` to `CLASS_DATA`
- [x] Implement `new_class_reward()` / `free_class_reward()` memory management
- [x] Add reward JSON load/save in `class_data.c`
- [x] Add reward flag table (`reward_types[]`, `reward_flags[]`) for OLC/serialization
- [x] On bootstrap, populate rewards from `group_table[]` skill assignments
- [x] Add `REWARD_TRAIT` type (7) and bump `MAX_REWARD_TYPE` to 8
- [x] Add `REWARD_SCOPE_*` constants for cross-class reward availability
- [x] Add `scope` field to `CLASS_REWARD` struct
- [x] Add `reward_scopes[]` flag table for OLC/serialization
- [x] Add scope JSON load/save in class_data.c
- [x] Add `cross_class_scope` and `source_class` fields to `SKILL_ENTRY`
- [x] Initialize new SKILL_ENTRY fields in `new_skill_entry()` (`mem.c`)
- [x] Add `CLASS_TITLE` struct with keyword, display, who_name, is_default
- [x] Add `LLIST *titles` to `CLASS_DATA` for selectable class titles
- [x] Add `char *active_title` to `CLASS_LEVEL` for per-character title selection
- [x] Add `new_class_title()` / `free_class_title()` memory management
- [x] Add title lookup API: `class_find_title()`, `class_get_default_title()`, `class_title_display()`, `class_title_who()`
- [x] Add title JSON load/save in class_data.c
- [x] Bootstrap titles from legacy sub_class_table[] gendered names (default + male/female variants)
- [x] Update `class_display_ch()` / `class_who_ch()` to prefer titles over body-type arrays
- [x] Add `active_title` save/load in character JSON (`json_char.c`)
- [x] Add `trait_values` (TRAIT_VALUE array) to `CLASS_DATA` in `merc.h`
- [x] Add `trait_values` (TRAIT_VALUE array) to `PC_DATA` in `merc.h`
- [x] Generalize trait system: `trait_values_alloc()`, `trait_values_load_json()`, `trait_values_save_json()`
- [x] Add class trait API: `class_init_traits()`, `class_load/save_traits_json()`, `class_has/get_trait_*()`
- [x] Add personal trait API: `char_init_traits()`, `char_load/save_traits_json()`
- [x] Add unified character trait queries: `ch_has_trait()`, `ch_get_trait_bool/int/string()`
- [x] Add trait JSON load/save in class_data.c (`class_load_json` / `save_class_data`)
- [x] Integrate personal trait init in character load/creation (`mem.c`, `nanny.c`)
- [x] Integrate personal trait save in character JSON serialization (`json_char.c`)
- [x] Add class level save/load in character JSON (`json_char.c`: `class_levels` array)
- [x] Add `CLASS_DEFAULT` flag and `class_get_default()` for new-character auto-assignment
- [x] Add default class assignment on character creation (`nanny.c`)
- [x] Free personal trait_values in `free_pcdata()` (`mem.c`)

#### 6b2: Skill Group Backend

- [x] Define `SKILL_GROUP` struct (name + LLIST of skill name strings) — matches src_20_dev
- [x] Create `skill_group.c` / `skill_group.h` with:
  - Load/save groups from/to `data/skill_groups/` JSON files
  - Bootstrap from `group_table[MAX_GROUP]` in const.c
  - Lookup functions: `skill_group_find()`, `skill_group_search()`
- [x] Add `load_skill_groups()` to boot sequence in `db.c` (between skills and classes)
- [x] Update `CMakeLists.txt` and `Makefile`
- [x] Verified clean build (zero warnings, zero errors)
- [x] Update `CLASS_DATA.groups` from `LLIST of char *` to `LLIST of SKILL_GROUP *`
- [x] Port `version002__resolve_class_skill_levels()` from src_20_dev:
  iterates class→groups→skills to populate `SKILL_CLASS_LEVEL` entries on `SKILL_DATA`
  (now subsumed by `resolve_class_rewards_to_skill_levels()` in 6e)
- [x] Update `CMakeLists.txt` and `Makefile`

#### 6c: Character Integration

- [x] Add `LLIST *classes` and `CLASS_LEVEL *current_class` to `PC_DATA`
- [x] Initialize in `new_pcdata()` / cleanup in `free_pcdata()`
- [x] Memory management: `new_class_level()`, `free_class_level()` in `class_data.c`
- [x] Character class helpers: `get_current_class()`, `get_class_level()`, `has_class_level()`, `add_class_level()`, `remove_class_level()`, `insert_class_level()`, `is_current_class_combat()`
- [x] Update character JSON save: write `CLASS_LEVEL` list with class name, level, xp, custom_data
- [x] Update character JSON load: read class levels, resolve `CLASS_DATA *` pointers, populate `custom_data`
- [x] Update character dat load: bootstrap `CLASS_LEVEL` entries from legacy 16 class fields
- [x] Add `VERSION_PLAYER_012` migration in `fix_character()` to convert existing characters
- [x] Level overflow detection: on load, clamp levels to `max_level` and create `BONUS_FREE_LEVELS` for overflow
- [x] Update `tot_level` calculation to respect `CLASS_NO_LEVEL` flag
- [x] Migrate `PC_DATA.group_known` from `bool[MAX_GROUP]` to `LLIST *known_groups` of `SKILL_GROUP *` (dual storage — legacy bool array retained for Phase 9)

#### 6d: Gameplay Integration

- [x] Update `nanny.c` character creation to use new class system (deprecated class/subclass selection, uses `finalize_new_character()` with `class_get_default()`)
- [x] Port `do_setclass` command for active class switching (with enter/leave callbacks)
- [x] Port `do_clslist` command for listing available classes
- [x] Update XP gain (`gain_exp()` in `update.c`) to target `CLASS_LEVEL.xp`
- [x] Update leveling to use `CLASS_LEVEL.level` and `CLASS_DATA.max_level`
- [x] Update `score` display to show class levels
- [x] Update `who` list to use body-type-aware class display names
- [x] Add `freelevel` command to spend `BONUS_FREE_LEVELS` on a class
- [x] Ensure `SKILL_CLASS_LEVEL.class_name` resolves to `CLASS_DATA *` pointers after class load (`resolve_skill_class_pointers()` implemented)
- [x] Update `get_skill_level()` to check class_levels list via `CLASS_DATA *` instead of legacy index

#### 6e: Reward System Integration

- [x] Implement `resolve_class_rewards_to_skill_levels()`: iterate all class rewards,
      populate `SKILL_CLASS_LEVEL` entries on `SKILL_DATA` from `REWARD_SKILL`/`REWARD_GROUP` rewards
- [x] Implement `apply_class_rewards(ch, clazz, from_level, to_level)`: apply rewards in range
- [x] Implement `revoke_class_rewards(ch, clazz)`: revoke rewards with `REWARD_REVOKE_ON_LEAVE` flag
- [x] Hook into level-up path: call `apply_class_rewards()` on each level gain
- [x] Hook into class join: apply all rewards from 1 to current level
- [x] Hook into class leave: revoke flagged rewards
- [x] Integrate `REWARD_TOKEN`: create token on character using wnum from `data`
- [x] ~~Integrate `REWARD_SCRIPT`~~: covered by token scripts (tokens can have scripts attached)
- [x] Integrate `REWARD_TITLE`: update `CLASS_LEVEL` display/who overrides
- [x] Integrate `REWARD_CUSTOM`: write JSON value to `CLASS_LEVEL.custom_data`
- [x] Track one-time rewards via `"rewards_applied"` in `CLASS_LEVEL.custom_data`
- [x] Restrict non-cross-class skills from characters when not in a class (`is_skill_available_for_class()` in `get_skill()`)
- [x] Display reward grant messages only once per reward (not on rejoin)

#### 6f: `skill_entry` Runtime Parity Hardening (vs `src_20_dev`)

**Goal:** Finish migration from legacy `learned[]`/`mod_learned[]` runtime authority to `SKILL_ENTRY.rating`/`mod_rating`, including token-enabled paths.
**Risk:** MEDIUM — broad but mechanical call-site conversion.
**Files:** `skills.c`, `handler.c`, `magic.c`, `script_commands.c`, `script_*pcmds.c`, `act_wiz.c`, `save.c`, `io/json/json_char.c`

- [x] Make `SKILL_ENTRY.rating` / `mod_rating` authoritative in core runtime reads:
    - `skill_entry_rating()` / `skill_entry_mod()` now return entry fields for non-token entries (token entries still derive from token values)
    - `get_skill()` now resolves from `SKILL_ENTRY` for players (legacy fallback only if entry is missing)
    - `check_improve_show()` now improves `entry->rating` instead of direct `pcdata->learned[sn]`
- [ ] Finish runtime writes to update `SKILL_ENTRY` first everywhere (keep legacy array mirrors during transition):
    - [x] `do_practice()` standard-skill branch
    - [x] script grant/set/adjust skill commands (`script_commands.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`; `script_mpcmds.c` path is currently disabled/commented)
    - [x] immortal skill set paths in `act_wiz.c`
- [x] Align cast/practice consumption paths with `skill_entry_*` helpers where applicable:
    - `find_spell()` cast selection now uses `get_skill()` for known-skill gating instead of direct `pcdata->learned[sn]`
    - `do_cast()` success/failure roll now consumes the already-resolved skill value for the cast attempt
    - `can_practice()` / `had_skill()` practice gating now check `SKILL_ENTRY.rating` first, with legacy `learned[]` fallback
    - token spell behavior (`TOKVAL_SPELL_*`) and trigger hooks preserved
        - runtime cleanup pass completed for non-legacy direct reads:
            - `hunt.c` now consumes hunt chance via `get_skill()`
            - `get_weapon_skill()` now resolves player weapon skill via `get_skill()`
            - new-character default weapon skill initialization in `nanny.c` now sets `SKILL_ENTRY.rating` primary with legacy mirror
            - `get_skill()` fallback reads from `learned[]`/`mod_learned[]` removed (entry-first runtime authority)
            - script `+`/`-` skill mutation paths (`script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`) now require and consume `SKILL_ENTRY.rating` only
            - `group_add()`, `had_skill()`, and `update_skills()` no longer read `learned[]` as runtime authority (mirror writes retained)
        - remaining `learned[]` read checks are limited to persistence (`io/json/json_char.c`, `save.c`) and dead/commented legacy blocks (`#if 0` / disabled `script_mpcmds.c` path)
- [x] Save/load consistency pass (JSON path):
    - [x] JSON char persistence now writes `rating`/`mod_rating` from `SKILL_ENTRY` and loads with `rating`/`mod_rating` first (fallback to `learned`/`mod_learned` for backward compatibility)
    - non-JSON player save/load paths are deprecated in current deployment; no additional non-JSON migration work planned for 6f
    - keep backward-compatible `learned[]`/`mod_learned[]` sync until Phase 9 removal
    - maintain JSON safety-net behavior for unmapped legacy skills during migration window
- [ ] Validation:
    - [x] unit tests (`./sent -test:unit`) after 6f slices
    - [ ] manual spot-check: normal skill, token skill, scripted grant/revoke, class-switch availability gating

### Phase 7: OLC Editors

**Goal:** In-game editors for skills and classes.
**Risk:** LOW — editor-only.
**Files:** `editors/skills/skedit.c` (new), `editors/classes/clsedit.c` (new), `interp.c`

- [x] Create `skedit` — skill/spell editor
- [x] Create `clsedit` — class editor with reward management:
  - `reward <level> skill|group|title|bonus|token|script|custom|trait <name> [value]`
  - `rewardflags <level> <type> <flags>` — set REVOKE_ON_LEAVE, ONE_TIME, HIDDEN
  - `reward remove <level> <type> [name]` — remove a specific reward
  - `rewards` — list all rewards for this class
  - Validate skill/group names exist at save time
- [x] Spell function name<>pointer lookup table for skedit
- [x] Register commands in `interp.c`

### Phase 8: Song System Migration

**Goal:** Migrate `music_table[]` to data-driven system (same pattern as skills).
**Risk:** LOW — small scope (~20 references).
**Files:** `music.c`, `const.c`, `merc.h`

- [x] Create `SONG_DATA` struct (similar to `SKILL_DATA`)
- [x] Create song hash table and lookup functions
- [x] Bootstrap from `music_table[]`
- [x] Update `music.c` to use new system

### Phase 9: Cleanup

**Goal:** Remove all legacy globals and static tables.
**Risk:** MEDIUM — final cut.
**Files:** `merc.h`, `const.c`, `db.c`, all files with `gsn_` references

- [ ] Remove `gsn_*` declarations from `merc.h` (272 declarations)
- [ ] Remove `gsn_*` definitions from `db.c`
- [x] Remove `skill_from_sn()` compatibility shim from runtime and headers
- [ ] Remove `skill_table[]` from `const.c`
- [ ] Remove `pgsn` field from `SKILL_DATA`
- [ ] Remove `int16_t sn` from `SKILL_ENTRY`
- [ ] Remove `int16_t type` from `AFFECT_DATA`
- [ ] Remove `learned[MAX_SKILL]` and `mod_learned[MAX_SKILL]` from `PC_DATA`
- [ ] Remove `class_table[]` and `sub_class_table[]` from source
- [ ] Remove legacy 16 class fields from `PC_DATA` (`class_mage`, `sub_class_mage`, etc.)
- [ ] Remove `class_current` and `sub_class_current` from `PC_DATA`
- [ ] Remove `CLASS_MAGE`/`CLASS_CLERIC`/`CLASS_THIEF`/`CLASS_WARRIOR` index constants (keep `CLASS_TYPE_*`)
- [ ] Rename `get_trust()` to `get_mob_level()` and remove player authority semantics
- [ ] Remove `IS_HERO()` and `IS_TRUSTED()` macros (replace all call sites with `IS_STAFF()`)
- [ ] Remove `LEVEL_HERO` and `LEVEL_IMMORTAL` constants (or restrict to NPC-only contexts)
- [ ] Remove `music_table[]` from `const.c`
- [ ] Remove `group_table[MAX_GROUP]` from `const.c`
- [ ] Remove `bool group_known[MAX_GROUP]` from `PC_DATA` (replaced by `LLIST *group_known`)
- [x] Remove `SEX_EITHER` and `SEX_MAX` from `merc.h` (class display uses `body_type_t` / `BODY_TYPE_MAX`)

---

## 5. Migration Strategy

### 5.1. Skill Table Bootstrap

On first boot with no `data/skills/` directory:
1. Iterate `skill_table[]` entries (0 to MAX_SKILL)
2. For each non-NULL entry, create a `SKILL_DATA` with:
   - `uid` = original array index (preserves compatibility with saved `sn` values)
   - `name` = `skill_table[sn].name`
   - `spell_fun` = `skill_table[sn].spell_fun` (resolved via function name table)
   - `isspell` = `(spell_fun != NULL && spell_fun != spell_null)`
   - Class levels from `skill_level[MAX_CLASS]` -> `SKILL_CLASS_LEVEL` list
   - All other fields mapped directly
3. Write each as `data/skills/<name>.json`
4. All subsequent boots load from JSON

### 5.2. gsn_ Elimination Strategy

During transition (Phases 2-8), existing code using `gsn_backstab` is migrated to:

```c
/* Before: */
if (sn == gsn_backstab) { ... }

/* After: file-local cached pointer */
static SKILL_DATA *_backstab = NULL;
if (!_backstab) _backstab = skill_find("backstab");
if (skill == _backstab) { ... }
```

Or for hot paths, a helper macro:

```c
#define SKILL_CACHED(var, name) \
    static SKILL_DATA *var = NULL; \
    if (!var) var = skill_find(name);

/* Usage: */
SKILL_CACHED(sk_backstab, "backstab");
if (skill == sk_backstab) { ... }
```

This provides the same O(1) access as `gsn_*` globals after first call, without polluting global namespace.

### 5.3. Compatibility During Transition

- `skill_table[]` remains in `const.c` as read-only reference during migration
- `learned[MAX_SKILL]` arrays remain during migration, shadowed by `SKILL_ENTRY.rating`
- A `skill_sn_to_data(int sn)` compat function maps old indices to `SKILL_DATA *`
- `affect.type` and `affect.skill` coexist during migration; new code sets both

### 5.4. Character Save File Migration

On character load:
- If `"skills"` section uses old format (skill name -> `{learned, mod_learned}`):
  - Resolve skill name via `skill_find()`
  - Create `SKILL_ENTRY` with `skill` pointer and `rating` = learned value
  - Populate both old `learned[]` and new `SKILL_ENTRY.rating`
- On save: write new format with `SKILL_DATA` name as key, `rating` as value

---

## 6. JSON Skill File Format

```json
{
    "name": "acid blast",
    "uid": 1,
    "type": "spell",
    "display": "Acid Blast",
    "summary": "Hurls a bolt of acid at the target.",

    "spell_function": "spell_acid_blast",
    "target": "offensive",
    "minimum_position": "fighting",
    "min_mana": 20,
    "beats": 4,

    "noun_damage": "acid blast",
    "msg_off": "!Acid Blast!",
    "msg_obj": "",
    "msg_disp": "",

    "race": null,
    "flags": [],

    "class_levels": [
        { "class": "mage",    "level": 16, "rating": 4 },
        { "class": "cleric",  "level": 31, "rating": 4 },
        { "class": "thief",   "level": 31, "rating": 4 },
        { "class": "warrior", "level": 16, "rating": 4 }
    ],

    "inks": [[0, 0], [0, 0], [0, 0]],
    "values": {},
    "token_wnum": null
}
```

## 6b. JSON Class File Format

```json
{
    "name": "paladin",
    "uid": 3,
    "type": "warrior",
    "flags": ["combative"],
    "description": "Holy warriors who combine martial prowess with divine power.",

    "display": {
        "neutral": "Paladin",
        "male": "Paladin",
        "female": "Paladin",
        "other": "Paladin"
    },
    "who": {
        "neutral": "Pal",
        "male": "Pal",
        "female": "Pal",
        "other": "Pal"
    },

    "primary_stat": "strength",
    "max_level": 30,
    "hp_min": 11,
    "hp_max": 15,
    "gains_mana": false,
    "weapon": 0,

    "groups": ["warrior basics", "warrior default"],

    "rewards": [
        { "level": 1,  "type": "group",  "name": "warrior basics" },
        { "level": 1,  "type": "skill",  "name": "sword", "value": 1, "scope": "type" },
        { "level": 5,  "type": "skill",  "name": "second attack", "value": 3 },
        { "level": 10, "type": "title",  "data": {
            "display": { "neutral": "Champion" },
            "who": { "neutral": "Chm" }
        }},
        { "level": 20, "type": "script", "name": "warrior_level_20" },
        { "level": 25, "type": "trait",  "name": "bash_damage_multiplier", "value": 2 },
        { "level": 30, "type": "title",  "data": {
            "display": { "neutral": "Warlord" },
            "who": { "neutral": "Wrl" }
        }}
    ],

    "traits": {
        "bash_damage_multiplier": 1
    },

    "enter_function": null,
    "leave_function": null
}
```

### Player Class Save Format (in character JSON)

```json
{
    "classes": [
        {
            "class": "paladin",
            "level": 30,
            "xp": 45000,
            "custom_data": null
        },
        {
            "class": "ranger",
            "level": 15,
            "xp": 12000,
            "custom_data": {
                "companion_type": "wolf",
                "companion_name": "Fenris",
                "companion_level": 10
            }
        },
        {
            "class": "blacksmith",
            "level": 20,
            "xp": 8000,
            "custom_data": {
                "mastery_weapons": 3,
                "mastery_armor": 2
            }
        }
    ],
    "current_class": "paladin"
}
```

### 6c. Class Table Bootstrap

On first boot with no `data/classes/` directory:
1. Iterate `sub_class_table[]` entries (0 to MAX_SUB_CLASS)
2. For each entry, create a `CLASS_DATA` with:
   - `uid` = sequential (1-based)
   - `name` = `sub_class_table[i].name[0]`
   - `type` = parent class type from `sub_class_table[i].class`
   - `display[]` = from `sub_class_table[i].name[]` (neutral/male/female)
   - `who[]` = from `sub_class_table[i].who_name[]`
   - `flags` = `CLASS_COMBATIVE` for combat classes, `CLASS_REMORT_ONLY` for remort entries
   - `max_level` = `MAX_CLASS_LEVEL` (30)
   - `hp_min`/`hp_max`/`gains_mana` from `class_table[parent_class]`
   - `primary_stat` from `class_table[parent_class].attr_prime`
   - `groups` populated from `sub_class_table[i].default_group` + `class_table[parent_class].base_group`
3. Write each as `data/classes/<name>.json`
4. All subsequent boots load from JSON

Bootstrap also creates non-combat class stubs (crafting, gathering, explorer) with
placeholder definitions if those JSON files don't already exist.

---

## 7. File Inventory

### New Files
| File | Purpose |
|------|---------|
| `skill_data.c` | Skill hash table, load/save, lookup API |
| `skill_data.h` | Public skill API declarations |
| `skill_group.c` | Skill group hash table, load/save, lookup API |
| `skill_group.h` | Public skill group API declarations |
| `class_data.c` | Class hash table, load/save, lookup API |
| `class_data.h` | Public class API declarations |
| `editors/skills/skedit.c` | In-game skill/spell editor |
| `editors/classes/clsedit.c` | In-game class editor |

| `mem.c` (additions) | `new_class_level()`, `free_class_level()` memory management |

### Modified Files (Major)
| File | Changes |
|------|---------|
| `merc.h` | New structs, updated typedefs, invocation constants, removal of gsn_ externs |
| `const.c` | Eventually: removal of `skill_table[]`, `music_table[]` |
| `db.c` | Call `load_skills()`, `load_classes()` in boot sequence |
| `handler.c` | Affect/token coupling, skill lookup migration, level check audit |
| `magic.c` | `do_cast()`, `obj_cast_spell()`, spell dispatch |
| `all magic_*.c` | Spell function signatures |
| `skills.c` | Class helpers, skill practice/train, class-level skill resolution |
| `io/json/json_char.c` | Character skill/class save/load, class migration |
| `interp.c` | New editor commands, `do_setclass`, `do_clslist`, `do_freelevel` |
| `music.c` | Song system migration |
| `fight.c`, `fight2.c` | gsn_ references in combat, level check audit |
| `update.c` | Skill gain, level advancement, XP to `CLASS_LEVEL`, level check audit |
| `mem.c` | `new_class_level()`, `free_class_level()` |
| `nanny.c` | Character creation class selection |
| `account/penalty.h` | `BONUS_FREE_LEVELS` type |
| `olc.c` | Level check audit |
| `act_wiz.c` | Level check audit |
| `pfile_migrate.c` | Level check audit |

### Reference Files (src_20_dev)
| File | Reference For |
|------|--------------|
| `src_20_dev/merc.h` | Struct designs, field lists |
| `src_20_dev/skills.c` | Load/save patterns, class management |
| `src_20_dev/magic.c` | Spell dispatch patterns |
| `src_20_dev/handler.c` | Affect/token coupling |
| `src_20_dev/olc.c` | Editor patterns |

---

## 8. Risks and Mitigations

| Risk | Severity | Mitigation |
|------|----------|------------|
| Breaking existing characters | HIGH | Bootstrap preserves exact UIDs matching old sn indices; migration code in save/load |
| Breaking combat/magic | HIGH | SPELL_FUN signature change is mechanical; test every spell |
| Performance regression | LOW | Hash table lookup is O(1); cached file-local pointers match gsn_ perf |
| Merge conflicts | MEDIUM | Work in phases; each phase is a clean commit boundary |
| Save file corruption | HIGH | Backup before migration; dual-write old+new format during transition |
| **Security: level-based privilege escalation** | **CRITICAL** | Phase 6a audits ALL `ch->level >= LEVEL_IMMORTAL` and `get_trust()` for staff-gating; replace with `IS_STAFF()` / `IS_IMMORTAL()` |
| Level overflow on class changes | MEDIUM | Free levels bonus system preserves player investment; logged and auditable |
| Class custom data corruption | LOW | `custom_data` is opaque JSON; classes validate their own schema via enter/leave callbacks |

---

## 9. Success Criteria

- [ ] No `gsn_*` global variables in codebase
- [ ] No `skill_table[]` static array
- [ ] No `class_table[]` / `sub_class_table[]` static arrays
- [ ] Skills loaded from individual JSON files
- [ ] Classes loaded from individual JSON files (including crafting, gathering, explorer types)
- [ ] `SPELL_FUN` takes `skill_t *` and invocation method
- [ ] Skills editable in-game via `skedit`
- [ ] Classes editable in-game via `clsedit`
- [ ] All existing characters load correctly
- [ ] All existing spells/skills function identically
- [ ] Token-driven spells work alongside C-function spells
- [ ] No `ch->level` or `tot_level` used for staff authority checks (all use `get_staff_rank()`)
- [ ] Multi-class characters can hold arbitrarily many class levels without privilege escalation
- [ ] Class-specific custom data persists correctly through save/load cycles
- [ ] Level overflow from class max_level changes produces free level bonuses
- [ ] `CLASS_NO_LEVEL` flag excludes class levels from `tot_level` accumulation
- [ ] Class rewards define skill/group/title/bonus/token/script grants at specific levels
- [ ] `SKILL_CLASS_LEVEL` entries are derived from class rewards, not manually maintained
- [ ] Reward OLC syntax works in `clsedit` for all reward types
- [ ] `REWARD_REVOKE_ON_LEAVE` correctly revokes applicable rewards on class leave
- [ ] Token and script rewards execute correctly on level-up

---

## 10. Open Questions

1. **Song system timing:** Should songs be migrated alongside skills (Phase 1) or separately (Phase 8)? Songs are a small system (~20 refs) and could go either way. *Current plan: Phase 8 (separate).*
2. **Skill groups:** The current `group_type` / `group_known[]` system also needs migration. Should this be part of the skill phase or a separate effort? *Current plan: Part of class phase (Phase 6). Groups become a `SKILL_GROUP` struct (name + LLIST of skill names), persisted to `data/skill_groups/`. Classes reference groups via `REWARD_GROUP` entries in their reward list, making groups the primary bridge between classes and skills. PC_DATA.group_known migrates from `bool[MAX_GROUP]` to `LLIST *` of `SKILL_GROUP *`. The reward system (§3.10) subsumes the old approach of skill-centric `SKILL_CLASS_LEVEL` with rating per class — classes now own their progression.*
3. **Affect type field:** During Phase 5, affects need both `type` (legacy int) and `skill` (new pointer). How long should dual-field persist? *Current plan: Until Phase 9 cleanup.*
4. **`get_trust()` long-term:** Should `get_trust()` be eliminated entirely, or repurposed? Currently it returns `tot_level`, which conflates progression with authority. Options: (a) keep it for NPC mob-level comparisons only, (b) rename to `get_mob_level()` for clarity, (c) remove entirely. *Current plan: (b) rename in Phase 9.*
5. **Class prerequisites for multi-class:** The old sub_class_table had `prereq[2]` for remort classes. Should the new system encode prerequisites in JSON, or handle them via scripting? *Current plan: JSON field `prerequisites` as array of class name strings, checked at class-switch time.*
6. **Non-combat class level scaling:** Crafting/gathering/explorer classes may have different level caps and XP curves than combat classes. Should this be per-class or per-type? *Current plan: Per-class (`max_level` on each `CLASS_DATA`). Types are informational categories, not mechanical constraints.*
7. **Free level expiration:** Should free level bonuses from level overflow be permanent or time-limited? *Current plan: Permanent (expires_at = 0), since they represent earned progression.*
8. **Custom data validation:** Should classes define a JSON schema for their custom_data, or is it purely convention? *Current plan: Convention-based; enter/leave callbacks handle initialization and teardown. Schema validation can be added later if needed.*
9. **Reward revocation scope:** When a player leaves a class, which rewards should be revoked? Skills are typically permanent (you learned them), but bonuses and tokens might be class-specific. *Current plan: Per-reward `REWARD_REVOKE_ON_LEAVE` flag — defaults to false for skills/groups, true for bonuses/titles. Builders control this per-reward in clsedit.*
10. **Reward vs. SKILL_CLASS_LEVEL authority:** Should `SKILL_CLASS_LEVEL` entries on `SKILL_DATA` remain editable independently, or should they be purely derived from class rewards? *Current plan: Derived. `resolve_class_rewards_to_skill_levels()` rebuilds `SKILL_CLASS_LEVEL` from class rewards on boot. Direct `SKILL_CLASS_LEVEL` edits in `skedit` are a convenience that writes back to the class's reward list.*
11. **Title reward stacking:** When a class has multiple `REWARD_TITLE` entries at different levels, should only the highest-level title apply, or should they be queried per-level? *Current plan: Highest applicable title wins — iterate rewards from current level downward, first `REWARD_TITLE` found is used.*
