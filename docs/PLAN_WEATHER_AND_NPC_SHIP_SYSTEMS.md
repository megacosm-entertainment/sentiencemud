# Plan: Weather and NPC Ship Systems (Wilderness-Dependent)

**Date:** February 24, 2026
**Status:** New Systems Integration Plan

---

## Purpose

Define how weather and NPC ship mechanics should be introduced as integrated systems, with explicit dependency on the wilderness runtime architecture.

For full ship-system completion scope (interiors, crew, naval combat, fleets), see `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`.

This plan assumes wilderness rework is the platform prerequisite.

---

## Dependency Statement

Weather and NPC ship behavior depend on wilderness capabilities that support persistent world-state simulation.

Required wilderness foundations:

- Stable coordinate/world-state model independent of transient room lifetime
- Simulation-friendly entity update loops (storms, ships, hazards)
- Persistence model for dynamic wilderness state
- Efficient visibility/projection from world-state to player-facing room/vroom context
- Integration with area-native region metadata as the authoritative region source

Without these, weather and NPC ship systems will remain partial or brittle.

---

## Current Observations

- Existing weather code includes storm structures and movement logic, but update paths are not fully active.
- Existing ship mechanics support movement/navigation and player-facing ship systems.
- NPC ship behavior, weather coupling, and wilderness-scale encounter orchestration are incomplete.
- Virtual room lifecycle complexity has historically disrupted persistent simulation behavior.

---

## System Goals

1. **Weather as world simulation**
   - Storms and climate effects should exist as persistent world-state actors, not room-local one-offs.

2. **NPC ships as living wilderness actors**
   - NPC ships should patrol, pursue objectives, react to weather, and participate in encounters.

3. **Predictable player-facing effects**
   - Visibility, travel risk, route planning, and encounter odds should be coherent and readable.

4. **Performance-safe simulation**
   - Systems must scale with wilderness size without requiring all virtual rooms to be loaded.

---

## Architecture Direction

## A) Wilderness Simulation Layer

Introduce/extend a simulation layer that runs on world coordinates and references loaded room proxies only when needed.

Core actor types:

- Weather actors (storm fronts, pressure cells, hazard zones)
- Ship actors (player and NPC)
- Route markers and hazard fields

## B) Weather Model

- Persistent storm entities with lifecycle, movement vectors, intensity, and effect profiles
- Region/biome modifiers affecting spawn rates and storm behavior
- Player and ship effects based on proximity/intensity bands

Potential effect channels:

- visibility
- movement speed/heading drift
- navigation error rate
- hazard/event triggers

## C) NPC Ship Model

NPC ships should use explicit behavior profiles:

- patrol
- trade route
- escort
- pirate/raider
- faction response

Behavior components:

- destination/route logic
- threat/risk evaluation
- weather avoidance or exploitation
- engagement/disengagement rules

## D) Encounter Integration

Ship encounters should be event-driven by proximity, faction, weather, and route context rather than room-presence coincidence.

---

## Delivery Phases

## Phase 0 - Wilderness Prerequisites

- Finalize wilderness runtime-state/persistence requirements.
- Define simulation tick ownership and cadence.

## Phase 1 - Weather Simulation Reactivation

- Bring storm simulation online in coordinate space.
- Validate persistence and player-facing weather projections.

## Phase 2 - NPC Ship Core Actors

- Introduce NPC ship entities with basic route and patrol behavior.
- Validate movement and persistence independent of loaded vrooms.

## Phase 3 - Weather-Ship Coupling

- Apply weather effects to ship pathing, speed, risk, and encounters.
- Add tactical and environmental interactions.

## Phase 4 - Encounter and Faction Layer

- Add pirate/naval faction behavior and conflict triggers.
- Add reward/risk loops tied to weather windows and route control.

## Phase 5 - Expansion and Tuning

- Expand route networks, storm archetypes, and NPC ship roles.
- Tune performance and gameplay readability.

---

## Risks and Mitigations

### Risk: Simulation cost explodes with wilderness scale

Mitigation:

- Simulate by coordinate actors and regions, not by loaded-room count.

### Risk: Player feedback becomes opaque

Mitigation:

- Add clear weather/ship intel channels (reports, map cues, navigation prompts).

### Risk: NPC ships feel random or unfair

Mitigation:

- Use rule-driven route/behavior profiles and bounded encounter odds.

### Risk: Tight coupling blocks progress

Mitigation:

- Keep weather and ship actor APIs modular but both pinned to wilderness simulation contract.

---

## Success Criteria

- Weather persists and behaves coherently across wilderness regions.
- NPC ships operate continuously without requiring loaded-room hacks.
- Weather materially affects navigation and encounters in understandable ways.
- Pirate/naval interactions create strategic gameplay rather than random interruptions.
- Performance remains stable under expected population and actor counts.

---

## Program Dependency

This plan is governed by:

- `PLAN_CORE_SYSTEMS_INTEGRATION.md`
- `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`
- `PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md`
- `WILDERNESS_ANALYSIS.md`
- `TODO_CORE_SYSTEMS_INTEGRATION.md`
