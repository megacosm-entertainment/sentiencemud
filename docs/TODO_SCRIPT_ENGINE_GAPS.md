# Script Engine Gap Analysis

Date: 2026-07-13
Source: Phase 1 extraction artifacts (commands, triggers, ifchecks, variables)
Related: PLAN_SCRIPT_AUDIT_CONSOLIDATED.md, TODO_SCRIPT_ENTITY_PARITY_GAPS.md

This document identifies gaps in the scripting engine itself — missing features,
inconsistencies, and potential improvements discovered during the Phase 1 script
documentation extraction. It does NOT duplicate items already tracked in the
consolidated audit plan or the entity parity gaps doc.

---

## 1. Area / Instance / Dungeon Trigger Starvation

Area, instance, and dungeon entities have critically few triggers compared to
core types. This severely limits what builders can script for these entities.

| Entity   | Triggers | Commands | Assessment                   |
|----------|----------|----------|------------------------------|
| Mob      | 153      | 167      | Comprehensive                |
| Token    | 206      | 163      | Comprehensive (claimed)      |
| Object   | 100      | 162      | Good                         |
| Room     | 71       | 157      | Moderate                     |
| Area     | 6        | 44       | **Severely underserved**     |
| Instance | 7        | 48       | **Severely underserved**     |
| Dungeon  | 9        | 48       | **Limited**                  |
| Event    | 6        | 44       | **Barely functional**        |
| Quest    | 12       | 44       | Isolated (see §6)           |

### Missing Area Triggers
- [ ] `player_enter` / `player_leave` — react to population changes
- [ ] `player_death` — area-wide death handling
- [ ] `combat_start` / `combat_end` — area combat awareness
- [ ] `weather_change` — environmental scripting
- [ ] `population_threshold` — spawn control based on player count
- [ ] `area_unlock` / `area_lock` — access state change hooks

### Missing Instance Triggers
- [ ] `player_joined` / `player_left` — party management
- [ ] `player_died` — wipe detection, respawn logic
- [ ] `objective_complete` — progression hooks
- [ ] `difficulty_adjust` — scaling triggers
- [ ] `instance_timer` — timed content
- [ ] `cleanup` — instance teardown logic

### Missing Dungeon Triggers
- [ ] `floor_completed` — per-floor progression
- [ ] `boss_defeated` — encounter phase transitions
- [ ] `secret_discovered` — exploration tracking
- [ ] `party_wiped` — failure state handling
- [ ] `time_expired` — timed dungeon runs

### Missing Event Triggers
Event entity has only system-level triggers (tick, random, reset, moon,
prereckoning, reckoning). It lacks any event-specific lifecycle hooks:
- [ ] `event_start` / `event_end` — event lifecycle
- [ ] `player_joined_event` / `player_left_event`
- [ ] `event_objective_met` — milestone triggers
- [ ] `event_phase_change` — phase transition hooks

---

## 2. Room Combat Trigger Gap

Rooms support death triggers but lack all 24 combat-specific triggers that
exist on mob/token. This prevents environmental combat scripting (arena hazards,
trapped rooms, magical wards that react to specific attack types).

### Attack Subtriggers Missing from Room (19)
- [ ] `attack`, `attack_backstab`, `attack_bash`, `attack_behead`, `attack_bite`
- [ ] `attack_blackjack`, `attack_circle`, `attack_counter`, `attack_cripple`
- [ ] `attack_dirtkick`, `attack_disarm`, `attack_intimidate`, `attack_kick`
- [ ] `attack_rend`, `attack_slit`, `attack_smite`, `attack_tailkick`
- [ ] `attack_trample`, `attack_turn`

### Combat Skill Triggers Missing from Room (5)
- [ ] `hit`, `hitgain`, `skill_berserk`, `skill_sneak`, `skill_warcry`

### Pre-action Combat Triggers Missing from Room
- [ ] `predeath` — room-level death prevention (sanctuary rooms)
- [ ] `preflee` — room-level flee prevention (arena lockdown)
- [ ] `prekill` — room-level kill prevention (safe zones)

**Use cases:** Combat arenas with environmental hazards, rooms that react to
specific fighting styles, magical wards that trigger on attack types, boss
encounter rooms with phase-based mechanics.

---

## 3. Token Flexibility Gaps

Token is positioned as the most flexible entity type (206 triggers) but is
missing several triggers that exist on other core types:

### Missing from Token
- [ ] `blow` — object detonation (obj-only)
- [ ] `catalyst` / `catalystfull` / `catalystsrc` — alchemical system (obj-only)
- [ ] `extract` — entity extraction (mob, obj, room but NOT token)

### Suspicious Token-Exclusive Triggers (Need Audit)
These 9 triggers exist ONLY on token with unclear purpose or scope:
- `caninterrupt` — boolean gate trigger, unclear semantics
- `combatstyle` — why token-only? combat styles are mob-centric
- `emoteself` / `verbself` — self-targeted social triggers, purpose unclear
- `interrupt` — should be broader than token-only
- `spellbeat` / `spellinter` / `spellpenetrate` — spell subsystem on tokens only
- `stripaffect` — too generic for token-only scope

**Action:** Audit each to determine if scope should be expanded or if they are
intentionally token-specific for the buff/debuff token pattern.

---

## 4. Command Entity Type Gaps

### Commands Missing from Token
Token lacks several commands available to mob/obj/room:
- [ ] `at` — execute at location (mob, obj, room have it)
- [ ] `cancel` / `delay` — timer management (mob, obj, room have it)
- [ ] `cast` — spell casting (mob, obj have it)
- [ ] `scriptwait` — pause execution (mob, obj, token have it but NOT room)

### Commands Missing from Area (Available in Instance/Dungeon)
- [ ] `questechoat` — quest messaging (in instance, dungeon but NOT area)
- [ ] `wildsanchor` / `wildsvlink` — wilderness management (mob-only currently)
- [ ] `makeinstanced` — instance scoping (instance/dungeon only)
- [ ] `stringmob` / `stringobj` — description modification (dungeon only)

### Visibility Commands Too Restricted
`appear`, `disappear`, `teleport` — only available to mob/obj, should logically
also be available to room (room visibility toggling) and area (mass teleport).

### Trait Commands Exclude Mob
`addtrait`, `adjusttrait`, `settrait`, `removetrait` — NOT available to mob
programs. Mobs should be able to modify their own or others' traits.

---

## 5. Undocumented Ifchecks (107 New)

Phase 1 extraction found 107 ifchecks in source code not present in existing
documentation. These need documentation and in some cases design review.

### By Category

| Category                 | Count | Examples                                          |
|--------------------------|-------|---------------------------------------------------|
| Class/Skill system       | 14    | `availskill`, `hasclass`, `classlevel`, `classcount` |
| Object type checks       | 11    | `isbook`, `iscontainer`, `isfood`, `isportal`      |
| Object values            | 10    | `objval0` through `objval9`                        |
| Flag lookups             | 12    | `flagcontainer`, `flagexit`, `flagroom`, etc.      |
| Event system             | 10    | `eventactive`, `eventphase`, `eventkills`, etc.    |
| Quest progression        | 7     | `questactive`, `questcomplete`, `queststage`       |
| Mission/PK stats         | 8     | `onmission`, `totalpkwins`, `savage`               |
| Temp storage             | 6     | `tempstore1`–`tempstore5`, `timer`                 |
| Traits                   | 3     | `traitbool`, `traitint`, `traitstring`             |
| Object repairs           | 2     | `objrepairs`, `objmaxrepairs`                      |
| Alignment/grouping       | 3     | `clan`, `haschurch`, `inchurch`                    |
| Race checks              | 2     | `racepath`, `raceremort`                           |
| Misc (validity, auras)   | 19    | `exists`, `isvalid`, `hasaura`, `isprog`, etc.     |

### Ifchecks With No Prog Type Support
These are registered but work in NO program types — likely unfinished:
- [ ] `clan` — clan membership check (no prog types)
- [ ] `hasship` — ship ownership check (no prog types)
- [ ] `exists` — variable existence check (no prog types)

**Action:** Either implement for all prog types or remove.

### Removed Ifchecks (3)
Found in docs but NOT in source — confirm intentional removal:
- `dawn`, `str`, `xcall`

---

## 6. Quest System Isolation

The quest scripting system (12 triggers, 44 commands) operates in complete
isolation from other entity types. There are NO cross-entity communication
mechanisms.

### Impact
Quest scripts CANNOT:
- Signal mob AI to start encounters on stage transitions
- Trigger area/room effects on quest completion
- Broadcast progress to other players
- Coordinate with token/object scripts for quest items

### Suggested Cross-Entity Triggers
- [ ] `quest_signal` — emit signal to mob/obj/room/token in area
- [ ] `quest_progress` — broadcast progress updates
- [ ] `quest_objective_signal` — notify entities of objective completion

---

## 7. Ifcheck Design Issues

### Redundant/Ambiguous Pairs
These pairs have unclear semantic distinction:
- [ ] `aura` vs `hasaura` — both return T/F for "entity has named aura"
- [ ] `availskill` vs `hasskill` — availability vs possession unclear
- [ ] `availspell` vs `hasspell` — same ambiguity
- [ ] `ismobile` vs `isnpc` — documented as alias, should remove one

**Action:** Clarify semantics or consolidate.

### Candidates for Parametric Consolidation
Multiple numbered ifchecks could be single parametric functions:
- `objval0`–`objval9` (10 ifchecks) → `objval(entity, index)`
- `tempstore1`–`tempstore5` (5 ifchecks) → `tempstore(entity, index)`
- 12 `flag*` lookups → `flaglookup(flagtype, flagname)`

**Benefit:** Reduces ifcheck table bloat, simplifies documentation, enables
extending value ranges without adding new ifchecks.

### Return Type Inconsistencies
- `flagextra` returns NUM but `objextra` returns T/F — same concept, different types
- Quest ifchecks mix T/F (`questactive`) and NUM (`questcompletions`) without
  consistent naming to indicate which

---

## 8. Variable System Gaps

### Entity Types With Minimal Field Access

| Entity  | Accessible Fields | Gap Level   |
|---------|-------------------|-------------|
| Mobile  | 90+               | Complete    |
| Object  | 40+               | Moderate    |
| Room    | 30+               | Underserved |
| Token   | 12+               | **Minimal** |
| Exit    | 10+               | **Minimal** |
| Area    | Unknown           | **Unknown** |

### Critical Missing Variable Access
Builders cannot read or set these via script variable expansion:
- [ ] Combat stats: `hp`, `maxhp`, `mana`, `maxmana`, `move`, `maxmove`, `exp`
- [ ] Entity identifiers: `name`, `short`, `long` (not in expansion layer)
- [ ] Object properties: durability, container capacity, decay timer
- [ ] Room properties: light level, population counts
- [ ] Area properties: level range, PvP status, player count

### Owner Field Inconsistency
`owner` returns **string** on Mobile/Object but **MOBILE entity** on Token.
Should standardize to one type or use distinct field names.

### Missing String Manipulation
Only `strlen` and `strprefix` exist. No `substr`, `trim`, `lowercase`,
`replace`. This forces builders to work around missing string operations.

### Stub Variable Types (Likely Incomplete)
These variable types exist in enum but lack clear expansion handlers:
- `VAR_TRAINER_ENTRY` — no documented access pattern
- `VAR_BLUEPRINT_SECTION` — limited integration
- `ENTITY_VAR_EVENT` — cast type exists, unclear semantics

---

## 9. Suggested New Features

### High Value
- [ ] **Cross-entity signaling** — triggers that let quest/area/instance scripts
      communicate with mob/obj/room/token scripts in their scope
- [ ] **Area lifecycle triggers** — enable area-level scripting for weather,
      population management, and dynamic events
- [ ] **Room combat awareness** — environmental combat scripting via room
      attack/skill triggers
- [ ] **Parametric ifchecks** — reduce 27+ numbered ifchecks to 3 parametric ones

### Medium Value
- [ ] **String manipulation ifchecks** — `substr`, `lowercase`, `replace`
- [ ] **Variable expansion for combat stats** — read hp/mana/move in messages
- [ ] **Entity name access in expansion** — `$n(name)`, `$n(short)`, etc.
- [ ] **Iterator ifchecks** — count/enumerate items in inventory, room occupants
- [ ] **Distance/range checks** — spatial queries between entities

### Lower Value
- [ ] **Math ifchecks** — `mod`, `pow`, `sqrt` (only basic arithmetic exists)
- [ ] **Cooldown/timer queries** — check ability recharge status
- [ ] **Equipment slot queries** — get item at specific wear location

---

## Summary

| Category                        | Gap Count | Priority |
|---------------------------------|-----------|----------|
| Area/Instance/Dungeon triggers  | 25+ needed| High     |
| Room combat triggers            | 24 missing| High     |
| Undocumented ifchecks           | 107       | High     |
| Command entity type gaps        | 15+ items | Medium   |
| Quest system isolation          | 3 needed  | Medium   |
| Token trigger gaps              | 5 missing | Medium   |
| Ifcheck design issues           | 4 pairs   | Medium   |
| Variable system gaps            | 10+ items | Medium   |
| Suggested new features          | 15+ items | Varies   |
