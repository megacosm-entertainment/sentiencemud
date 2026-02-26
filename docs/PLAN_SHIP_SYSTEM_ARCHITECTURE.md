# Plan: Ship System Architecture (Net-New Completion Track)

**Date:** February 24, 2026
**Status:** New Systems Design / Directional Plan

---

## Purpose

Define a complete ship system architecture for Sentience, including:

- world-map ship movement representation
- instanced ship interiors
- crew systems and progression
- ship combat and boarding
- following, convoy, and fleet mechanics
- integration with wilderness and weather simulation

Scope note:

- This architecture includes the existing goblin airship travel path as a first-class transport mode.
- Land travel actors (trade caravans, wagons, teams) should integrate through the same wilderness/runtime contracts, with legacy yoke/cart mechanics from `src_20_dev` treated as backport candidates.

This plan complements `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md` and treats ship completion as a first-class systems program.

---

## Current Context

Existing ship functionality appears partially implemented (movement/navigation and some player-facing behaviors), but key pillars are incomplete:

- ship combat remains underdeveloped
- following/convoy/fleet systems are missing or incomplete
- NPC ship behavior depth is limited
- interior/world synchronization rules are not formalized as a stable contract

Alignment snapshot (2026-02-24):

- Implemented (foundation): wilderness-coordinate storm actors with persistence and region-aware weighting.
- Implemented (foundation): weather-to-ship baseline coupling (storm-severity movement penalty and drift events).
- Implemented (foundation): weather coupling now includes airship movement/drift tuning.
- Implemented (foundation): area-region weather profile controls (`aedit regions weather`) with JSON persistence.
- Pending (core architecture): explicit world/interior sync state machine, fleet entity model, and NPC ship ecosystem loops.

Alignment update (2026-02-25):

- Implemented (projection clarity): wilderness room/exits/location displays now prefer wilderness map names and terrain shownames over generic UID tuples, improving world/interior context readability for transport operations and staff diagnostics.
- Implemented (ops readability): staff index/reset listings now prefer `list_name` for spawned entities, reducing ambiguity during ship/weather-adjacent tuning and troubleshooting.

---

## Core Design Principles

1. **Single Ship Entity, Multiple Projections**
   - One authoritative ship actor in simulation space, projected to:
     - world-map presence
     - interior instance state
     - nearby-room/player-facing descriptions

2. **Persistent Ship State**
   - Ship state should not depend on loaded vroom lifetime.

3. **Crew as Functional Subsystem**
   - Crew should be modeled by role capabilities and readiness, not cosmetic staffing only.

4. **Composable Group Navigation**
   - Following/convoy/fleet should be deterministic and tunable, not ad-hoc per-command behavior.

5. **Weather and Wilderness Native**
   - Ship behavior must consume weather and wilderness simulation outputs directly.

---

## System Model

## A) Ship Actor Runtime Model

Authoritative ship actor fields (conceptual):

- identity/faction/ownership
- hull state, sail/rig state, engine/oar state (if applicable)
- heading, velocity, maneuver profile
- cargo/passenger/crew manifests
- tactical state (peaceful, alert, engaged, fleeing, disabled)
- interior instance reference (or interior state handle)

## B) Interior Instancing Contract

Define explicit synchronization contract between world actor and interior instance:

- **Always-authoritative world actor** for position, travel state, and encounter state.
- **Interior instance** holds onboard room/object/NPC state.
- Sync boundaries:
  - movement tick -> interior context updates (e.g., sea state, motion effects)
  - combat tick -> interior damage/status propagation
  - docking/boarding transitions -> interior/world entrypoint remap

### Required Rules

- Boarding state transitions are atomic.
- Interior persistence survives temporary unloads.
- World destruction/disable states are reflected interior-side safely.

## C) Crew System

Crew roles (baseline):

- captain/command
- navigator
- helmsman/steersman
- sailmaster/engineer/oars lead
- gunners/weapon operators
- deckhands/repair
- marines/boarding

Crew model features:

- readiness/fatigue/morale
- skill ratings by role
- role assignment and redundancy
- injury/attrition impact
- NPC and player-crew coexistence

## D) Ship Combat (Naval + Boarding)

Combat layers:

1. **Maneuver layer** (positioning, heading control, range bands)
2. **Exchange layer** (ranged weapons, special actions, repairs, morale shocks)
3. **Boarding layer** (grapple/repel/boarding action transitions)

Design intent:

- naval positioning matters before raw damage race
- boarding is a tactical escalation, not mandatory immediate state
- weather and sea conditions influence options and risk

## E) Following, Convoy, and Fleet Mechanics

Define formation-driven group movement:

- leader-follower link rules
- convoy spacing and error tolerance
- fleet command intents (hold, screen, flank, pursue, disengage)
- collision/deconfliction and drift behavior under weather stress

### Fleet Entity (Conceptual)

- fleet id
- flagship/command source
- member ships
- formation profile
- shared intent and command latency model

## F) NPC Ship Behavior

NPC ship AI profiles:

- trader
- patrol/naval guard
- pirate/raider
- escort/support
- scripted mission actor

Behavior consumes:

- faction goals
- threat map
- weather risk
- route value
- fleet context

---

## Dependencies

Hard dependencies:

- wilderness simulation/persistence contract
- area-native region integration for route/risk semantics
- weather actor outputs
- encounter/event framework for wilderness actors

Reference contract:

- `PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md`

Soft dependencies:

- class/party systems (for marine/crew role expression)
- crafting/gathering/exploration outputs (ship supplies, repairs, route intel)

## Three-Doc Execution Contract

This architecture plan is coupled to:

- `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`
- `PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md`

Execution guardrails:

- Ship architecture changes must not bypass wilderness runtime/storage authority.
- Weather/NPC transport implementation slices should land in the weather plan but remain architecture-compliant here.
- Any change that alters actor authority, fleet semantics, or interior/world sync assumptions requires synchronized updates in all three plans.

Ownership boundaries:

- **This plan:** transport architecture and mission-level semantics (combat, convoy/fleet, actor/interior authority).
- **Weather/NPC plan:** incremental behavior delivery and tuning on top of architecture.
- **Wilderness plan:** runtime substrate, persistence order, and simulation lifecycle constraints.

---

## Delivery Phases

## Phase 0 - Runtime Contract and Data Model

- Define ship actor schema and interior sync contract.
- Define persistence ownership boundaries.

## Phase 1 - Interior/World Sync Stabilization

- Formalize transitions (dock, board, disengage, sink/disable).
- Validate interior persistence and state propagation.

## Phase 2 - Crew System Activation

- Implement role assignments, readiness, and role-quality effects.
- Add baseline AI crew behavior and command tools.

## Phase 3 - Naval Combat Core

- Implement maneuver + exchange layers.
- Add environmental modifiers (weather/sea state).

## Phase 4 - Boarding and Damage States

- Add boarding transitions and interior combat linkages.
- Add disable/retreat/capture outcomes.

## Phase 5 - Following/Convoy/Fleet

- Implement fleet entity and formation movement.
- Add command intents and group tactical behaviors.

## Phase 6 - NPC Ship Ecosystem

- Add NPC ship populations, route economies, and faction conflict loops.
- Integrate with weather windows and wilderness hotspots.

---

## Risks and Mitigations

### Risk: World/interior desync bugs

Mitigation:

- Explicit state machine with transition guards and reconciliation checks.

### Risk: Naval combat becomes opaque

Mitigation:

- Expose range/heading/crew-readiness feedback clearly to players.

### Risk: Fleet mechanics become micro-heavy

Mitigation:

- Support intent-level commands and automation tiers.

### Risk: Performance degradation with many active ships

Mitigation:

- Region-based simulation throttling and actor LOD policies.

---

## Success Criteria

- Ship actor and interior states remain coherent across all transitions.
- Crew quality and assignments materially affect outcomes.
- Naval combat is tactical and readable, not pure stat race.
- Fleet/convoy mechanics are usable and reliable.
- NPC ships create persistent world dynamism without instability.

---

## Program Dependency

This plan is governed by:

- `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`
- `PLAN_CORE_SYSTEMS_INTEGRATION.md`
- `TODO_CORE_SYSTEMS_INTEGRATION.md`
- `WILDERNESS_ANALYSIS.md`
