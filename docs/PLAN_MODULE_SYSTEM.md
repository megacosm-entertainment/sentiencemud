# Plan: Module & Namespace System

**Date:** February 8, 2026
**Status:** Early Design / Sketch

---

## Motivation

Sentience aspires to be a framework that others can build on. To make that viable, subsystems need clean boundaries and a consistent namespacing convention so that downstream projects can extend, replace, or coexist with core functionality without collisions.

Several subsystems already exhibit modular patterns — some well-factored, some ad-hoc. This document surveys the current state, draws parallels to the widevnum namespace model, and sketches out how a unified module/namespace system could work.

---

## The Widevnum Precedent

Widevnums already establish namespace-scoped identity for world data:

```
area_uid:local_vnum       (e.g., midgaard:3001)
```

This is the same fundamental pattern we want for code modules:

```
module_namespace:component  (e.g., sentience_magic:cold)
```

Both cases solve the same problem: **globally unique identity through hierarchical scoping**. Widevnums prove this pattern works for data; extending it to code modules is the logical next step.

A downstream project's world data (`my_mud:3001`) won't collide with core data (`midgaard:3001`) — and likewise `my_mud_magic:frost` won't collide with `sentience_magic:cold`.

---

## Current State: Subsystem Survey

### 1. Magic System — `sentience_magic`

**Current structure:** 25 `magic_*.c` files, each implementing spells for an element/school. Spells are registered in `skill_table[]` in `const.c` with `SPELL_FUN` function pointers and dispatched through `do_cast` in `magic.c`.

**What's modular already:**
- Each `magic_*.c` file is self-contained: it implements spell functions and nothing else
- Spells are declared in `magic.h` via `DECLARE_SPELL_FUN()` macros
- Spell dispatch goes through function pointers in the skill table

**What's not modular:**
- All spell declarations are hardcoded in `magic.h`
- All registrations are hardcoded in `const.c` `skill_table[]`
- Adding a new school means editing core files, not just dropping in a new `.c`
- No way for a downstream project to add spells without modifying core source

**Namespace mapping:**
| Current | Namespaced |
|---------|-----------|
| `magic_cold.c` → `spell_chill_touch` | `sentience_magic:cold` → `sentience_magic:spell_chill_touch` |
| `magic_fire.c` → `spell_fireball` | `sentience_magic:fire` → `sentience_magic:spell_fireball` |

**Preservation note:** The current magic system should remain available as a loadable module (e.g., `sentience_magic_legacy`) even if a new magic system replaces it. This means the module system must support swapping implementations: a game could load `sentience_magic_legacy` OR `my_mud_magic_v2`, but not necessarily both (unless designed to coexist).

### 2. Authentication — `sentience_auth`

**Current structure:** `account/auth.c` provides a unified API. `auth_sodium.c` implements Argon2id. `verify_password()` dispatches based on `passwd_version`. OTP/MFA handled in `account/otp.c`.

**What's modular already:**
- Clean separation between auth API (`auth.c`) and backends (`auth_sodium.c`)
- Version-based dispatch in `verify_password()` already selects the right hash algorithm
- MFA is a separate concern in `otp.c`
- Already lives in its own subdirectory (`account/`)

**What's not modular:**
- Backend selection is hardcoded if/else in `verify_password()`
- No way to register a new auth backend without modifying `auth.c`

**Namespace mapping:**
| Current | Namespaced |
|---------|-----------|
| `auth_sodium.c` | `sentience_auth:argon2id` |
| SHA-256 path in `auth.c` | `sentience_auth:sha256_legacy` |
| `otp.c` | `sentience_auth:totp` |

**This is the closest subsystem to being module-ready.** The refactor is small: replace the if/else dispatch with a registered handler lookup.

### 3. I/O / Persistence — `sentience_io`

**Current structure:** JSON loading in `src/io/json/`, legacy `.are` parsing in `db.c`/`db2.c`/`olc_save.c`. Format detection is hardcoded: check for `.json`, fall back to `.are`/`.dat`.

**What's modular already:**
- JSON handlers are already in their own directory (`src/io/json/`)
- `PLAN_IO_REFACTOR.md` already envisions `src/io/legacy/` for old format handlers
- Format fallback chains exist (JSON → .dat → .are)

**What's not modular:**
- Format dispatch is hardcoded if/else in `db.c` and `db2.c`
- No registration mechanism — adding a new format means editing `db.c`
- Save-side dispatch in `olc_save.c` has hardcoded format exceptions

**Namespace mapping:**
| Current | Namespaced |
|---------|-----------|
| `json_area.c` | `sentience_io:json_area` |
| `.are` loading in `db.c` | `sentience_io:are_legacy` |
| `.dat` loading in `db2.c` | `sentience_io:dat_legacy` |

### 4. Connection / Protocol — `sentience_net`

**Current structure:** `connection.c` with `connection_tcp.c`, `connection_tls.c`, `connection_websocket.c`. Protocol negotiation in `protocol_layer.c` with `protocol_telnet.c` and `protocol_websocket.c`.

**What's modular already:**
- Each transport is its own file
- Protocol layer already uses a dispatch pattern

**This subsystem maps cleanly to modules:** `sentience_net:tcp`, `sentience_net:tls`, `sentience_net:websocket`.

---

## Proposed Architecture

### Core Concepts

```
┌─────────────────────────────────────────────────┐
│                 Module Registry                  │
│                                                  │
│  Namespace       Component       vtable          │
│  ─────────       ─────────       ──────          │
│  sentience_auth  argon2id        auth_ops { }    │
│  sentience_auth  sha256_legacy   auth_ops { }    │
│  sentience_io    json_area       io_ops { }      │
│  sentience_io    are_legacy      io_ops { }      │
│  sentience_magic cold            magic_school { } │
│  my_mud_magic    psychic         magic_school { } │
└─────────────────────────────────────────────────┘
```

### Module Identity

A module is identified by `namespace:component`:
- **Namespace** groups related modules (e.g., `sentience_magic`, `my_mud_auth`)
- **Component** identifies a specific implementation within that namespace
- The delimiter (`:`) matches the widevnum convention

### Module Contract (vtable)

Each module type defines a contract — a struct of function pointers that any implementation must provide:

```c
/* Example: auth backend contract */
typedef struct auth_module_ops {
    const char *name;
    bool (*verify)(const char *stored, const char *input);
    char *(*hash)(const char *input);
    int  (*version)(void);  /* password version number */
} auth_module_ops_t;

/* Example: I/O format contract */
typedef struct io_format_ops {
    const char *name;
    const char *extension;    /* ".json", ".are", ".dat" */
    int priority;             /* dispatch order */
    bool (*can_load)(const char *path);
    bool (*load)(const char *path, void *dest);
    bool (*save)(const char *path, const void *src);
} io_format_ops_t;

/* Example: magic school contract */
typedef struct magic_school_ops {
    const char *name;
    int spell_count;
    spell_entry_t *spells;     /* array of spell definitions */
    bool (*init)(void);        /* called at boot to register spells */
    void (*shutdown)(void);
} magic_school_ops_t;
```

### Module Registration

Modules self-register at boot time. Two possible approaches:

**Option A: Explicit registration in boot sequence**
```c
/* In db.c boot_db() or a new module_init() */
module_register("sentience_auth:argon2id",  &argon2id_ops);
module_register("sentience_auth:sha256",    &sha256_ops);
module_register("sentience_io:json_area",   &json_area_ops);
module_register("sentience_magic:cold",     &magic_cold_ops);
```

**Option B: Constructor-based auto-registration (GCC/Clang)**
```c
/* In magic_cold.c */
__attribute__((constructor))
static void magic_cold_register(void) {
    module_register("sentience_magic:cold", &magic_cold_ops);
}
```

Option A is simpler and more portable. Option B is more "drop-in" but relies on compiler extensions.

### Module Dispatch

Replace hardcoded if/else chains with registry lookups:

```c
/* Current (auth) */
if (version == PASSWD_VERSION_ARGON2ID)
    result = sodium_verify(stored, input);
else if (version == PASSWD_VERSION_SHA256)
    result = strcmp(sha256_crypt(input), stored) == 0;

/* Proposed */
auth_module_ops_t *mod = module_find_auth(version);
result = mod->verify(stored, input);
```

```c
/* Current (I/O) */
if (access(json_path, F_OK) == 0)
    json_area_load(json_path);
else
    read_area_new(fp);

/* Proposed */
io_format_ops_t *fmt = module_find_format(base_path);  /* tries by priority */
fmt->load(path, &area);
```

---

## Data Serialization: Who Owns the Format?

A key architectural question: if magic systems are swappable modules, how does the I/O layer know how to save and load their data?

The current system hardcodes this knowledge everywhere — `json_char.c` knows exactly what fields a character's spells have, `const.c` knows every spell's properties, `save.c` knows the player file layout. If modules are pluggable, this breaks down: the JSON saver can't anticipate the data shape of `my_mud_magic:psychic`.

### Approach: Modules Own Their Serialization

The cleanest pattern for C: each module provides its own `to_json()` and `from_json()` as part of its vtable. The I/O layer provides *infrastructure* (file handles, JSON helpers, save scheduling, path resolution) but delegates the actual data format to the module that owns the data.

```c
/* Extended magic school contract */
typedef struct magic_school_ops {
    const char *name;
    int spell_count;
    spell_entry_t *spells;
    bool (*init)(void);
    void (*shutdown)(void);

    /* Serialization — the module defines its own format */
    json_t *(*to_json)(const void *module_data);
    bool   (*from_json)(const json_t *root, void *module_data);
    int     data_version;   /* for format migration */
} magic_school_ops_t;
```

### How It Works in Practice

**Saving a character:**
```c
/* Core save logic (io layer) */
json_t *save_character(CHAR_DATA *ch) {
    json_t *root = json_object();

    /* Core fields — the I/O layer owns these */
    json_object_set_new(root, "name", json_string(ch->name));
    json_object_set_new(root, "level", json_integer(ch->level));

    /* Module data — each registered module serializes its own section */
    json_t *modules = json_object();
    for (each registered module that has data on this character) {
        json_t *section = mod->ops->to_json(ch->module_data[mod->id]);
        json_object_set_new(modules, mod->qualified_name, section);
    }
    json_object_set_new(root, "modules", modules);

    return root;
}
```

**Resulting JSON:**
```json
{
    "name": "Gandalf",
    "level": 50,
    "modules": {
        "sentience_magic:cold": {
            "version": 1,
            "learned_spells": ["chill_touch", "ice_storm", "frost_breath"],
            "school_mastery": 75
        },
        "sentience_magic:fire": {
            "version": 1,
            "learned_spells": ["fireball", "burning_hands"],
            "school_mastery": 30
        },
        "my_mud_magic:psychic": {
            "version": 2,
            "psi_points": 140,
            "disciplines": ["telepathy", "telekinesis"]
        }
    }
}
```

**Loading:**
```c
/* Core load logic */
void load_character(CHAR_DATA *ch, json_t *root) {
    /* Core fields */
    ch->name = str_dup(json_get_string(root, "name"));
    ch->level = json_get_int(root, "level");

    /* Module data — look up each section's module by name */
    json_t *modules = json_object_get(root, "modules");
    const char *key;
    json_t *section;
    json_object_foreach(modules, key, section) {
        module_t *mod = module_find(key);
        if (mod && mod->ops->from_json) {
            mod->ops->from_json(section, &ch->module_data[mod->id]);
        } else {
            /* Unknown module — preserve data for round-tripping */
            store_unknown_module_data(ch, key, section);
        }
    }
}
```

### Key Design Decisions

**1. Unknown module data is preserved, not discarded.**
If a player file contains data from `my_mud_magic:psychic` but that module isn't loaded, the data is stored opaquely and written back on save. This prevents data loss when switching module configurations or when a module is temporarily removed.

**2. Each module section is versioned.**
The `data_version` field lets a module migrate its own data format over time. When `from_json()` sees `"version": 1` but the current module is at version 2, it can upgrade in place.

**3. Core character data stays in the I/O layer.**
Fields like name, level, stats, inventory — the structural skeleton of a character — remain in the core save/load code. Modules only own their *additional* data. This keeps the system practical: you don't need a module loaded just to read a character's name.

**4. The I/O format modules and data-owning modules are separate concerns.**
`sentience_io:json_area` handles *how* data is persisted (JSON files). `sentience_magic:cold` handles *what* its data looks like. They don't need to know about each other — the I/O layer calls `to_json()` on the magic module and writes whatever it gets back.

### What This Means for Legacy Magic Preservation

The current magic system's data (learned spells, skill percentages, groups) would be serialized by the `sentience_magic_legacy` module. If you later replace it with a new magic system, the legacy module's data remains in player files under `"sentience_magic_legacy:*"` keys. If you ever re-enable the legacy module, the data is still there.

This is the same principle that makes the `.json` / `.are` fallback chain work today — old format data isn't destroyed, it's just read by a different handler.

---

## Migration Strategy

### Phase 0: Registry Infrastructure
- Implement the core module registry (`module.c` / `module.h`)
- Simple hash table mapping `"namespace:component"` → `void *ops`
- Type-safe wrapper functions per module category

### Phase 1: Authentication (Lowest Risk)
- Already well-factored with clear backend separation
- Wrap existing `auth_sodium.c` and SHA-256 code as registered modules
- Replace if/else dispatch in `verify_password()` with registry lookup
- Validates the pattern with minimal disruption

### Phase 2: I/O Formats
- Aligns with existing `PLAN_IO_REFACTOR.md`
- Wrap JSON and legacy loaders as registered format handlers
- Replace format-detection chains in `db.c`/`db2.c` with priority-ordered registry scan

### Phase 3: Magic System
- Largest scope — 25 files, hundreds of spells in `skill_table[]`
- Wrap each `magic_*.c` as a module that registers its spells at boot
- Move spell declarations out of `magic.h` into per-school headers
- Current system preserved as `sentience_magic_legacy` module
- New magic systems can coexist or replace it entirely

### Phase 4: Protocol / Networking
- Wrap transport and protocol handlers as modules
- Enables custom protocols without modifying core connection code

---

## Open Questions

1. **Configuration:** How does a game operator choose which modules to load? A `modules.conf` file? A JSON config in `data/system/`? Compile-time selection?

2. **Dependencies:** Can modules depend on other modules? If `sentience_auth:argon2id` depends on libsodium, how is that expressed?

3. **Hot-swapping:** Is runtime module loading/unloading a goal, or is boot-time registration sufficient? (Boot-time is far simpler and likely sufficient for a MUD.)

4. **Naming convention:** Should the core namespace be `sentience` or something shorter? Should downstream projects be required to use a unique prefix?

5. **Interaction with widevnums:** Widevnums namespace world data; modules namespace code. Should they share registry infrastructure, or are they separate systems that happen to use the same naming convention?

6. **Magic system replacement:** What does the "new" magic system look like? The module system provides the *mechanism* for swapping, but the design of the replacement is a separate concern.

---

## Relationship to Other Plans

| Document | Relationship |
|----------|-------------|
| `PLAN_IO_REFACTOR.md` | Phase 2 builds directly on this — the IO refactor's directory structure becomes the home for IO format modules |
| `PLAN_SKILL_REFACTOR.md` | Magic module system intersects with skill system changes |
| `widevnums/WIDEVNUM_MASTER_PLAN.md` | Shared namespace philosophy; module registry could reuse or mirror widevnum's area UID patterns |
| `PLAN_crypto_modernization.md` | Auth modules formalize what crypto modernization started |
