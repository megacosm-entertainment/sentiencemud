# Plan: Combat Loop and Damage Rework

**Date:** February 24, 2026
**Status:** Early Design / Directional Plan

**Program dependency:** See `PLAN_CORE_SYSTEMS_INTEGRATION.md` for cross-system sequencing and phase gates with casting, party tiers, and class evolution.

---

## Motivation

Current combat is dominated by a small number of binary outcomes:

- `sanctuary` (flat 50% incoming damage reduction) is effectively mandatory.
- `dispel magic` success/failure often decides the fight.
- Flee/re-engage patterns can outperform sustained combat.
- PvP often collapses into silence/web + dispel + damage race.
- Gear-based spell reapplication encourages "remove all; wear all" loops.

As progression moves to a party-focused model with max effective class level 30, this loop becomes increasingly brittle and harder to balance.

---

## Current Problems to Solve

### 1) Binary Defensive State

A flat 50% DR buff doubles effective HP while active and creates extreme pressure to maintain or remove it.

### 2) Dominant Combat Pattern

Common high-end flow:

1. Attack
2. Attempt dispel
3. Flee
4. Re-engage

This encourages exploit-adjacent pacing and suppresses diverse skill usage.

### 3) Flee-Kill Incentives

Immediate disengage after offense reduces retaliation windows and rewards reset cycles.

### 4) Throughput Inflation Risk

Removing current sanctuary without compensation effectively doubles practical damage pace.

### 5) Party-Scaling Mismatch

Legacy level/stat growth and multiclass stacking can inflate damage unpredictably, conflicting with level-30 effectiveness goals.

---

## Directional Decisions

## A) Replace Sanctuary's Combat Role

Sanctuary will no longer be a personal flat DR combat buff.

New intent:

- `sanctuary` becomes a location safety mechanic (temporary anti-combat ward in an area/location).
- It is strategic utility (positioning/reset/control), not mandatory throughput mitigation.

### Consequence

Removing old sanctuary requires global damage/durability retuning to preserve encounter pacing.

---

## B) Rebalance Damage Around Party Combat

Target model:

- Encounters tuned around party roles (frontline, support, control, damage).
- Less single-round burst determinism.
- More interaction and tactical windows.

Initial compensation options after sanctuary removal:

1. Global damage scalar reduction
2. HP/EHP increase
3. Crit/burst cap reduction
4. Hybrid approach (recommended)

Recommended first pass (subject to telemetry):

- Damage scalar: reduce global outgoing damage moderately
- Durability: increase baseline survivability
- Burst control: reduce extreme spike frequency/magnitude

---

## C) Break Binary Dispel Dynamics

If defensive buffs remain in combat systems, they should degrade rather than collapse in one event.

Design intent:

- Convert high-impact defensive effects to strength tiers or charge pools.
- `dispel` reduces strength/duration in steps, not all-or-nothing removal.
- Preserve counterplay while reducing coin-flip outcomes.

---

## D) Reduce Flee-Kill Abuse

Near-term:

- Ensure flee handling preserves retaliation/threat context rather than resetting combat state too cleanly.
- Introduce disengage cost (tempo loss/exposure) for immediate re-engage loops.

Long-term:

- Move toward encounter-scoped combat entities with participant tracking, ordering, and threat.
- Allow controlled disengage and pursuit behavior instead of hard reset semantics.

---

## E) Unify Combat Messaging Pipeline

Combat text will be routed through a structured event layer.

- Keep verbose mode as fallback.
- Default to concise round summaries where appropriate.
- Preserve high-signal immediate messages (death/incapacitation/major effects).

This supports readability and future attack-table/party AI features.

---

## F) Shift Progression Away from Raw Stat Inflation

Given max effective class level 30 and broader multiclass progression:

- Reduce or remove per-level unconditional stat inflation.
- Move power toward role identity, kit choices, gear tradeoffs, and horizontal utility.
- Use diminishing returns/soft caps to prevent multiclass throughput runaway.

---

## G) Modernize Armor, Weapons, and Damage-Type Defenses

Current mitigation and weapon models are too coarse for party-era combat:

- Armor remains tied to legacy AC buckets (slash/pierce/bash/exotic).
- Resist/vuln are blunt fixed modifiers (roughly ±25%) across many damage types.
- Weapon damage behavior is narrow, reducing gear/build identity and encounter design space.

Design intent:

- Replace legacy AC-only defense with layered mitigation concepts (physical mitigation, magical mitigation, penetration, and guard/stability style stats).
- Move resist/vuln from flat binary steps toward graded curves/tiers with soft caps and diminishing returns.
- Preserve type identity while making outcomes less all-or-nothing.
- Expand weapon profiles so attack patterns and damage composition create role differences without one dominant path.

Implementation principles:

- Keep compatibility bridges for existing flags/items while introducing richer internals.
- Avoid multiplying hidden multipliers; prefer explicit, inspectable defense contributions.
- Support separate PvE/PvP tuning knobs where needed.

---

## H) Rebuild Actor Stats and Combat Identity (Players + NPCs)

Current combat identity is constrained by legacy assumptions:

- NPC combat is heavily flag-driven and lacks clear class/skill identity.
- Player combat states are primarily tuned for hack-and-slash throughput races.
- Stat expression is broad but shallow, making many builds converge in practice.

Design intent:

- Move to explicit **combat archetypes** for both players and NPCs (tank/control/support/skirmisher/artillery/etc.).
- Give NPCs a real combat model: stat profile + ability package + behavior profile, rather than only act/off flag combinations.
- Make player states richer than pure DPS race by introducing role-relevant secondary stats and interaction windows.

Dependency note:

- This actor migration is a required prerequisite for the planned tiered companion/party model (especially Tier 2+), where NPC companions must express class-like role identity, skill usage, and tactical behavior.

Model direction:

- Introduce per-actor **combat profile** data:
	- primary stats (power, resilience, focus)
	- secondary stats (accuracy, penetration, guard, control potency, control resist, recovery)
	- behavior profile (target priority, threat response, disengage rules, cooldown priorities)
- Support class/kit-driven ability sets for NPCs so encounters are authored through mechanics, not just inflated numbers.
- Keep legacy flags as compatibility signals while migrating to profile-driven runtime behavior.

---

## Proposed Implementation Phases

## Phase 0 - Instrumentation (Required)

Collect and monitor:

- Median and percentile damage per round
- Burst spike distribution
- Fight duration by content tier
- Flee frequency and re-engage timing
- Dispel impact on win/loss outcomes

## Phase 1 - Stability Patches

- Fix known flee-retaliation/threat continuity issues.
- Add conservative anti-loop friction for immediate flee/re-engage.
- Keep behavior changes minimal but measurable.

## Phase 2 - Sanctuary & Damage Retune

- Remove old sanctuary DR behavior.
- Add sanctuary-as-location-safe-mechanic.
- Apply global pacing compensation (damage/HP/burst) and tune to target TTK.

## Phase 3 - Combat Event and Display Unification

- Route damage and combat actions through a unified event model.
- Add concise default display with verbose fallback.
- Keep script/magic/weapon proc output consistent with same pipeline.

## Phase 4 - Encounter and Threat Foundation

- Introduce encounter/participant tracking model.
- Add NPC threat tracking and pursuit/disengage logic.
- Prepare for attack-table-driven NPC behavior and party role AI.

## Phase 5 - Progression/Scaling Alignment

- Finalize level-30 effectiveness scaling.
- Retune class and multiclass interactions.
- Validate party-focused balance targets across PvE and PvP.

## Phase 6 - Defense/Weapon System Migration

- Introduce compatibility-layered defense model alongside legacy AC/resist/vuln.
- Convert resist/vuln flags to graded runtime values (with legacy mappings).
- Add weapon profile data needed for richer damage composition and encounter interaction.
- Rebalance core gear baselines and high-impact outliers under the new model.

## Phase 7 - Stat and Actor Framework Migration

- Add runtime combat-profile schema for players and NPCs.
- Migrate selected NPC families from flag-only behavior to archetype + ability package behavior.
- Introduce richer player secondary stat interactions and tune around role expression.
- Reconcile class/progression systems with encounter pacing targets and party role depth.
- Keep legacy fallback paths during staged migration.
- Gate companion tier rollout so advanced tiers are enabled only after actor-profile baseline is in place.

---

## Risks and Mitigations

### Risk: Over-correction to low lethality

Mitigation:

- Ship conservative scalar first; tune upward gradually with telemetry.

### Risk: Legacy scripts/gear assume old sanctuary behavior

Mitigation:

- Add compatibility audit for spells, items, and scripts before removing old behavior.

### Risk: PvE and PvP need different pacing

Mitigation:

- Support separate balancing knobs for PvE and PvP where needed.

### Risk: Messaging overhaul obscures combat clarity

Mitigation:

- Preserve explicit high-signal events and provide player verbose toggle.

### Risk: Defense model migration causes balance whiplash

Mitigation:

- Run dual-calculation telemetry (legacy vs new) during transition and ship in staged content slices.

### Risk: NPC actor migration increases content complexity

Mitigation:

- Start with a small set of archetype templates and reusable behavior packages before full NPC conversion.

---

## Success Criteria

- Combat no longer revolves around dispel->flee loops as dominant strategy.
- Sanctuary is strategic utility, not required personal DR upkeep.
- Party encounters support role interactions beyond pure damage race.
- PvP outcomes rely less on binary buff states and more on tactical sequencing.
- Text output remains readable in high-action rounds.
- Armor and damage-type interactions are meaningful without requiring binary resist/vuln stacking.
- Weapon choices produce distinct tactical roles instead of mostly equivalent damage race behavior.
- NPCs demonstrate identifiable combat roles and ability patterns beyond raw stat inflation.
- Player builds show deeper tactical identity than pure burst-race optimization.

---

## Handoff to Next Topic: Casting System

Casting redesign should proceed with this combat direction in mind:

1. Interrupt and control effects should create windows, not total lockouts.
2. Buff reapplication should avoid mandatory maintenance loops during active combat.
3. Cast pacing should support party roles and shared encounter rhythm.
4. Spell effects should integrate with unified combat event output.

This document is the baseline context for the upcoming casting-system plan.

---

## Appendix A: V1 Combat Stat Model (Draft)

This appendix defines a practical first-pass model to move away from legacy-only AC/flags while remaining migration-friendly.

### A1) Primary Combat Axes

- **Power**: baseline outgoing effect scaling for attacks and offensive abilities.
- **Resilience**: baseline incoming reduction and effective survivability.
- **Focus**: casting/control quality, recovery, and interruption resistance.

### A2) Secondary Stats

- **Accuracy**: hit quality and consistency.
- **Penetration**: bypasses part of target mitigation.
- **Guard**: flat/graded anti-burst protection and stagger resistance.
- **Control Potency**: improves non-damage effect strength/duration.
- **Control Resist**: reduces incoming control reliability.
- **Recovery**: cadence recovery (cooldowns, post-action lock, resource stabilization).

### A3) Defense Layers (Runtime)

1. Base mitigation from Resilience + gear profile.
2. Type/domain mitigation from graded resistance table (not binary ±25%).
3. Penetration contest (attacker penetration vs defender guard/resilience).
4. Final clamp layer for spike control (anti one-round delete).

### A4) Weapon Profile Fields (V1)

- Damage composition weights (physical/type/domain mix)
- Cadence profile (fast/medium/slow)
- Reach/engagement tag
- Penetration bias
- Control interaction tag (e.g., stagger, bleed, sunder)

### A5) NPC Combat Profile (V1)

NPC definitions gain:

- `archetype`
- `stat_profile`
- `ability_package`
- `behavior_profile`

Legacy `act/off` flags continue as compatibility overrides during migration.

### A6) Migration Mapping (V1)

- Legacy AC buckets -> base mitigation + profile tags.
- Legacy resist/vuln flags -> graded domain/type modifiers.
- Flag-only NPCs -> starter archetype templates with conservative defaults.

This V1 model is intended to be telemetry-tuned, not a final locked schema.
