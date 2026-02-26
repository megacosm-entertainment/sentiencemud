# Effects System Redesign Plan

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Overview

The current "affects" system is a mature DikuMUD-derived implementation that has served
well but carries accumulated technical debt. This plan outlines a path to a cleaner
"effects" system with proper source attribution, room support, unified flags, and
full token integration.

### Goals

1. Rename "affects" to the grammatically correct "effects" throughout the codebase
2. Unify AFF_/AFF2_ dual bitvectors into a single flagset_t
3. Add proper source attribution (who/what created the effect, when, why)
4. Support effects on rooms, not just characters and objects
5. Complete token-effect integration (persistence, auto-extraction, triggers)
6. Remove legacy `type` field (int16_t skill number) in favor of `SKILL_DATA *skill`

### Non-Goals

- Casting system rework (separate plan: PLAN_CASTING_SYSTEM_REWORK.md)
- Full combat rebalancing (separate plan: PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md)

---

## Current State Analysis

### AFFECT_DATA Structure (merc.h:2535-2553)

```c
struct affect_data
{
    AFFECT_DATA *   next;
    bool            valid;
    int16_t         group;          // AFFGROUP_* category
    int16_t         where;          // TO_AFFECTS, TO_IMMUNE, TO_RESIST, etc.
    SKILL_DATA *    skill;          // Skill/spell that created this (new)
    int16_t         type;           // Legacy: skill number (Phase 9 removal)
    int16_t         level;          // Caster/effect level
    int16_t         duration;       // >0 timed, 0 expired, <0 permanent
    int16_t         location;       // APPLY_* constant
    int16_t         modifier;       // Amount of modification
    long            bitvector;      // AFF_ flags
    long            bitvector2;     // AFF2_ flags
    int16_t         random;
    char *          custom_name;    // Custom display name
    int16_t         slot;           // Wear location
    TOKEN_DATA *    token;          // Source token (bidirectional link)
};
```

### Flag Counts

- 29 AFF_ flags (primary: blind, invisible, sanctuary, haste, etc.)
- 26 AFF2_ flags (secondary: silence, evasion, barriers, etc.)
- Stored as two `long` bitvectors, split arbitrarily by capacity limits
- Characters carry both current (`affected_by[2]`) and permanent (`affected_by_perm[2]`)

### Where Targets

| Constant | Value | Target | Status |
|----------|-------|--------|--------|
| TO_AFFECTS | 0 | Character AFF_ flags | Implemented |
| TO_OBJECT | 1 | Object extra[0] | Implemented |
| TO_IMMUNE | 2 | Character immunity | Implemented |
| TO_RESIST | 3 | Character resistance | Implemented |
| TO_VULN | 4 | Character vulnerability | Implemented |
| TO_WEAPON | 5 | Weapon flags | Implemented |
| TO_ROOM | 6 | Room effects | **Defined but unimplemented** |
| TO_AFFECTS2 | 7 | Character AFF2_ flags | Implemented |
| TO_OBJECT2 | 8 | Object extra[1] | Implemented |
| TO_OBJECT3 | 9 | Object extra[2] | Implemented |
| TO_OBJECT4 | 10 | Object extra[3] | Implemented |
| TO_CATALYST_DORMANT | 20 | Catalyst (dormant) | Implemented |
| TO_CATALYST_ACTIVE | 21 | Catalyst (active) | Implemented |

### Core Functions (handler.c)

| Function | Purpose | Notes |
|----------|---------|-------|
| affect_to_char() | Add affect to character | Creates copy, links token, applies mods |
| affect_to_obj() | Add affect to object | Also applies to wearer if equipped |
| affect_remove() | Remove from character | Unapplies mods, unlinks token, recalcs |
| affect_remove_obj() | Remove from object | Handles extra flags cleanup |
| affect_modify() | Apply/unapply stat changes | Stats, flags, immunities, skills |
| affect_fix_char() | Recalculate all flags | Full rebuild from permanent + active |
| affect_strip() | Remove all of a skill | By skill number |
| affect_strip_name() | Remove by custom name | Token-based effects |
| affect_join() | Stack same-skill affects | Averages level, sums duration+modifier |
| affect_join_full() | Stack with full matching | Also matches location and bitvectors |
| affect_find() | Find affect by skill | Returns first match |
| affect_check() | Validate affect flags | Equipment flag verification |

### Persistence (save.c)

- **Write format**: Compact numeric (`Afk` key with 10 numeric fields)
- **Read format**: Multiple legacy formats (AFK, AFN, AFI, AFR, AFV, AF2)
- **Token linkage**: NOT persisted across reboots
- **Custom names**: Saved via AFN format

### Update Loop (update.c)

- Character affects: duration countdown, 20% level fade per tick, expiry message
- Object affects: same pattern, with room echo for uncarried objects
- **No auto-extraction**: When a token's last affect expires, token is not cleaned up
  (2.0 has this, 1.0 does not)

---

## What 2.0 Dev Implemented (Reference: /sentience/src_20_dev/)

### 2.0 AFFECT_DATA (merc.h:2759-2777)

```c
struct affect_data
{
    AFFECT_DATA *next;
    bool valid;
    int16_t group;
    int16_t where;
    int16_t catalyst_type;      // Only used by catalysts
    SKILL_DATA *skill;          // No legacy 'type' field - clean pointer only
    TOKEN_DATA *token;
    int16_t level;
    int16_t duration;
    int16_t location;
    int16_t modifier;
    long bitvector;
    long bitvector2;
    int16_t random;
    char *custom_name;
    int16_t slot;
};
```

Key differences from 1.0:
- **No legacy `type` field** - uses only `SKILL_DATA *skill`
- `catalyst_type` field for catalyst specialization
- Token pointer positioned earlier in struct (minor)

### Backport Status

| Feature | 2.0 | 1.0 | Action Needed |
|---------|-----|-----|---------------|
| Token-affect coupling | Complete | **Ported** | None |
| `SKILL_DATA *skill` pointer | Native (no legacy) | Dual with `type` | Remove `type` |
| Affect groups (AFFGROUP_*) | 12 groups | **Ported** | None |
| `custom_name` field | Complete | **Ported** | None |
| `slot` field | Complete | **Ported** | None |
| Auto-extraction on expiry | update.c complete | **Not ported** | Port from 2.0 |
| Token-affect persistence | Partial (reconstructed) | **Not implemented** | Design + implement |
| Level fading (20%/tick) | Complete | **Ported** | None |
| Token-aware stacking | affect_join checks token | Needs verification | Verify/update |
| Catalyst system | Complete | **Ported** | None |
| TRIG_TOKEN_GIVEN/REMOVED | Complete | **Not ported** | Port from 2.0 |

### 2.0 Auto-Extraction Pattern (update.c:2035-2068)

```c
// When affect expires (duration == 0):
TOKEN_DATA *token = paf->token;
affect_remove(ch, paf);

if (IS_VALID(token)) {
    if (list_size(token->affects) < 1)
        extract_token(token);
}
```

This is the critical missing piece: tokens that exist solely to provide affects should
clean themselves up when their last affect expires.

### 2.0 Save/Load Pattern (save.c)

2.0 saves affects with string-based field names:
```
#AFFECT
Name custom_name~
Skill skill_name~
Group group_flag~
Where apply_type~
Level 10
Duration 5
Modifier 3
Location apply_flag~
BitVector flags
BitVector2 flags
Slot wear_loc~
#-AFFECT
```

Token linkage is NOT saved directly. Instead, tokens are saved with their affects,
and the linkage is reconstructed at load time by iterating token->affects and
inserting them into ch->affected.

---

## Why Not Just Flagsets?

The original version of this plan proposed migrating AFF_/AFF2_ bitvectors to
unified flagset_t (per CODE_MIGRATION_PATTERNS.md). This solves the capacity limit
and the arbitrary AFF_/AFF2_ split, but fundamentally **doesn't change the
architecture**. You'd still have 361 hardcoded flag checks scattered across 33+
files, just with string lookups instead of bitwise ops:

```c
// Before:
if (IS_AFFECTED2(ch, AFF2_FIRE_BARRIER) && number_percent() < 10)
    fire_retaliation_damage();

// After flagset migration — same problem, different syntax:
if (flagset_is_set(&ch->effect_flags, "fire_barrier") && number_percent() < 10)
    fire_retaliation_damage();
```

The flag doesn't describe what fire_barrier *does*. The 10% proc chance is in
fight.c. The spell that applies it is in magic_fire.c. The display name is in
bit.c. There's no single source of truth.

**The trait system migration proved a better path exists.** When `IS_VAMPIRE(ch)`
was replaced with `ch_has_trait(ch, "sunlight_vulnerability")`, the behavioral
*meaning* moved into data. We should do the same for effects.

### What Flags Actually Do

Analyzing all 361 IS_AFFECTED/IS_AFFECTED2 callsites, effect flags serve three
distinct roles:

**1. Binary State Checks (high frequency, hot paths)**
These are genuine boolean conditions checked frequently in combat, movement, and
command processing:
```
blind, invisible, sneak, hide, flying, sleeping, charmed, pass_door,
haste, slow, sanctuary, detect_invis, detect_hidden, poison, plague
```
These are checked hundreds of times in tight loops. They *are* flags — fast
boolean tests are appropriate here.

**2. Behavior Modifiers (medium frequency, parameterized)**
These carry implicit parameters baked into C code:
```
fire_barrier     → 10% proc, fire damage retaliation (fight.c:1583)
frost_barrier    → 10% proc, cold damage retaliation (fight.c:1613)
electrical_barrier → 10% proc, shock damage retaliation (fight.c:1553)
sanctuary        → flat 50% damage reduction (fight.c)
stone_skin       → AC modifier (magic_earth.c)
healing_aura     → periodic heal amount (fight.c:162)
evasion          → dodge bonus percentage (fight.c:1323)
warcry           → 150% combat chance multiplier (fight.c:4941)
regeneration     → HP regen rate (fight.c:177)
```
These should be **effect definitions** with parameters, not flags with hardcoded
behavior.

**3. Visual/Status Indicators (low frequency)**
These exist primarily for display or detection:
```
faerie_fire, infrared, detect_evil, detect_good, detect_magic,
deathsight, dark_shroud, light_shroud, cloak_of_guile, improved_invis
```
Many of these could be handled by the aura system or effect metadata rather
than consuming flag slots.

---

## Proposed Architecture: Effect Definitions

Following the trait system pattern, effect behaviors should be defined in data
and looked up at runtime, not hardcoded as flag checks.

### Design Principles

1. **Follow the trait system pattern** — JSON definitions, indexed array lookup,
   layered data, OLC-editable
2. **Effects are self-describing** — an effect definition carries its behaviors,
   not just a name
3. **Retain fast boolean state** — characters still have a bitfield/flagset for
   hot-path state checks (blind, flying, etc.), but these are *derived from*
   effect definitions, not the primary data model
4. **Effects work on characters, objects, and rooms** — same EFFECT_DATA struct,
   different target
5. **Source attribution is built in** — every active effect knows what created it

### Core Data Structures

#### EFFECT_DEF (Effect Definition) — loaded from JSON at boot

Analogous to TRAIT_DEF. Defines what an effect *is* and what it *does*.

#### Behavior System

Instead of a single exclusive type per effect, each definition carries an array
of **behaviors**. This allows composite effects (sanctuary = state flag + damage
reduction) without special-casing. Each behavior is a tagged union — it only
carries the fields relevant to its type:

```c
#define MAX_EFFECT_BEHAVIORS  4     // Max behaviors per definition

typedef enum {
    BEHAVIOR_STATE,             // Sets a boolean state flag (blind, flying, hidden)
    BEHAVIOR_MODIFIER,          // Stat/attribute modifier (+5 STR, +10 HR)
    BEHAVIOR_PERIODIC,          // Tick-based action (regen, poison damage)
    BEHAVIOR_REACTIVE,          // Triggered on event (barrier retaliation)
    BEHAVIOR_PASSIVE            // Passive presence (detect_invis, infrared)
} effect_behavior_type_t;

typedef struct effect_behavior {
    effect_behavior_type_t type;
    char *          label;          // Optional label for scripting (e.g., "damage_reduction")
                                    // Falls back to type name if NULL

    union {
        struct {
            char *  state_flag;     // Boolean flag to set (e.g., "blind", "sanctuary")
        } state;

        struct {
            int     apply_location; // APPLY_STR, APPLY_DEX, etc.
            int     default_modifier; // Default value (overridable per-instance)
        } modifier;

        struct {
            int     action;         // What happens each tick (ACTION_DAMAGE, ACTION_HEAL, etc.)
            int     default_param;  // Default parameter (overridable per-instance)
        } periodic;

        struct {
            int     trigger_event;  // What triggers it (TRIGGER_MELEE_HIT_RECEIVED, etc.)
            int     trigger_chance; // Percentage chance to fire (0-100)
            int     trigger_action; // What happens (ACTION_DAMAGE, ACTION_DAMAGE_REDUCTION, etc.)
            int     default_param;  // Default parameter (overridable per-instance)
            int     dam_type;       // Damage type for damage-based triggers
        } reactive;
    };
} EFFECT_BEHAVIOR;
```

The engine dispatches on behavior type — each effect can contribute multiple
behaviors, processed independently:

```c
for (int i = 0; i < def->behavior_count; i++) {
    if (!(eff->behavior_mask & (1 << i)))
        continue;   // Masked off for this instance

    switch (def->behaviors[i].type) {
        case BEHAVIOR_STATE:    apply_state(target, &def->behaviors[i].state);        break;
        case BEHAVIOR_MODIFIER: apply_modifier(target, eff, i);                       break;
        case BEHAVIOR_PERIODIC: process_periodic(target, eff, i);                     break;
        case BEHAVIOR_REACTIVE: check_reactive(target, eff, i, event);                break;
        case BEHAVIOR_PASSIVE:  /* no-op, presence-only */                            break;
    }
}
```

#### Instance-Level Behavior Control

At apply time, scripts and spells can control which behaviors are active on a
specific instance via a bitmask, and override the default modifier for any
behavior by label:

**Behavior mask** — selectively enable/disable behaviors:

```c
// In EFFECT_DATA:
uint8_t  behavior_mask;             // Bit per behavior; default = all bits set
int16_t  modifiers[MAX_EFFECT_BEHAVIORS]; // Per-behavior runtime values (0 = use def default)
```

Script syntax uses labels (not bit positions) for clarity:

```
# Apply sanctuary without the damage reduction:
addeffect $TARGET sanctuary -skip damage_reduction

# Apply only the state flag behavior:
addeffect $TARGET sanctuary -only state

# Override a specific behavior's modifier by label:
addeffect $TARGET sanctuary damage_reduction=75 level=$LEVEL duration=12
```

**Runtime modification** — scripts and spells can adjust modifiers on live effects:

```
# Set DR to 75% on target's sanctuary:
modeffect $TARGET sanctuary damage_reduction 75

# Shorthand for single-behavior effects (targets modifiers[0]):
modeffect $TARGET giant_strength 8

# Relative adjustment (-10 from current value):
modeffect $TARGET sanctuary damage_reduction -10 relative
```

Under the hood, label resolution is a small loop over `behavior_count` (max 4):

```c
int get_behavior_modifier(EFFECT_DATA *eff, int behavior_index) {
    if (eff->modifiers[behavior_index] != 0)
        return eff->modifiers[behavior_index];
    return eff->def->behaviors[behavior_index].default_modifier;
}
```

#### EFFECT_DEF Structure

```c
typedef enum {
    EFFECT_TARGET_CHARACTER,    // Applies to characters (CHAR_DATA)
    EFFECT_TARGET_OBJECT,       // Applies to objects (OBJ_DATA)
    EFFECT_TARGET_ROOM,         // Applies to a single room (ROOM_INDEX_DATA)
    EFFECT_TARGET_AREA,         // Applies to an entire area/zone (AREA_DATA)
    EFFECT_TARGET_REGION,       // Applies to a region within an area (AREA_REGION)
    EFFECT_TARGET_ANY           // Can apply to any target type
} effect_target_t;

struct effect_def {
    EFFECT_DEF *    next;           // Global linked list
    bool            valid;
    int             index;          // Sequential index for O(1) array lookup

    // Identity
    char *          id;             // Unique string key (e.g., "fire_barrier")
    char *          name;           // Display name (e.g., "Fire Barrier")
    char *          description;    // Long description
    char *          category;       // Grouping (defensive, offensive, utility, etc.)

    // Inheritance (resolved at load time — parent fields copied, child overrides applied)
    char *          parent_id;      // ID of parent def (NULL = no parent)
    EFFECT_DEF *    parent;         // Resolved pointer (NULL after flattening)

    // Targeting
    effect_target_t target;         // What it can apply to
    bool            beneficial;     // Is this a buff (true) or debuff (false)?
    bool            dispellable;    // Can this be removed by dispel magic?

    // Behaviors — what the effect actually does
    // Each effect has 1..MAX_EFFECT_BEHAVIORS typed behavior entries.
    // The engine processes all active behaviors on each instance.
    int             behavior_count;
    EFFECT_BEHAVIOR behaviors[MAX_EFFECT_BEHAVIORS];

    // Stacking rules
    int             stack_mode;     // STACK_REPLACE, STACK_REFRESH, STACK_BEST,
                                    //   STACK_ADD, STACK_INDEPENDENT
    int             max_stacks;     // Maximum stack count (0 = unlimited)

    // Spatial precedence
    // List of effect IDs that this effect suppresses when active at the
    // same or narrower spatial scope. A room-level divine sanctuary that
    // suppresses "miasma" blocks a region-level miasma in that room only.
    LLIST *         suppresses;     // LLIST of char* effect IDs (NULL = none)

    // Script hook (optional)
    // If set, this token is loaded at boot and its scripts are fired for
    // effect lifecycle events. Enables complex behaviors that go beyond
    // the generic trigger/action system — e.g., a miasma that degrades
    // sanctuary effects over time, or a curse that spreads on kill.
    //
    // Supported triggers on the token:
    //   TRIG_EFFECT_APPLY     - fired when effect is applied to a target
    //   TRIG_EFFECT_REMOVE    - fired when effect is removed
    //   TRIG_EFFECT_TICK      - fired each tick (after periodic action)
    //   TRIG_EFFECT_TRIGGER   - fired when reactive trigger fires
    //   TRIG_EFFECT_SUPPRESS  - fired when this effect suppresses another
    //   TRIG_EFFECT_INTERACT  - fired when another effect is applied to
    //                           a target that already has this effect
    //
    // The script receives context variables for the effect, target,
    // source, and (for interactions) the other effect involved.
    WNUM            script_token;   // Token wnum for script hooks (0 = none)

    // Messages
    char *          msg_on;         // Message when effect is applied
    char *          msg_off;        // Message when effect wears off
    char *          msg_room_on;    // Room message on application
    char *          msg_room_off;   // Room message on expiry
    char *          msg_tick;       // Periodic tick message (if any)
    char *          msg_trigger;    // Reactive trigger message (if any)

    // Display
    char *          look_desc;      // What others see ("is surrounded by flames")
    char *          score_desc;     // What the character sees in 'score'

    // OLC
    void *          olc_history;
};
```

#### EFFECT_DATA (Active Effect Instance) — on characters, objects, rooms

Simplified from current AFFECT_DATA. The definition carries the behavior;
the instance carries the runtime state.

```c
struct effect_data {
    EFFECT_DATA *   next;           // Linked list on target
    bool            valid;

    // What effect this is
    EFFECT_DEF *    def;            // Points to the effect definition
    // Legacy compat: for effects without a def (transition period)
    char *          custom_name;    // Custom display name override

    // Per-instance behavior control
    uint8_t         behavior_mask;  // Bit per behavior; default = all bits set
                                    // Allows selectively disabling def behaviors
    int16_t         modifiers[MAX_EFFECT_BEHAVIORS]; // Per-behavior runtime values
                                    // 0 = use def's default_modifier for that behavior

    // Runtime state
    int16_t         level;          // Caster/effect level
    int16_t         duration;       // >0 timed, 0 expired, <0 permanent
    int16_t         stacks;         // Current stack count

    // Source attribution
    effect_source_t source_type;    // What kind of thing created this
    SKILL_DATA *    skill;          // Skill/spell that created this (if any)
    TOKEN_DATA *    token;          // Source token (bidirectional link)
    unsigned long   source_id;      // UID of caster/item (0 = unknown)
    char *          source_name;    // Display name of source

    // Group (for display organization)
    int16_t         group;          // EFFGROUP_* category
};
```

Note what's gone:
- `type` (int16_t skill number) — replaced by `skill` pointer + `def` pointer
- `where` (TO_AFFECTS, TO_IMMUNE, etc.) — the definition describes targeting
- `bitvector` / `bitvector2` — state flags derived from behavior state_flags
- `location` — the definition carries `apply_location` per behavior
- `modifier` (single) — replaced by per-behavior `modifiers[]` array
- `slot` — tracked elsewhere
- `random` — rarely used, can be added back if needed

#### Character State Cache

For hot-path boolean checks, characters still maintain a fast lookup. But it's
a **derived cache**, not the source of truth:

```c
// In CHAR_DATA — rebuilt by effect_rebuild_cache(ch) whenever effects change:
bool            effect_states[MAX_EFFECT_STATES];   // Indexed by state flag index
bool            effect_states_perm[MAX_EFFECT_STATES]; // Permanent (racial, equipment)

// Fast check macro:
#define HAS_EFFECT_STATE(ch, state_idx)  ((ch)->effect_states[(state_idx)])

// Or string-based (slightly slower, for non-hot paths):
bool ch_has_effect_state(CHAR_DATA *ch, const char *state_name);
```

State flags are a subset of effect behaviors — only BEHAVIOR_STATE entries
set a boolean state. An effect can have multiple state behaviors (though this
is uncommon). The cache rebuild scans all active effects, iterates their
behaviors (respecting behavior_mask), and sets state flags from each
BEHAVIOR_STATE entry found. This replaces the entire AFF_/AFF2_ flag bank
system while keeping O(1) boolean checks in combat.

### Lookup API

Following the trait system pattern:

```c
// Definition lookup (cached after first call, like trait_def_lookup)
EFFECT_DEF *    effect_def_lookup(const char *id);
EFFECT_DEF *    effect_def_lookup_name(const char *name);

// Check if character has an active instance of a named effect
bool            ch_has_effect(CHAR_DATA *ch, const char *effect_id);
EFFECT_DATA *   ch_find_effect(CHAR_DATA *ch, const char *effect_id);
EFFECT_DATA *   ch_find_effect_by_skill(CHAR_DATA *ch, SKILL_DATA *skill);
EFFECT_DATA *   ch_find_effect_by_source(CHAR_DATA *ch, unsigned long source_id);

// Same for objects
bool            obj_has_effect(OBJ_DATA *obj, const char *effect_id);

// Spatial effect checks — room, region, area
bool            room_has_effect(ROOM_INDEX_DATA *room, const char *effect_id);
bool            region_has_effect(AREA_REGION *region, const char *effect_id);
bool            area_has_effect(AREA_DATA *area, const char *effect_id);

// Resolved spatial check — walks room -> region -> area
// "Is this effect active anywhere in the spatial hierarchy for this room?"
bool            room_has_effective_effect(ROOM_INDEX_DATA *room, const char *effect_id);

// Character spatial shorthand — checks ch's own effects + room hierarchy
bool            ch_under_spatial_effect(CHAR_DATA *ch, const char *effect_id);

// Fast state check (hot path, uses cached boolean array)
bool            ch_has_effect_state(CHAR_DATA *ch, const char *state_name);

// Apply / remove — characters, objects, spatial targets
void            effect_to_char(CHAR_DATA *ch, EFFECT_DATA *eff);
void            effect_to_obj(OBJ_DATA *obj, EFFECT_DATA *eff);
void            effect_to_room(ROOM_INDEX_DATA *room, EFFECT_DATA *eff);
void            effect_to_region(AREA_REGION *region, EFFECT_DATA *eff);
void            effect_to_area(AREA_DATA *area, EFFECT_DATA *eff);
void            effect_remove(CHAR_DATA *ch, EFFECT_DATA *eff);
void            effect_remove_obj(OBJ_DATA *obj, EFFECT_DATA *eff);
void            effect_remove_room(ROOM_INDEX_DATA *room, EFFECT_DATA *eff);
void            effect_remove_region(AREA_REGION *region, EFFECT_DATA *eff);
void            effect_remove_area(AREA_DATA *area, EFFECT_DATA *eff);

// Strip by definition, skill, source, or custom name
void            effect_strip(CHAR_DATA *ch, EFFECT_DEF *def);
void            effect_strip_skill(CHAR_DATA *ch, SKILL_DATA *skill);
void            effect_strip_name(CHAR_DATA *ch, const char *custom_name);

// Rebuild cached state flags from active effects
void            effect_rebuild_cache(CHAR_DATA *ch);
```

### JSON Effect Definition Format

```json
{
    "id": "fire_barrier",
    "name": "Fire Barrier",
    "description": "A shimmering wall of flame surrounds the target.",
    "category": "defensive",
    "target": "character",
    "beneficial": true,
    "dispellable": true,

    "behaviors": [
        {
            "type": "state",
            "state_flag": "fire_barrier"
        },
        {
            "type": "reactive",
            "label": "retaliation",
            "trigger_event": "melee_hit_received",
            "trigger_chance": 10,
            "trigger_action": "retaliate_damage",
            "default_param": 0,
            "dam_type": "fire"
        }
    ],

    "stack_mode": "replace",

    "msg_on": "You are surrounded by a barrier of flames.",
    "msg_off": "Your fire barrier fades away.",
    "msg_room_on": "$n is surrounded by a barrier of flames.",
    "msg_room_off": "$n's fire barrier fades away.",
    "msg_trigger": "$N's fire barrier flares, burning $n!",

    "look_desc": "is surrounded by a shimmering barrier of flames"
}
```

Inheritance example — a stronger version only needs to override what changes:

```json
{
    "id": "fire_barrier_greater",
    "name": "Greater Fire Barrier",
    "parent": "fire_barrier",
    "behaviors": [
        { "type": "reactive", "label": "retaliation", "trigger_chance": 20, "default_param": 15 }
    ],
    "msg_on": "You are engulfed by a roaring wall of flames.",
    "look_desc": "is engulfed by a roaring wall of flames"
}
```

All other fields (target, stack_mode, state behavior, etc.) are inherited from
`fire_barrier`. Behavior inheritance matches by label — the child's "retaliation"
overrides the parent's "retaliation", while the parent's "state" behavior carries
through unchanged. Resolved at load time so runtime has zero inheritance overhead.

Compare how the same thing works today — the behavior is scattered across three
files with no single source of truth:

```c
// magic_fire.c — creates the effect
af.bitvector2 = AFF2_FIRE_BARRIER;

// fight.c:1583 — hardcodes the behavior
if (IS_AFFECTED2(victim, AFF2_FIRE_BARRIER) && number_percent() < 10)
    fire_retaliation_damage();

// bit.c — hardcodes the display name
if (vector2 & AFF2_FIRE_BARRIER) strcat(buf, " fire_barrier");
```

### Migration from IS_AFFECTED to Data-Driven

The migration follows the exact pattern used for IS_RACE -> ch_has_trait:

#### Category 1: State Checks (direct replacement)

```c
// Before:
if (IS_AFFECTED(ch, AFF_BLIND))

// After:
if (HAS_EFFECT_STATE(ch, ESTATE_BLIND))
// Or for non-hot paths:
if (ch_has_effect_state(ch, "blind"))
```

The state cache ensures these remain O(1) — no performance regression in combat.

#### Category 2: Behavior Modifiers (move logic to definition)

```c
// Before (fight.c) — hardcoded sanctuary behavior:
if (IS_AFFECTED(ch, AFF_SANCTUARY))
    dam /= 2;

// After — definition-driven:
// sanctuary.json behaviors: [
//   { "type": "state", "state_flag": "sanctuary" },
//   { "type": "reactive", "label": "damage_reduction",
//     "trigger_event": "damage_received",
//     "trigger_action": "damage_reduction", "default_param": 50 }
// ]
// fight.c calls a generic handler:
dam = apply_damage_reduction_effects(ch, dam);
```

The key insight: `apply_damage_reduction_effects()` iterates active effects,
scanning each instance's behaviors for any BEHAVIOR_REACTIVE with
`trigger_event == damage_received` and `trigger_action == damage_reduction`.
It doesn't know about "sanctuary" — it processes whatever damage reduction
behaviors exist across all active effects. New DR effects can be added in JSON
without touching fight.c. Per-instance modifiers allow the DR percentage to
vary by caster level or spell variant.

#### Category 3: Reactive Effects (generalized handlers)

```c
// Before (fight.c) — three near-identical blocks for three barriers:
if (IS_AFFECTED2(victim, AFF2_FIRE_BARRIER) && number_percent() < 10)
    fire_retaliation_damage();
if (IS_AFFECTED2(victim, AFF2_FROST_BARRIER) && number_percent() < 10)
    frost_retaliation_damage();
if (IS_AFFECTED2(victim, AFF2_ELECTRICAL_BARRIER) && number_percent() < 10)
    shock_retaliation_damage();

// After — single generic handler:
apply_reactive_effects(victim, attacker, TRIGGER_MELEE_HIT_RECEIVED);
// Iterates all active effects, checks trigger_event match, rolls trigger_chance,
// executes trigger_action with trigger_param and trigger_dam_type.
// All three barriers (and any future ones) handled by one code path.
```

### Trigger Events and Actions

These are the vocabulary for reactive/periodic effect behaviors:

```c
// Trigger events — WHEN does the effect fire?
typedef enum {
    TRIGGER_NONE = 0,
    TRIGGER_MELEE_HIT_DEALT,        // Character lands a melee hit
    TRIGGER_MELEE_HIT_RECEIVED,     // Character is hit by melee
    TRIGGER_SPELL_HIT_RECEIVED,     // Character is hit by a spell
    TRIGGER_DAMAGE_RECEIVED,        // Character takes any damage
    TRIGGER_TICK,                   // Periodic tick (once per pulse)
    TRIGGER_COMBAT_ROUND,           // Each combat round
    TRIGGER_ROOM_ENTER,             // Character enters a room
    TRIGGER_ROOM_EXIT,              // Character leaves a room
    TRIGGER_SKILL_USE,              // Character uses a skill
    TRIGGER_SPELL_CAST,             // Character casts a spell
    TRIGGER_DEATH,                  // Character dies
    TRIGGER_KILL,                   // Character kills something
    TRIGGER_REST,                   // Character rests
    TRIGGER_WAKE,                   // Character wakes
} effect_trigger_t;

// Trigger actions — WHAT does the effect do?
typedef enum {
    ACTION_NONE = 0,
    ACTION_DAMAGE,                  // Deal damage to trigger source
    ACTION_HEAL,                    // Heal the effect holder
    ACTION_DAMAGE_REDUCTION,        // Reduce incoming damage by param %
    ACTION_STAT_DRAIN,              // Reduce a stat
    ACTION_MANA_DRAIN,              // Drain mana
    ACTION_MOVE_DRAIN,              // Drain movement
    ACTION_DISPEL,                  // Remove effects from trigger source
    ACTION_STUN,                    // Stun the trigger source
    ACTION_FLEE,                    // Force flee
    ACTION_MESSAGE,                 // Display a message only
    ACTION_SCRIPT,                  // Run a script trigger
} effect_action_t;
```

Not every behavior uses these — BEHAVIOR_STATE and BEHAVIOR_PASSIVE entries have
no trigger/action. BEHAVIOR_MODIFIER entries use `apply_location` and the
per-instance modifier value. Only BEHAVIOR_REACTIVE and BEHAVIOR_PERIODIC entries
use the trigger/action system. A single effect definition can combine multiple
behavior types (e.g., sanctuary has both a STATE and a REACTIVE behavior).

### Source Attribution

Built into EFFECT_DATA from the start:

```c
typedef enum {
    EFFECT_SOURCE_NONE = 0,     // Unknown/legacy
    EFFECT_SOURCE_SPELL,        // Cast by a character
    EFFECT_SOURCE_SKILL,        // Passive skill effect
    EFFECT_SOURCE_TOKEN,        // Applied by a token
    EFFECT_SOURCE_ITEM,         // From worn/held equipment
    EFFECT_SOURCE_RACIAL,       // Innate racial ability
    EFFECT_SOURCE_CLASS,        // Class-granted effect
    EFFECT_SOURCE_ROOM,         // Environmental/room effect
    EFFECT_SOURCE_CONSUMABLE,   // Potion, scroll, wand, etc.
    EFFECT_SOURCE_SCRIPT,       // Applied by game script
    EFFECT_SOURCE_SYSTEM        // System-applied (e.g., death penalties)
} effect_source_t;
```

Use cases:
- **Dispel targeting**: remove only spell-sourced effects, or all effects from a
  specific caster
- **Player display**: "Fire Barrier (cast by Gandalf, 3 hours remaining)"
- **Stacking rules**: "only one instance per source" or "best level wins"
- **Script queries**: "who applied this effect to me?"

### Spatial Effects (Rooms, Regions, Areas)

Effects can target three spatial scopes, all using the same EFFECT_DATA struct.
Each scope gains an effect list:

```c
// In ROOM_INDEX_DATA:
EFFECT_DATA *   effects;        // Active room effects
EFFECT_DATA *   base_effects;   // Template effects (reset with zone)

// In AREA_REGION:
EFFECT_DATA *   effects;        // Active region effects

// In AREA_DATA:
EFFECT_DATA *   effects;        // Active area-wide effects
```

#### Scope Hierarchy, Propagation, and Precedence

Effects at broader scopes apply to everything within them:

```
Area Effect (AREA_DATA)
 └─ affects all rooms (and their occupants) in the area
    └─ includes all regions within the area

Region Effect (AREA_REGION)
 └─ affects all rooms (and their occupants) in the region

Room Effect (ROOM_INDEX_DATA)
 └─ affects all occupants of that specific room
```

When resolving what effects apply at a given location, the system checks all
three levels — but with **inside-out precedence**. Inner scopes can override
or negate outer scopes. This is analogous to the trait system's layered lookup
(personal > class > race), where the most specific layer wins.

**Resolution order** (innermost first):

```
1. Room effects      (most specific — highest priority)
2. Region effects    (mid-scope)
3. Area effects      (broadest — lowest priority)
```

**Interaction rules:**

- **Beneficial vs. harmful**: A beneficial effect at a narrower scope can
  block or suppress a harmful effect from a broader scope. A divine sanctuary
  on a room blocks the region's miasma *in that room only*. The miasma still
  applies everywhere else in the region.

- **Same-category stacking**: When the same category of effect exists at
  multiple scopes (e.g., damage reduction at both room and area level), the
  innermost scope takes precedence by default. The `stack_mode` on the
  definition controls this — `replace` means inner wins, `best` means
  highest value wins, `add` means they combine.

- **Suppression flag**: Effect definitions can specify `suppresses` — a list
  of effect IDs that this effect blocks when active at the same or narrower
  scope:

```json
{
    "id": "divine_sanctuary_room",
    "name": "Divine Sanctuary",
    "target": "room",
    "behaviors": [
        { "type": "reactive", "label": "damage_reduction",
          "trigger_event": "damage_received",
          "trigger_action": "damage_reduction", "default_param": 100 }
    ],
    "suppresses": ["miasma", "poison_cloud", "arcane_storm"],
    "msg_on": "A divine light shields this place from all harm."
}
```

- **Practical example**: A swamp region has a miasma (periodic poison).
  A cleric consecrates a specific room within the swamp. That room's
  consecration effect lists `"suppresses": ["miasma"]`. Characters in
  that room are shielded. Step outside, and the miasma applies again.

```c
// Resolved spatial check — walks room -> region -> area with precedence:
// Returns true only if the effect is active AND not suppressed at a
// narrower scope.
bool room_has_effective_effect(ROOM_INDEX_DATA *room, const char *effect_id);

// Character spatial shorthand — checks ch's own effects + room hierarchy
bool ch_under_spatial_effect(CHAR_DATA *ch, const char *effect_id);

// Check specific scopes (without precedence resolution):
bool room_has_effect(ROOM_INDEX_DATA *room, const char *effect_id);
bool region_has_effect(AREA_REGION *region, const char *effect_id);
bool area_has_effect(AREA_DATA *area, const char *effect_id);
```

#### Room-Scoped Examples

```json
{
    "id": "magical_darkness",
    "name": "Magical Darkness",
    "target": "room",
    "behaviors": [
        { "type": "state", "state_flag": "dark" }
    ],
    "dispellable": true,
    "msg_on": "Darkness engulfs the room.",
    "msg_off": "The magical darkness lifts."
}
```

```json
{
    "id": "poison_cloud",
    "name": "Poison Cloud",
    "target": "room",
    "behaviors": [
        { "type": "periodic", "action": "damage", "default_param": 10, "dam_type": "poison" }
    ],
    "dispellable": true,
    "msg_on": "A cloud of poisonous gas fills the room.",
    "msg_off": "The poison cloud dissipates.",
    "msg_tick": "The poison cloud burns your lungs!"
}
```

#### Region/Area-Scoped Examples

Region and area effects enable zone-wide environmental events — a miasma
spreading across a swamp, a divine blessing over a temple district, a
magical storm engulfing an entire continent:

```json
{
    "id": "miasma",
    "name": "Miasma",
    "description": "A choking, toxic fog blankets the region.",
    "target": "region",
    "behaviors": [
        { "type": "periodic", "action": "damage", "default_param": 5, "dam_type": "poison" }
    ],
    "beneficial": false,
    "dispellable": false,
    "msg_tick": "The miasma burns your lungs and stings your eyes.",
    "look_desc": "A sickly green fog hangs heavy in the air."
}
```

```json
{
    "id": "divine_sanctuary",
    "name": "Divine Sanctuary",
    "target": "area",
    "behaviors": [
        { "type": "reactive", "label": "damage_reduction",
          "trigger_event": "damage_received",
          "trigger_action": "damage_reduction", "default_param": 25 }
    ],
    "beneficial": true,
    "msg_on": "A warm golden light suffuses the area.",
    "look_desc": "A warm golden radiance permeates this place."
}
```

```json
{
    "id": "arcane_storm",
    "name": "Arcane Storm",
    "target": "area",
    "behaviors": [
        { "type": "state", "state_flag": "arcane_disruption" },
        { "type": "periodic", "label": "energy_bolts",
          "action": "damage", "default_param": 8, "dam_type": "energy" }
    ],
    "beneficial": false,
    "msg_on": "The sky crackles with wild arcane energy!",
    "msg_tick": "A bolt of wild magic arcs through the area!",
    "look_desc": "The sky churns with wild, crackling arcane energy."
}
```

#### Spatial Effect Checks

Spatial effects are checked:
- On room entry (state effects, look descriptions)
- On tick (periodic effects applied to all occupants)
- On look (display spatial effect descriptions from all scopes)
- On spell cast (check for antimagic, silence at any scope)
- On movement (check for immobilization, gravity)
- On combat events (reactive effects from any scope)

#### Script Integration

Scripts can apply effects at any spatial scope, with optional behavior control:

```
# Basic spatial application:
ADDEFFECT room <effect_id> [level [duration]]
ADDEFFECT region <region_uid> <effect_id> [level [duration]]
ADDEFFECT area <area_uid> <effect_id> [level [duration]]

# With behavior mask (by label):
ADDEFFECT $TARGET sanctuary -skip damage_reduction
ADDEFFECT $TARGET sanctuary -only state

# With per-behavior modifier overrides (by label):
ADDEFFECT $TARGET sanctuary damage_reduction=75 level=$LEVEL duration=12

# Modify a live effect's behavior modifier:
MODEFFECT $TARGET sanctuary damage_reduction 75
MODEFFECT $TARGET giant_strength 8              # shorthand for single-behavior
MODEFFECT $TARGET sanctuary damage_reduction -10 relative  # adjust, not set
```

This enables event-driven environmental storytelling — a quest triggers a miasma
spreading across a swamp region, a boss death lifts a curse from an entire area,
a ritual consecrates a temple district. Behavior masks and per-instance modifiers
allow the same effect definition to serve multiple use cases without creating
variant definitions for every permutation.

### Token Integration

Token-effect coupling works the same as today, but cleaner:

- EFFECT_DATA retains `TOKEN_DATA *token` for bidirectional linking
- Auto-extraction on last effect expiry (port from 2.0)
- Token reference persisted via wnum in save format
- Linkage reconstructed at load time

Script commands for token-driven effects:
- `GRANTSKILL player name|vnum [rating [permanent [flags]]]`
- `REVOKESKILL player name|vnum`
- `ADDEFFECT target effect_id [level [duration]] [-skip label] [-only label] [label=value ...]`
- `MODEFFECT target effect_id [label] value [relative]`
- `REMEFFECT target effect_id`

Token event triggers:
- `TRIG_TOKEN_GIVEN` — fires when token is granted
- `TRIG_TOKEN_REMOVED` — fires when token is removed

### Display Integration via Aura System

Rather than building a separate display path, effect visibility delegates to
the **aura subsystem** (see [PLAN_AURA_SYSTEM_BACKPORT.md](PLAN_AURA_SYSTEM_BACKPORT.md)).

The aura system provides:
- Per-character list of named visual entries (`name` + `long_descr`)
- Room/look output rendering with `%s` character name substitution
- `MAX_AURAS_SHOWN` cap to prevent output bloat
- Persistence on player save/load

Effect integration:
- When `effect_to_char()` applies an effect with a non-NULL `look_desc`, it
  automatically calls `add_aura_to_char(ch, def->id, def->look_desc)`.
- When `effect_to_char()` applies an effect with a non-NULL `score_desc`, it
  automatically calls `add_aura_score_to_char(ch, def->id, def->score_desc)`.
- When `effect_remove()` strips it, it removes both aura entries.
- The aura system's upsert behavior (same name = update, not duplicate) aligns
  with effect stacking — replacing an effect updates the aura entry.

This means the effect system does not need its own display rendering in
`act_info.c` — it delegates entirely to the aura subsystem for both room
output (look_desc) and self output (score_desc). The aura backport becomes
a **prerequisite** for Phase 2 (or at minimum Phase 3).

### OLC Editor (effedit)

Following the traitedit pattern, an in-game OLC editor for effect definitions:

- Create/modify effect definitions without code changes
- Edit all fields (behaviors, triggers, actions, messages, stacking)
- Automatic serialization to `data/effects/effects.json`
- Change history tracking
- Validation (e.g., reactive behaviors must have trigger_event)

---

## Implementation Phases

### Phase 1: Effect Definition System

**Risk: LOW (additive, parallel to existing system)**

Build the EFFECT_DEF infrastructure alongside the existing affect system. Both
systems run simultaneously during migration.

Tasks:
- [ ] Define EFFECT_BEHAVIOR struct (tagged union) and EFFECT_DEF struct in new `effects.h`
- [ ] Define behavior type enum, target enum, MAX_EFFECT_BEHAVIORS constant
- [ ] Implement `effects.c` with definition loading, lookup, registration
- [ ] Implement behaviors array JSON parsing (type dispatch, label handling, union population)
- [ ] Implement inheritance resolution at load time (flatten parent fields into child,
      merge behaviors by label — child overrides parent behaviors with matching labels)
- [ ] Create `data/effects/effects.json` with definitions for all current AFF_/AFF2_ flags
- [ ] Create definitions for current AFFLICTION_DATA types (folding into effect system)
- [ ] Implement `effect_def_lookup()` with indexed array (same as trait_def_lookup)
- [ ] Implement `find_behavior_index()` — resolve label string to behavior array index
- [ ] Build effedit OLC editor (following traitedit pattern, with behavior editing support)
- [ ] Update Makefile and CMakeLists.txt

Acceptance criteria:
- Effect definitions load at boot, inheritance resolved
- `effect_def_lookup("blind")` returns correct definition
- Child defs inherit parent fields with overrides applied
- Behavior labels resolve correctly to array indices
- Multi-behavior definitions (e.g., sanctuary with state + reactive) parse correctly
- effedit can create/modify definitions (including behaviors and parent)
- Existing affect system completely unchanged

### Phase 2: New EFFECT_DATA + State Cache

**Risk: MEDIUM (new struct, cache system)**

Introduce the new EFFECT_DATA struct and character state cache. Wire up the
core lifecycle functions (apply, remove, rebuild cache).

Tasks:
- [ ] Define new EFFECT_DATA struct with behavior_mask and per-behavior modifiers[] array
- [ ] Add `effect_states[]` and `effect_states_perm[]` to CHAR_DATA
- [ ] Implement effect_to_char(), effect_remove(), effect_rebuild_cache()
- [ ] Implement behavior mask application (default all-on, respect mask in all processing)
- [ ] Implement get_behavior_modifier() (instance override vs def default fallback)
- [ ] Implement effect_to_obj(), effect_remove_obj()
- [ ] Add `EFFECT_DATA *effects` to ROOM_INDEX_DATA, AREA_REGION, and AREA_DATA
- [ ] Add `EFFECT_DATA *base_effects` to ROOM_INDEX_DATA (template effects for zone reset)
- [ ] Implement effect_to_room(), effect_to_region(), effect_to_area() and their removals
- [ ] Implement room_has_effective_effect() — resolved lookup across room -> region -> area
- [ ] Implement ch_under_spatial_effect() — character + spatial hierarchy check
- [ ] Implement source attribution fields
- [ ] Implement token bidirectional linking (same as current)
- [ ] Implement ch_has_effect(), ch_has_effect_state(), obj_has_effect()
- [ ] Wire look_desc into aura system (auto add/remove aura on effect apply/remove)

Acceptance criteria:
- New effect functions work for create/remove lifecycle
- State cache correctly reflects active effects
- Source attribution populated and queryable
- Spatial effects at room, region, and area scope can be applied and removed
- room_has_effective_effect() correctly resolves through room -> region -> area
- Effects with look_desc automatically create/remove aura entries

### Phase 3: Behavior Handlers

**Risk: MEDIUM (replaces hardcoded logic)**

Implement the generic trigger/action handlers that replace hardcoded flag checks
for parametric effects.

Tasks:
- [ ] Implement behavior dispatch loop (iterate behaviors, check mask, switch on type)
- [ ] Implement apply_reactive_effects() — scans BEHAVIOR_REACTIVE entries across all active effects
- [ ] Implement apply_periodic_effects() — scans BEHAVIOR_PERIODIC entries across all active effects
- [ ] Implement apply_damage_reduction_effects() — replaces sanctuary checks (uses per-instance modifier)
- [ ] Implement apply_modifier_effects() — replaces affect_modify stat logic (per-behavior modifiers)
- [ ] Wire periodic handler into update.c tick loop
- [ ] Wire reactive handler into fight.c combat events
- [ ] Wire damage reduction into fight.c damage calculation
- [ ] Add room periodic effect processing to room update loop

Acceptance criteria:
- Fire/frost/electrical barriers work via generic reactive handler
- Sanctuary/damage reduction works via generic DR handler, with per-instance modifier support
- Healing aura/regeneration work via generic periodic handler
- Poison/plague tick damage works via generic periodic handler
- Behavior masks correctly disable individual behaviors on specific instances
- Per-instance modifiers correctly override def defaults
- All behavior matches current hardcoded behavior exactly

### Phase 4: Migration (affect -> effect)

**Risk: HIGH (touches every file, but mechanical)**

Migrate all existing code from the old affect system to the new effect system.
This is the big rename + behavioral migration.

Tasks:
- [ ] Create mapping table: each AFF_/AFF2_ flag -> its EFFECT_DEF id
- [ ] Migrate spell/skill code to create EFFECT_DATA with EFFECT_DEF references
- [ ] Replace IS_AFFECTED/IS_AFFECTED2 checks with HAS_EFFECT_STATE or ch_has_effect
- [ ] Replace hardcoded behavior blocks with generic handler calls
- [ ] Update save.c to persist new EFFECT_DATA format
- [ ] Maintain backward-compatible loading of old save formats
- [ ] Update act_info.c (do_affects -> do_effects) display
- [ ] Update OLC affect editing to use new system
- [ ] Update script commands for effect manipulation
- [ ] Port auto-extraction from 2.0 (token cleanup on last effect expiry)
- [ ] Remove AFFECT_DATA struct, old AFF_/AFF2_ defines, IS_AFFECTED macros
- [ ] Rename: affect -> effect throughout (functions, variables, comments)
- [ ] Keep `affects` as player command alias for `effects`

Acceptance criteria:
- All spells/skills create effects through new system
- All flag checks use new API
- Save/load works with new format (and loads old format)
- No AFFECT_DATA references remain
- All tests pass

### Phase 5: Token + Script Completion

**Risk: LOW (well-understood patterns)**

Complete the token integration and add script support for the new effect system.

Tasks:
- [ ] Persist token-effect linkage in save format
- [ ] Restore linkage on load
- [ ] Port auto-extraction from 2.0 update.c
- [ ] Add TRIG_TOKEN_GIVEN / TRIG_TOKEN_REMOVED triggers
- [ ] Implement ADDEFFECT script command (with -skip/-only mask and label=value modifier syntax)
- [ ] Implement MODEFFECT script command (label-based modifier adjustment, absolute and relative)
- [ ] Implement REMEFFECT script command
- [ ] Port GRANTSKILL / REVOKESKILL from 2.0
- [ ] Add script variable access for effect queries

Acceptance criteria:
- Token-effect linkage survives save/load
- Tokens auto-extract when last effect expires
- Scripts can apply/remove/query effects
- Token triggers fire correctly

### Phase Dependencies

```
Aura backport                -> prerequisite (PLAN_AURA_SYSTEM_BACKPORT.md)
Phase 1 (definitions)        -> standalone, build first
Phase 2 (EFFECT_DATA+cache)  -> depends on Phase 1 + aura backport
Phase 3 (behavior handlers)  -> depends on Phase 2
Phase 4 (migration)          -> depends on Phase 1, 2, 3
Phase 5 (token+scripts)      -> depends on Phase 2, can parallel Phase 3/4
```

### Naming Convention

Throughout this migration:

| Old | New |
|-----|-----|
| AFFECT_DATA | EFFECT_DATA |
| affect_data | effect_data |
| affect_to_char() | effect_to_char() |
| affect_remove() | effect_remove() |
| affect_modify() | effect_apply_modifiers() |
| affect_fix_char() | effect_rebuild_cache() |
| affect_strip() | effect_strip() |
| affect_join() | effect_stack() |
| ch->affected | ch->effects |
| obj->affected | obj->effects |
| new_affect() | new_effect() |
| free_affect() | free_effect() |
| AFF_BLIND | (via effect_def "blind") |
| AFF2_SILENCE | (via effect_def "silence") |
| IS_AFFECTED(ch, AFF_X) | HAS_EFFECT_STATE(ch, ESTATE_X) |
| IS_AFFECTED2(ch, AFF2_X) | HAS_EFFECT_STATE(ch, ESTATE_X) |
| affected_by[] | effect_states[] |
| affected_by_perm[] | effect_states_perm[] |
| AFFGROUP_ | EFFGROUP_ |
| do_affects | do_effects |

---

## Files Impacted

### Phase 1 (Definitions) — New Files

- effects.h (new — struct definitions, API)
- effects.c (new — definition loading, lookup, registration)
- editors/effedit.c (new — OLC editor)
- data/effects/effects.json (new — effect definitions)
- Makefile, CMakeLists.txt (add new files)

### Phase 2 (EFFECT_DATA + Cache) — Core Changes

- merc.h (new struct, cache arrays on CHAR_DATA, effect list on ROOM_INDEX_DATA)
- handler.c (new effect lifecycle functions)
- mem.c (new_effect/free_effect allocators)
- recycle.h (allocator declarations)

### Phase 3 (Behavior Handlers) — Targeted

- fight.c (wire reactive + DR handlers, remove hardcoded flag checks)
- fight2.c (same)
- update.c (wire periodic handler, room effect ticks)
- act_move.c (room entry/exit effect handling)
- handler.c (generic handler implementations)

### Phase 4 (Migration) — Nearly Every File

- magic.c, magic2.c, magic_*.c (create EFFECT_DATA instead of AFFECT_DATA)
- fight.c, fight2.c (replace IS_AFFECTED checks)
- act_move.c, act_enter.c (replace IS_AFFECTED checks)
- act_comm.c (replace silence checks)
- act_info.c (rewrite do_affects -> do_effects display)
- act_obj.c, act_obj2.c (equipment effect handling)
- save.c / json_persist.c (new persistence format)
- db.c, db2.c (area loading)
- script_*.c (script effect commands)
- olc_act.c, olc_act2.c (OLC effect editing)
- bit.c (remove old flag name functions)
- tables.c / tables.h (remove old flag tables)
- interp.c (command table)
- special.c (special procedure flag checks)
- skills.c (skill effect application)
- effects.c (existing file — review/integrate)

### Phase 5 (Token + Scripts) — Targeted

- update.c (auto-extraction)
- save.c (token reference persistence)
- scripts.h / tables.c (new triggers)
- script_commands.c (ADDEFFECT/REMEFFECT/GRANTSKILL/REVOKESKILL)

---

## Testing Strategy

### Unit Tests

- Effect definition loading and lookup
- Effect creation/removal lifecycle
- State cache rebuild correctness
- Reactive trigger matching and execution
- Periodic effect tick processing
- Damage reduction calculation
- Modifier application and removal
- Stacking behavior for each stack mode
- Source attribution persistence
- Room effect add/remove/expire
- Token-effect linkage and auto-extraction
- Legacy save format backward compatibility

### Integration Tests

- Cast spell -> verify effect applied with correct def, source, and state
- Effect expiration -> verify cleanup, messaging, cache update
- Token with effects -> save -> load -> verify linkage restored
- Room effect -> enter room -> verify periodic ticks on occupants
- Room effect -> leave room -> verify periodic ticks stop
- Dispel by source type -> verify only matching effects removed
- Equipment with effects -> equip/remove -> verify stat changes and cache
- Reactive barrier -> take hit -> verify retaliation fires at correct rate
- Stacking -> apply same effect twice -> verify stack_mode behavior
- All 55 current AFF_/AFF2_ effects -> verify identical behavior under new system

### Regression Tests

- All existing spell/skill effects produce identical gameplay results
- Combat calculations unchanged
- Save/load cycle preserves all effect data
- OLC effect editing works
- Script effect commands work
- No performance regression in combat loop (benchmark state cache vs old bitwise)

---

## Resolved Questions

1. **Catalyst system**: Catalysts have already been separated into their own
   data structure and no longer use AFFECT_DATA. They remain independent of the
   effect system — no action needed.

2. **Affliction system**: AFFLICTION_DATA (type, state, timer, level, values[4])
   should be **folded into the effect system**. Afflictions are conceptually just
   effects with extra state. The EFFECT_DEF system can represent them with
   appropriate type/category fields, and the values[] array can be handled via
   the modifier + stacks fields or by extending EFFECT_DATA with a small
   fixed-size values array if needed. This eliminates a parallel system that
   duplicates lifecycle management.

3. **Effect inheritance**: **Yes, support it.** Effect definitions should be able
   to specify a `parent` id. Child definitions inherit all fields from the parent
   and override only what they specify. This is important for creators — defining
   "fire_barrier_greater" as inheriting from "fire_barrier" with just
   `trigger_chance: 20` is far more practical than duplicating the entire
   definition. Implementation: resolve inheritance at load time (flatten into
   the child def), so runtime lookups have zero overhead.

4. **Script-defined effects**: **Not needed.** The original motivation for
   ad-hoc script effects (addaffectname, custom_name affects) was that adding
   real effects required code changes. With data-driven EFFECT_DEFs that are
   OLC-editable via effedit, creators can define new effects in-game without
   code changes. Scripts should reference pre-defined effect IDs rather than
   constructing ad-hoc effects. The `custom_name` field on EFFECT_DATA is
   retained for legacy compatibility during migration but is not the intended
   path for new content.

5. **Composite effects**: **Use a behaviors array on EFFECT_DEF.** Each
   definition carries 1..MAX_EFFECT_BEHAVIORS typed behavior entries (tagged
   union). The engine processes all active behaviors per instance. This replaces
   the exclusive `type` enum — there is no COMPOSITE type, any effect can have
   multiple behaviors. Instance-level control is provided via:
   - **behavior_mask** — bitmask on EFFECT_DATA to selectively disable specific
     behaviors (scripts use labels: `-skip damage_reduction`, `-only state`)
   - **per-behavior modifiers** — `modifiers[MAX_EFFECT_BEHAVIORS]` array on
     EFFECT_DATA, parallel to the def's behaviors array. Zero means "use def's
     default." Scripts set by label: `damage_reduction=75` or
     `modeffect $TARGET sanctuary damage_reduction 75`.
   This keeps the definition as the source of truth for what behaviors *exist*,
   while the instance controls which are *active* and *at what values*.

## Open Questions

1. **Room effect scope**: Should room effects transfer to characters (apply on
   entry, remove on exit) or only check contextually? Transferring is simpler
   for state effects; contextual is better for periodic effects.

2. **Performance**: The state cache should make hot-path checks equivalent to
   current bitwise ops. Need to benchmark to confirm. The generic reactive
   handler iterates active effects per combat event — is this fast enough?
   (Probably yes — characters rarely have more than 10-20 active effects.)

3. **Backward compatibility**: How long do we maintain legacy save format
   readers? Should we do a one-time migration tool or keep dual-reading
   indefinitely?

---

## References

- [TOKEN_AFFECT_COUPLING_CHANGES.md](TOKEN_AFFECT_COUPLING_CHANGES.md) - Implemented coupling
- [TOKEN_MIGRATION_ANALYSIS.md](TOKEN_MIGRATION_ANALYSIS.md) - Backport priorities
- [CODE_MIGRATION_PATTERNS.md](CODE_MIGRATION_PATTERNS.md) - Flagset migration patterns
- [PLAN_AURA_SYSTEM_BACKPORT.md](PLAN_AURA_SYSTEM_BACKPORT.md) - Aura system (prerequisite for display)
- [PLAN_backport_skills_classes.md](PLAN_backport_skills_classes.md) - Skill/affect integration
- [PLAN_CASTING_SYSTEM_REWORK.md](PLAN_CASTING_SYSTEM_REWORK.md) - Casting system redesign
- [PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md](PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md) - Combat rebalancing
- [done/PLAN_TRAITS.md](done/PLAN_TRAITS.md) - Trait system design (architectural precedent)
- Trait implementation: traits.h, traits.c, bootstrap/bootstrap_data/traits/traits.json
- Reference implementation: /sentience/src_20_dev/ (handler.c, update.c, save.c, merc.h)
