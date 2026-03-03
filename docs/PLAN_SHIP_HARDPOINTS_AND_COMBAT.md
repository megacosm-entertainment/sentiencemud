# Plan: Ship Hardpoints, Modular Upgrades, and Combat System

**Date:** March 2, 2026
**Status:** Implementation Plan
**Depends On:** `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`, `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`

---

## Purpose

Define and implement a modular hardpoint system for ships (aerial, aquatic, and
terrestrial), ship combat, NPC crew/captain management, and reputation
integration.  This plan is the concrete implementation track for Phases 2–4 of
`PLAN_SHIP_SYSTEM_ARCHITECTURE.md`.

---

## Scope

1. **Modular hardpoint system** — MechWarrior-style slot/module fitting
2. **Ship combat** — targeting, fire/reload cycle, damage model, sinking
3. **NPC ships with crew and captains** — auto-population, AI state machine
4. **Reputation integration** — faction-based consequences from naval combat
5. **Domain coverage** — all mechanics must work for aerial, aquatic, and
   terrestrial vessel classes

---

## Domain Model: Ship Classes

The system must be domain-agnostic at the mechanical level while allowing
domain-specific behavior at the edges (weather effects, terrain interaction,
movement semantics).

### Ship Class Definitions

```c
#define SHIP_SAILING_BOAT     0   // Aquatic — wind/oar-powered water vessel
#define SHIP_AIR_SHIP         1   // Aerial  — airship (engine/balloon/magic)
#define SHIP_LAND_VESSEL      2   // Terrestrial — wagon, caravan, war machine
```

### Domain-Specific Behavior Matrix

| Behavior              | Aquatic              | Aerial                | Terrestrial           |
|-----------------------|----------------------|-----------------------|-----------------------|
| Movement medium       | Water tiles          | Air (any terrain)     | Land roads/paths      |
| Weather impact        | Storms, waves, drift | Turbulence, lightning | Mud, snow, sandstorm  |
| Propulsion types      | Sails, oars          | Engines, balloons     | Beasts, wheels, tracks|
| Combat range bands    | Sea-level broadside  | 3D altitude advantage | Road/terrain LoS      |
| Boarding transition   | Grapple alongside    | Fly alongside/ramp    | Pull alongside/ramp   |
| Sinking/destruction   | Sinks, cargo floats  | Crashes, wreckage     | Breaks down, lootable |
| Docking               | Harbor/port          | Landing pad/mooring   | Waystation/camp       |

### Hardpoint Validity by Domain

Not all hardpoint types make sense for all domains.  Module templates declare
which ship classes they are compatible with via a `domain_flags` bitmask:

```c
#define DOMAIN_AQUATIC      (A)
#define DOMAIN_AERIAL       (B)
#define DOMAIN_TERRESTRIAL  (C)
#define DOMAIN_ALL          (DOMAIN_AQUATIC | DOMAIN_AERIAL | DOMAIN_TERRESTRIAL)
```

---

## A) Hardpoint System

### Design Principles

- Ship hulls define **hardpoint slots** of typed categories and sizes
- Players install **modules** into compatible slots
- Modules grant stat bonuses, weapon capability, cargo, or utility
- Modules require crew with specific skills to operate
- Module weight is bounded by the hull's total module weight budget
- Damaged modules degrade or go offline; repairable by mechanics crew

### Hardpoint Types

```c
#define HARDPOINT_WEAPON      0   // Cannons, ballistae, fire throwers, ram
#define HARDPOINT_DEFENSE     1   // Hull plating, shields, anti-boarding nets
#define HARDPOINT_UTILITY     2   // Cargo holds, crew quarters, nav gear, comms
#define HARDPOINT_PROPULSION  3   // Sails, oars, engines, beast harnesses
#define HARDPOINT_MAX         4
```

### Hardpoint Sizes

```c
#define HARDPOINT_SIZE_SMALL   1
#define HARDPOINT_SIZE_MEDIUM  2
#define HARDPOINT_SIZE_LARGE   3
```

A module's size must be `<=` the slot's size.  A small module fits any slot;
a large module requires a large slot.

### Struct: `SHIP_HARDPOINT_DEF` (template — on `SHIP_INDEX_DATA`)

```c
struct ship_hardpoint_def {
    SHIP_HARDPOINT_DEF *next;
    bool valid;

    int16_t slot_id;          // Unique slot number on this hull (1-based)
    char *name;               // "Port Cannon Bay", "Stern Armor Plate", etc.
    int type;                 // HARDPOINT_WEAPON, etc.
    int size;                 // HARDPOINT_SIZE_SMALL/MEDIUM/LARGE
    long domain_flags;        // Which ship domains this slot applies to
    long flags;               // HARDPOINT_REQUIRED (must be filled), etc.
};
```

### Struct: `SHIP_MODULE_INDEX` (module template — area-scoped vnum space)

```c
struct ship_module_index {
    SHIP_MODULE_INDEX *next;
    bool valid;

    long vnum;
    AREA_DATA *area;

    char *name;
    char *description;

    int type;                 // Must match hardpoint type
    int size;                 // Must be <= hardpoint size
    int weight;               // Contributes to hull module weight budget
    long domain_flags;        // DOMAIN_AQUATIC | DOMAIN_AERIAL | etc.

    // --- Stat bonuses (applied additively to ship base stats) ---
    int hit_bonus;            // Extra hull HP
    int armor_bonus;          // Extra damage reduction
    int speed_bonus;          // Movement speed modifier (percent)
    int turning_bonus;        // Turning speed modifier (degrees)
    int cargo_weight_bonus;   // Extra weight capacity
    int cargo_capacity_bonus; // Extra item capacity
    int crew_bonus;           // Extra crew capacity

    // --- Weapon stats (HARDPOINT_WEAPON only) ---
    int damage;               // Base damage per volley
    int range;                // Max range in wilderness tiles
    int reload_time;          // Ticks between volleys
    int damage_type;          // SHIP_DAMAGE_GRIND, SHIP_DAMAGE_FIRE, etc.
    long weapon_flags;        // MODULE_AOE, MODULE_ANTI_CREW, etc.

    // --- Crew requirements ---
    int16_t operators;        // How many crew members needed
    int16_t req_gunning;      // Minimum gunning skill (0 = no req)
    int16_t req_mechanics;    // Minimum mechanics skill
    int16_t req_scouting;     // Minimum scouting skill
    int16_t req_navigation;   // Minimum navigation skill
    int16_t req_oarring;      // Minimum oarring skill
    int16_t req_leadership;   // Minimum leadership skill

    // --- Ammo (optional) ---
    union {
        WNUM_LOAD load;
        long vnum;
    } ammo_ref;
    OBJ_INDEX_DATA *ammo;     // Required ammo object (NULL = unlimited)
    int ammo_per_shot;        // Ammo consumed per volley

    long flags;               // MODULE_REQUIRES_AMMO, MODULE_PASSIVE, etc.
};
```

### Struct: `SHIP_MODULE` (installed instance — on runtime `SHIP_DATA`)

```c
struct ship_module {
    SHIP_MODULE *next;
    bool valid;

    SHIP_MODULE_INDEX *index; // Template
    int16_t slot_id;          // Which hardpoint it is installed in

    int condition;            // Current HP (degrades in combat)
    int max_condition;        // Initial = 100 (tunable by quality)

    int ammo_count;           // Current ammo remaining
    int reload_countdown;     // Ticks until ready to fire again

    LLIST *assigned_crew;     // Crew members operating this module
    bool active;              // Enabled/disabled by player
    bool operational;         // Derived: enough qualified crew + condition > 0
};
```

### Additions to `SHIP_INDEX_DATA`

```c
LLIST *hardpoints;            // List of SHIP_HARDPOINT_DEF
int max_module_weight;        // Total weight budget across all modules
```

### Additions to `SHIP_DATA`

```c
LLIST *modules;               // List of installed SHIP_MODULE
int total_module_weight;      // Cached sum: recalculated on install/remove
```

### Stat Recalculation

When modules or module state changes, `ship_recalc_modules(ship)` recomputes
derived stats from base template values plus all active module bonuses:

```
effective_stat = base_template_stat + SUM(active_module.bonus)
```

Modules with `condition <= 0` or `operational == false` do not contribute.

### Module Operational Check

```
operational = active
           && condition > 0
           && assigned_crew count >= operators
           && each assigned crew meets minimum skill thresholds
```

Crew skill checks use the six `SHIP_CREW_DATA` fields already defined:
`scouting`, `gunning`, `oarring`, `mechanics`, `navigation`, `leadership`.

Operating a module triggers `crew_skill_improve()` for the relevant skill on
each assigned crew member (existing diminishing-returns system).

---

## B) Ship Combat System

### Combat Flow

1. **Target Acquisition** (`ship aim <target>`)
   - Player at helm selects target ship or character
   - Validates: in range (max weapon range across operational weapon modules),
     not in safe zone, enough crew, at least one operational weapon
   - Sets `ship_attacked` / `char_attacked`, `attack_position = LOADING`

2. **Loading Phase** (per weapon module, in `ship_pulse_update`)
   - Each weapon module counts down `reload_countdown` independently
   - Modified by assigned crew `gunning` skill (higher = faster reload)
   - When countdown reaches 0, module transitions to READY

3. **Firing Phase** (automatic when READY)
   - Each ready weapon fires at target
   - Calculate hit chance: base 70% ± range modifier ± crew gunning ± weather
   - On hit: calculate damage from module `damage` stat
   - Consume ammo if `MODULE_REQUIRES_AMMO`
   - Trigger `crew_skill_improve()` for gunning on all assigned crew
   - Reset `reload_countdown` to `reload_time`

4. **Damage Resolution** (`boat_damage()`)
   - Incoming damage reduced by target armor
   - Primary hull HP loss
   - Random chance to hit installed modules (condition damage)
   - Anti-crew weapons: chance to injure/kill crew members
   - Fire damage: ongoing DoT, requires mechanics crew to extinguish
   - Threshold effects at 75%/50%/25% hull HP:
     - 75%: minor flooding/smoke (cosmetic + minor speed penalty)
     - 50%: propulsion damage (speed halved), fires
     - 25%: critical — crew casualties, module failures, listing
   - 0%: sinking/crashing/breaking down (domain-appropriate destruction)

5. **Destruction Sequence**
   - Aquatic: 5-tick sinking countdown, cargo floats, crew can abandon
   - Aerial: crash sequence, wreckage field on ground, explosion damage
   - Terrestrial: breakdown, wreckage remains, lootable

### Ship Chase (`ship chase <target>`)

- Auto-pilot toward target ship, matching heading
- Speed advantage determines if chaser closes or target escapes
- Disengage after target exceeds max weapon range × 3
- NPC ships use this in CHASING state

### Disengagement

- `ship aim stop` ceases fire
- Moving out of max range auto-disengages
- Entering a safe harbor disengages

### Combat Constants

```c
#define SHIP_COMBAT_HIT_BASE       70   // Base hit chance %
#define SHIP_COMBAT_RANGE_PENALTY   5   // -% per tile beyond half max range
#define SHIP_COMBAT_SKILL_BONUS     3   // +% per gunning skill point
#define SHIP_DAMAGE_THRESHOLD_MINOR 75  // % HP
#define SHIP_DAMAGE_THRESHOLD_MAJOR 50
#define SHIP_DAMAGE_THRESHOLD_CRIT  25
#define SHIP_MODULE_HIT_CHANCE     15   // % chance per hit to damage a module
#define SHIP_CREW_HIT_CHANCE        5   // % chance per anti-crew hit to injure
#define SHIP_SINK_COUNTDOWN         5   // Ticks to sink after reaching 0 HP
```

### Example Weapon Modules

| Module          | Type    | Size   | Dmg | Range | Reload | Crew | Gunning | Domain   |
|-----------------|---------|--------|-----|-------|--------|------|---------|----------|
| Light Cannon    | Weapon  | Small  |  50 |     3 |      4 |    1 |       1 | Aquatic  |
| Heavy Cannon    | Weapon  | Medium | 120 |     5 |      8 |    2 |       3 | Aquatic  |
| Fire Thrower    | Weapon  | Small  |  30 |     2 |      3 |    1 |       2 | All      |
| Ballista        | Weapon  | Medium |  80 |     8 |     10 |    1 |       2 | All      |
| Mortar          | Weapon  | Large  | 200 |    12 |     15 |    3 |       4 | Aq+Land  |
| Air Bombard     | Weapon  | Large  | 180 |     6 |     12 |    2 |       4 | Aerial   |
| Battering Ram   | Weapon  | Large  | 300 |     1 |     20 |    0 |       0 | Land     |
| Harpoon Launcher| Weapon  | Medium | 60  |     6 |      6 |    1 |       2 | All      |

### Example Defense Modules

| Module             | Type    | Size   | Armor | HP   | Special              | Domain |
|--------------------|---------|--------|-------|------|----------------------|--------|
| Iron Plating       | Defense | Medium |   +50 | +500 | Speed -10%           | All    |
| Reinforced Hull    | Defense | Small  |   +20 | +200 | —                    | All    |
| Anti-Boarding Nets | Defense | Small  |     — |    — | +50% boarding def    | Aq+Air |
| Magic Ward         | Defense | Large  |   +30 |    — | Fire resist 50%      | All    |
| Spiked Bulwark     | Defense | Medium |   +15 | +100 | Boarding dmg reflect | Land   |

### Example Utility Modules

| Module           | Type    | Size   | Effect                        | Crew | Skill      | Domain |
|------------------|---------|--------|-------------------------------|------|------------|--------|
| Expanded Hold    | Utility | Medium | +2000 weight, +20 capacity    |    0 | —          | All    |
| Crew Quarters    | Utility | Small  | +4 crew capacity              |    0 | —          | All    |
| Spyglass Array   | Utility | Small  | +3 view range                 |    1 | Scouting 3 | All    |
| Advanced Sextant | Utility | Small  | Navigation accuracy +50%      |    1 | Nav 4      | Aq+Air |
| Repair Station   | Utility | Medium | Passive module repair per tick |    1 | Mech 3     | All    |
| Signal Flags     | Utility | Small  | Fleet command range +50%      |    1 | Lead 3     | All    |

### Example Propulsion Modules

| Module           | Type       | Size   | Effect               | Crew | Skill      | Domain |
|------------------|------------|--------|----------------------|------|------------|--------|
| War Sails        | Propulsion | Large  | Speed +30%           |    2 | Oarring 2  | Aquatic|
| Oar Bank         | Propulsion | Medium | Oar power +40%       |    4 | Oarring 1  | Aquatic|
| Arcane Engine    | Propulsion | Large  | Speed +50%, no wind  |    1 | Mech 4     | Air    |
| Draft Team       | Propulsion | Large  | Speed +25%           |    1 | Oarring 2  | Land   |
| Emergency Sails  | Propulsion | Small  | Backup if main fails |    1 | Oarring 1  | Aq+Air |

---

## C) NPC Ships with Crew and Captains

### Captain System

- `NPC_SHIP_INDEX_DATA` already has a `MOB_INDEX_DATA *captain` field
- On NPC ship spawn, load the captain mob into the helm room
- Captain mob stats drive:
  - Combat aggression threshold (level-based)
  - Flee threshold (HP percent based on captain's wimpy/level)
  - Targeting priority (based on `npc_type`: pirates target traders, coast
    guard targets pirates)
- Captain's `MOB_REPUTATION_DATA` entries define faction alignment

### Crew Auto-Population

When creating an NPC ship:

1. Determine crew count: random between `min_crew` and `max_crew`
2. Generate crew members from `SHIP_CREW_INDEX_DATA` templates
3. Create NPC mobs placed in appropriate ship rooms (deck, below deck, etc.)
4. Auto-assign to modules by best-fit:
   - Sort modules by highest skill requirement descending
   - For each module, assign crew with highest matching skill
   - Greedy algorithm ensures critical stations staffed first

### NPC Ship State Machine

Activate `npc_ship_state_update()` in the pulse cycle.  State transitions:

```
STOPPED → SAILING (has route/goal)
SAILING → CHASING (hostile detected in scan range)
CHASING → ATTACKING (target in weapon range)
ATTACKING → BOARDING (target HP < 25% and has marines)
ATTACKING → CHASING (target moves out of range)
ATTACKING → FLEEING (own HP < flee threshold)
BOARDING → SAILING (boarding complete or target destroyed)
FLEEING → SAILING (out of threat range, cooldown expired)
Any → STOPPED (route complete, docked, or disabled)
```

### NPC Targeting Rules by Type

| NPC Type       | Attacks                          | Flees From        |
|----------------|----------------------------------|-------------------|
| Coast Guard    | Pirates, hostiles                | Nothing (brave)   |
| Pirate         | Traders, adventurers, weak ships | Coast Guard       |
| Bounty Hunter  | Wanted players, pirates          | Superior force    |
| Trader         | Nothing (non-combatant)          | Any attacker      |
| Escort         | Anything attacking escortee      | Superior force    |

---

## D) Reputation Integration

### Captain-Based Faction Resolution

Every NPC ship has a captain with `MOB_REPUTATION_DATA` entries.  Ship combat
reputation changes use the captain's faction data — mechanically identical to
killing a faction-aligned mob on land.

### Ship Faction Reference

Add to `NPC_SHIP_INDEX_DATA`:

```c
REPUTATION_INDEX_DATA *faction;     // Primary faction allegiance
int16_t faction_rank;               // Ship's effective rank in that faction
```

### Reputation Change Events

| Event                                  | Rep Effect                         |
|----------------------------------------|------------------------------------|
| Attack a faction's ship                | Negative with that faction         |
| Sink a faction's ship                  | Large negative with that faction   |
| Sink a faction's enemy (in patrol view)| Positive with observing faction    |
| Board and plunder                      | Negative with victim's faction     |
| Escort/defend a faction ship           | Positive with that faction         |
| Destroy pirate near coast guard        | Positive with coast guard faction  |

### Pirate Status Rework

Replace legacy `pcdata->rank[3]` / `IS_PIRATE()` with proper reputation:

- Create pirate reputation factions per continent
- Coast Guard proximity detection (existing distance code in `do_ship_aim`)
  triggers reputation decreases when attacking non-hostile ships
- Coast Guard aggression toward players at `REPUTATION_RANK_HOSTILE` threshold
- Pirate reputation ranks progress: Petty Thief → Buccaneer → Corsair →
  Dread Pirate

---

## E) `ship_has_enough_crew()` Implementation

Currently always returns `true`.  Wire to actually check:

```c
bool ship_has_enough_crew(SHIP_DATA *ship) {
    if (!IS_VALID(ship) || !ship->index) return false;
    if (ship->index->min_crew <= 0) return true;
    return list_size(ship->crew) >= ship->index->min_crew;
}
```

---

## F) Player Commands

### New/Modified Ship Subcommands

| Command                          | Action                              |
|----------------------------------|-------------------------------------|
| `ship modules`                   | List installed modules with status  |
| `ship install <item> <slot#>`    | Install module (requires port)      |
| `ship uninstall <slot#>`         | Remove module (requires port)       |
| `ship repair <slot#>`            | Repair damaged module (crew action) |
| `ship aim <target>`              | Begin targeting (reactivated)       |
| `ship aim stop`                  | Cease fire                          |
| `ship chase <target>`            | Pursue target ship                  |
| `ship crew assign <crew> <slot>` | Assign crew to module               |
| `ship crew unassign <crew> <slot>`| Remove crew from module            |
| `ship crew roster`               | Full crew + module assignment view  |
| `ship status`                    | Hull HP, module conditions, combat  |

### OLC Commands (SHEdit)

| Command                            | Action                            |
|------------------------------------|-----------------------------------|
| `hardpoint add <type> <size> <name>`| Add hardpoint slot to template   |
| `hardpoint remove <#>`             | Remove hardpoint slot             |
| `hardpoint list`                   | Show all hardpoint definitions    |
| `moduleweight <value>`             | Set total module weight budget    |

### Module Template Editor (SMEdit — new)

Separate OLC editor for `SHIP_MODULE_INDEX` templates:

| Command     | Action                              |
|-------------|-------------------------------------|
| `name`      | Set module name                     |
| `desc`      | Set module description              |
| `type`      | Set hardpoint type                  |
| `size`      | Set module size                     |
| `domain`    | Set domain compatibility flags      |
| `weight`    | Set module weight                   |
| `stats`     | Set stat bonuses                    |
| `weapon`    | Set weapon stats (dmg/range/reload) |
| `crew`      | Set crew requirements and skills    |
| `ammo`      | Set ammo object reference           |
| `flags`     | Set module flags                    |
| `show`      | Display module template             |

---

## G) Persistence

### Ship Template (`ships.dat`)

Add to `save_ship_index` / `load_ship_index`:

```
ModuleWeight 5000
#HARDPOINT 1
Name Port Cannon Bay~
Type 0
Size 2
Domain 1
Flags 0
#-HARDPOINT
```

### Module Templates (`ship_modules.dat` — new file)

Same pattern as `ships.dat`:

```
#MODULE 1
Name Light Cannon~
Description A standard naval cannon.~
Type 0
Size 1
Weight 200
Domain 1
Damage 50
Range 3
ReloadTime 4
Operators 1
ReqGunning 1
Flags 0
#-MODULE
```

### Runtime Ship (JSON persist)

Add to `ship_to_json` / `json_to_ship`:

```json
{
  "modules": [
    {
      "module_vnum": 1,
      "slot_id": 1,
      "condition": 85,
      "max_condition": 100,
      "ammo_count": 20,
      "active": true,
      "assigned_crew": [3, 7]
    }
  ]
}
```

---

## Implementation Phases

### Phase 1 — Data Model (structs, memory, persistence)

1. Define structs in `merc.h`: `SHIP_HARDPOINT_DEF`, `SHIP_MODULE_INDEX`,
   `SHIP_MODULE`; constants for types/sizes/domains/flags
2. Add `new_*` / `free_*` to `mem.c`
3. Extend `SHIP_INDEX_DATA` with `hardpoints` list and `max_module_weight`
4. Extend `SHIP_DATA` with `modules` list and `total_module_weight`
5. Wire hardpoints into `load_ship_index` / `save_ship_index`
6. Create `load_ship_modules` / `save_ship_modules` for module templates
7. Wire modules into `ship_to_json` / `json_to_ship`
8. Add `SHIP_LAND_VESSEL` class and `DOMAIN_*` flags
9. Update both `CMakeLists.txt` and `Makefile` if new `.c` files added

### Phase 2 — OLC and Player Commands

10. Add `hardpoint` and `moduleweight` commands to `shedit`
11. Create `smedit` (ship module editor) for module templates
12. Add `ship modules`, `ship install`, `ship uninstall`, `ship repair` commands
13. Extend `ship crew` with `assign`, `unassign`, `roster` subcommands
14. Implement `ship_recalc_modules()` and call on module state changes
15. Implement `ship_module_is_operational()` crew skill checks

### Phase 3 — Combat

16. Implement `boat_damage()` with module-aware damage distribution
17. Reactivate `do_ship_aim()` with module-based range/targeting
18. Implement fire/reload cycle in `ship_pulse_update()`
19. Implement `do_ship_chase()` pursuit mechanics
20. Wire `ship_has_enough_crew()` to actual crew count
21. Add destruction/sinking sequence (domain-appropriate)
22. Add threshold effects (flooding, fires, propulsion damage)
23. Add `ship status` command for combat readout

### Phase 4 — NPC Ships

24. Implement NPC ship creation with auto-generated crew and captain
25. Implement crew auto-assignment to modules (best-fit algorithm)
26. Activate `npc_ship_state_update()` in `update_handler()`
27. Implement NPC combat AI state machine transitions
28. Add NPC targeting rules by ship type
29. Create NPC ship editor (placeholder at bottom of `boat.c`)

### Phase 5 — Reputation

30. Add faction references to NPC ship templates
31. Hook ship combat into reputation system via captain's `MOB_REPUTATION_DATA`
32. Implement coast guard proximity detection for pirate flagging
33. Plan migration path from legacy `pcdata->rank[]` to reputation factions

---

## File Impact Summary

| File                          | Changes                                    |
|-------------------------------|--------------------------------------------|
| `merc.h`                      | New structs, constants, field additions     |
| `mem.c`                       | new_/free_ for hardpoint_def, module_index, module |
| `boat.c`                      | Combat, chase, crew, modules, NPC AI        |
| `tables.c`                    | New type tables, ship_class expansion       |
| `update.c`                    | Wire npc_ship_state_update into pulse       |
| `editors/ships/shedit.c`      | Hardpoint + moduleweight OLC commands       |
| `editors/ships/smedit.c`      | New: module template editor                 |
| `io/json/json_instance.c`     | Module serialization in ship JSON           |
| `interp.c` / `interp.h`      | New command registrations                   |
| `CMakeLists.txt` / `Makefile` | New source files                            |

---

## Risks

| Risk                             | Mitigation                                  |
|----------------------------------|---------------------------------------------|
| Module stat stacking exploits    | Weight budget hard cap, slot limits          |
| NPC combat AI causing lag        | Throttle AI ticks, region-based activation   |
| Complexity overload for players  | Sensible defaults, `ship status` summary     |
| Legacy save format breaks        | Version check on load, graceful fallback     |
| Domain differences cause bugs    | Domain flag validation on install, tested per class |

---

## Success Criteria

- Players can install/remove modules at ports with visible stat effects
- Ship combat produces tactical decisions (weapon choice, positioning, crew)
- NPC ships fight, flee, and chase with believable behavior
- Sinking a faction ship produces measurable reputation consequences
- All mechanics work for sailing boats, airships, and land vessels
- Build compiles clean; test suite passes
