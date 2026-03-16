# Plan: Crafting, Gathering, and Exploration Systems (Net-New)

**Date:** February 24, 2026 (Updated March 15, 2026)
**Status:** New Systems Design / Directional Plan

**Detailed mechanics documents:**
- `PLAN_CRAFTING_MECHANICS.md` — Synthesis loop, actions, CP system, HQ output
- `PLAN_GATHERING_MECHANICS.md` — Node types, GP system, gathering skills, tool tiers

---

## Motivation

Crafting, gathering, and exploration are being introduced as core systems for Sentience (not legacy mechanics being refurbished). They should launch as first-class progression and party-value loops for Crafter/Gatherer/Explorer identities.

Goals:

- Make crafting/gathering meaningful to world interaction, not side economy only.
- Ensure utility classes contribute to party outcomes without becoming mandatory tax roles.
- Integrate outputs and pacing with combat/casting/party/class reworks.

---

## Initial Design Risks to Address

1. **Utility loop isolation**
   - New utility systems can become disconnected from encounter pacing and role identity if designed in a silo.

2. **Progression mismatch**
   - Combat progression and utility progression may use incompatible assumptions if not designed under one budget model.

3. **Resource model opacity**
   - Node distribution, rarity, and output impact can drift without explicit system-wide power/economy budgets.

4. **Class identity risk**
   - Crafter/Gatherer/Explorer can feel either too weak (economy-only) or too mandatory (hard bottlenecks).

---

## Design Principles

1. **Utility as strategic contribution**
   - Utility classes should create options, resilience, and preparation advantages, not direct throughput dominance.

2. **World-rooted loops**
   - Gathering should be region/biome/context-aware and tie to exploration and logistics.

3. **Recipe clarity and role fit**
   - Crafting outputs should map to explicit role needs (support, sustain, control setup, mobility, protection, recovery).

4. **Cross-system budget alignment**
   - Crafted power must respect combat/casting pacing and defense/stat frameworks.

---

## System Direction

## A) Gathering Framework (New)

- Introduce node families by domain/biome/region.
- Add quality tiers with predictable variance windows.
- Add tool/skill interactions for yield, speed, and failure risk.
- Add exploration hooks (hidden nodes, route optimization, environmental risk/reward).

## B) Crafting Framework (New)

- Move recipes to explicit data-driven schema with clear inputs, outputs, and role tags.
- Distinguish output categories:
  - Consumables (short tactical windows)
  - Components (system dependencies, catalysts/materials)
  - Equipment mods/upgrades (bounded, budget-aware)
  - Utility deployables (camp, ward, traversal, ritual prep)

## C) Class Integration (Crafter / Gatherer / Explorer Archetypes)

Crafter, Gatherer, and Explorer are **archetypes** (`archetype_id` in `CLASS_DATA`), not classes
themselves. Individual jobs sit under each archetype and level independently.

### Gatherer Archetype — Jobs

| Job | Domain |
|---|---|
| Miner | Ore, stone, gems, crystals |
| Botanist | Wood, plants, fibers, resins |
| Fisher | Fish, shells, sea plants, deep-water reagents |

### Crafter Archetype — Jobs

| Job | Output focus | Gear coverage |
|---|---|---|
| Blacksmith | Melee weapons, heavy armor | Primary competitive |
| Weaver | Light/medium armor, cloaks | Primary competitive |
| Carpenter | Ranged/magic weapons, shields, deployables | Competitive + utility |
| Alchemist | Potions, poisons, consumables, augment materia | Consumable + support |
| Jeweler *(optional)* | Rings, earrings, trinkets | Competitive for accessory slots |

### Explorer Archetype

Explorer provides scouting, routing, and hazard mitigation that amplifies both Gatherer and
Crafter efficiency rather than having its own production domain. See Explorer Class Coverage below.

Party contribution channels:
- **Crafter:** synthesis, quality control, adaptation recipes, infrastructure.
- **Gatherer:** acquisition, refinement prep, scarcity handling, logistics.
- **Explorer:** discovery, route/region bonuses, rare source access, hazard mitigation.

These archetypes provide distinct party value channels in both preparation and active expedition cycles.

### Explorer Class Coverage (Explicit)

Explorer should be designed as a full gameplay pillar, not a passive travel bonus package.

Core explorer loops:

- **Survey and Intel:** reveal node potential, hazard bands, and encounter modifiers before commitment.
- **Routecraft:** optimize travel paths, extraction routes, and logistics windows for party operations.
- **Discovery Unlocks:** unlock hidden sites, unique node variants, and contextual interaction points.
- **Expedition Utility:** provide in-field tools (scouting markers, hazard dampening, traversal setup, emergency egress support).
- **Regional Mastery:** long-term progression tied to biomes/regions that improves team preparation and execution.

Party value expectation:

- Explorer contribution should improve consistency, safety, and opportunity quality for the group without replacing core combat or casting roles.

## D) Party and Encounter Integration

- Permit utility classes to influence encounter readiness (resists, supplies, tactical tools, recovery options) without replacing combat role gameplay.
- Design encounters with optional utility solutions (prep paths, environmental interactions, sustain routes).

## E) Economy and Balance Controls

- Add global control knobs for resource inflow, sink rates, and output potency.
- Prevent runaway stockpiling loops through decay, specialization, or usage constraints where needed.
- Separate PvE convenience outputs from PvP-critical outputs when required.

---

## Data and Tooling Implications

- Extend recipe schema with role tags, progression tags, and budget tags.
- Extend node schema with biome/domain metadata and spawn rules.
- Add telemetry for gather/craft loops: time-to-output, output usage, sink velocity, and party impact.
- Ensure editors/OLC tools can manage node families and recipe groups safely.

---

## Delivery Strategy (Net-New Buildout)

## Phase 0 - Discovery and Metrics

- Inventory adjacent dependencies (economy, classes, regions, encounter hooks) that will interface with these new systems.
- Instrument gather/craft throughput and usage by content tier.
- Identify dead loops and dominant loops.

## Phase 1 - Schema and Integration Layer

- Introduce unified node and recipe schemas.
- Add runtime integration interfaces to combat/casting/party/class systems.

## Phase 2 - Pilot Regions and Core Loops

- Launch one region and one crafted-output family per category.
- Validate pacing, usability, and economy impact.

## Phase 3 - Class Value Activation

- Activate class-specific utility mechanics and progression hooks.
- Validate party contribution for Crafter/Gatherer/Explorer in expedition content.

## Phase 4 - Full Integration with Core Systems

- Connect outputs and constraints to combat/casting budgets.
- Ensure party-tier and actor-model behaviors consume utility outputs coherently.

## Phase 5 - Expansion and Stabilization

- Expand content coverage and regional specialization.
- Lock balancing envelopes for utility classes to avoid mandatory-composition drift.

---

## Risks and Mitigations

### Risk: Utility classes become mandatory bottlenecks

Mitigation:

- Provide alternative acquisition paths and bounded utility impact.

### Risk: Crafting outputs destabilize combat/casting tuning

Mitigation:

- Apply role-budget tags and hard output caps in early phases.

### Risk: Gathering becomes repetitive grind

Mitigation:

- Use exploration variability, route optimization, and contextual events.

### Risk: Economy inflation

Mitigation:

- Tune sink velocity and rarity bands with telemetry before expansion.

---

## Success Criteria

- Crafter/Gatherer/Explorer have clear and valued party contribution channels.
- Crafting/gathering loops are engaging and world-connected, not purely transactional.
- Outputs support strategy and preparation without overriding combat/casting core loops.
- Economy remains stable under normal and high-engagement play patterns.
- Explorer gameplay is a distinct, active loop (survey/route/discovery/expedition support), not a passive stat bonus bucket.

---

## Material Tiers (Summary)

Five tiers aligned with content and class level brackets. Full detail in `PLAN_GATHERING_MECHANICS.md`.

| Tier | Class level | Examples |
|---|---|---|
| 1 — Common | 1-20 | Iron Ore, Oak Log, Sardine |
| 2 — Refined | 21-40 | Mythril Ore, Mahogany, Carp |
| 3 — Rare | 41-60 | Darksteel Ore, Ancient Wood, Deepwater Bass |
| 4 — Exotic | 61-80 | Adamantite, Ironwood, Stormfish |
| 5 — Legendary | 81-100 | Aurite Crystal, World-Tree Heartwood, Abyssal Shark |

Raw materials refine into processed materials (ore → ingot, log → lumber) as a discrete step.
Gatherer classes can self-refine at base efficiency; Crafter classes get better yield and
occasional special outputs when performing the same refinement.

---

## Program Dependency

This plan is governed by the cross-system dependency contract in `PLAN_CORE_SYSTEMS_INTEGRATION.md` and tracked through `TODO_CORE_SYSTEMS_INTEGRATION.md`.
