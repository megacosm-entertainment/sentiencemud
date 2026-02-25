# Plan: Weather and NPC Ship Systems (Wilderness-Dependent)

**Date:** February 24, 2026
**Status:** New Systems Integration Plan

---

## Purpose

Define how weather and transport mechanics should be introduced as integrated systems, with explicit dependency on the wilderness runtime architecture.

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

Status update (2026-02-25):

- Player/staff-facing wilderness projection labels are now materially clearer in runtime views: room/exits/location diagnostics prefer wilderness map names and terrain shownames over UID-only labels.
- Builder/admin observability for wilderness spawns improved: region spawn entries resolve wnums to readable indexed names (`list_name` preferred), reducing ambiguity when tuning transport-adjacent ambient ecosystems.

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
- Transport actors (sea ships, airships, and land caravans/wagons)
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

## C) Transport Actor Model

NPC transport actors should use explicit behavior profiles:

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

Land caravan/wagon path (legacy source):

- `src_20_dev` includes prior pull/yoke mechanics (`do_pull`, `do_yoke`, `do_unyoke`) and cart-linked mobile state (`pulled_cart`).
- This legacy model is a candidate backport foundation for trade caravans and wagon teams in current wilderness runtime.

## D) Encounter Integration

Transport encounters should be event-driven by proximity, faction, weather, and route context rather than room-presence coincidence.

## Cross-Plan Mapping (Ship Architecture)

This plan is the operational bridge between wilderness runtime mechanics and the completion-track scope in `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`.

- **Ship Phase 0 (runtime contract):** supplied by wilderness-side persistence and coordinate simulation ownership.
- **Ship Phase 1 (interior/world sync):** consumes weather + movement state generated here, but transition rules remain in ship architecture scope.
- **Ship Phase 5 (convoy/fleet):** reuses weather drift/risk outputs from this plan as formation stress inputs.
- **Ship Phase 6 (NPC ecosystem):** depends on this plan's region/weather weighting for route value and conflict pressure.

## Three-Doc Execution Contract

This document is governed jointly with:

- `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`
- `PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md`

Execution guardrails:

- Weather + transport behavior work in this plan must consume runtime/persistence contracts defined by wilderness plan.
- Ship/transport features implemented here must stay within architecture boundaries (actor ownership, sync rules, fleet semantics) defined by ship architecture plan.
- If a feature touches persistence ownership, resolver order, chunk/runtime lifecycle, or actor authority, update all three docs in the same change.

Phase ownership map:

- **Wilderness plan owns:** storage contract, resolver order, chunk/runtime lifecycle, sidecar persistence.
- **Ship architecture plan owns:** actor/interior authority, combat/fleet/formation semantics, long-horizon ship ecosystem design.
- **Weather + NPC ships plan owns:** behavior execution slices (weather effects, route goals, NPC transport goal weighting) built on the above contracts.

---

## Delivery Phases

## Phase 0 - Wilderness Prerequisites

- Finalize wilderness runtime-state/persistence requirements.
- Define simulation tick ownership and cadence.

## Phase 1 - Weather Simulation Reactivation

- Bring storm simulation online in coordinate space.
- Validate persistence and player-facing weather projections.

Status (2026-02-24):

- Implemented: active coordinate storm lifecycle (spawn/drift/expire) and forecast projection.
- Implemented: wilderness storm persistence in wilderness state sidecars (`storms` in `wilderness_state.json`).
- Implemented: region-aware weather weighting for storm type selection, target density, and lifecycle tuning (ocean/polar/undersea profiles).
- Implemented: area-region weather profile controls in `aedit regions weather` with JSON persistence (`density`, `life`, `severity`).

Suggested region tuning presets:

- `calm`: `density 70`, `life 80`, `severity -35`
- `coastal`: `density 130`, `life 120`, `severity 20`
- `extreme`: `density 185`, `life 170`, `severity 60`

## Phase 2 - Transport Core Actors

- Introduce transport entities (sea, air, land) with basic route and patrol behavior.
- Validate movement and persistence independent of loaded vrooms.

Status (2026-02-24):

- Implemented (baseline): NPC-owned transport autopilot now starts movement for sea ships and airships in wilderness without manual helm commands.
- Implemented (baseline): route-driven NPC navigation now activates from existing ship route definitions when available.
- Implemented (baseline): NPC detour goals can temporarily pull transports off current routes and then resume their prior waypoint target.
- Implemented (baseline): weighted goal selection now includes trade-stop detours using existing area `trade_list` metadata as destination candidates.
- Implemented (baseline): profile-aware weighting by `npc_sub_type` now differentiates patrol, trader, raider/hunter, and generic detour behavior.
- Implemented (baseline): escort/fleet-style detours now allow profile-matched NPC ships to form up around nearby leader vessels and then resume route goals.
- Pending: deeper encounter-aware goal selection and explicit leader/formation command semantics, then land caravan actor backport path.

## Phase 3 - Weather-Ship Coupling

- Apply weather effects to ship pathing, speed, risk, and encounters.
- Add tactical and environmental interactions.

Status (2026-02-24):

- Implemented (baseline): weather-driven movement penalties by active storm severity for ships and airships.
- Implemented (baseline): storm-driven heading drift events for sea ships and airships in wilderness.
- Pending: route-level weather avoidance logic and encounter-risk modulation by storm band.
- Pending: backport and adapt `src_20_dev` yoke/cart-mobile mechanics for land caravans and wagons.

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
