# Plan: Core Systems Integration (Combat, Casting, Party, Classes)

**Date:** February 24, 2026
**Status:** Program-Level Integration Draft

---

## Why This Plan Exists

The current reworks are deeply interdependent:

- Combat rework changes survivability, pacing, and actor behavior assumptions.
- Casting rework changes resource, timing, and control dynamics.
- Party system requires meaningful NPC role identity and tactical AI.
- Class evolution (including newer paradigms like Crafter/Gatherer/Explorer) changes progression and capability expression.
- Weather and NPC ship mechanics depend on wilderness simulation architecture and persistence.

Crafting, gathering, and exploration are also being introduced as net-new core systems and must be sequenced with the same integration discipline.

If these ship independently, gameplay coherence and balance will likely break.

---

## Existing Class Paradigm (Current + Emerging)

Current class families in use/planning context:

- **Combat-core:** Mage, Warrior, Cleric, Thief
- **Newer paradigms:** Crafter, Gatherer, Explorer

Integration implication:

- System design cannot assume all classes are pure combat throughput classes.
- Combat and casting systems must support non-combat class value in party play (utility, logistics, control, setup, sustain, traversal, preparation).

---

## Dependency Reality (High-Level)

1. **Class/Actor identity** is upstream of meaningful NPC companion tiers.
2. **Combat actor model** is upstream of party role gameplay.
3. **Casting profile model** must align with combat timing/interrupt semantics.
4. **Defense/weapon model** must align with class and role budgets.

In short: classes -> actor model -> combat/casting runtime -> party tiers and encounter behavior.

---

## Integration Architecture Principles

1. **One runtime model, many expressions**
   - Avoid separate subsystems with different hidden rules.
2. **Role budgets before content tuning**
   - Define tank/control/support/damage envelopes first.
3. **Compatibility-first migration**
   - Preserve legacy behavior behind translation layers while converting.
4. **Telemetry-gated rollout**
   - No major phase promotion without measurable targets.

---

## Cross-System Capability Matrix

| Capability | Classes | Combat | Casting | Party |
|---|---|---|---|---|
| Role identity (tank/control/support/etc.) | Defines role access | Uses role budgets | Supports role tools | Required for Tier 2+ companion behavior |
| Resource pacing | Defines progression scaling | Sets round tempo | Throughput/profile costs | Determines sustained party performance |
| Actor behavior | PC/NPC kits and passives | Threat/targeting/disengage | Interrupt/channel interactions | Companion AI/tactics |
| Survivability model | Stat growth constraints | Defense layers/resists | Defensive spell design | Companion durability and recovery |
| Utility depth (non-combat classes) | Crafter/Gatherer/Explorer hooks | Encounter objectives | Ritual/prep/economy hooks | Party support value beyond DPS |

---

## Program Phases and Hard Gates

## Phase I - Foundations (No Public Behavior Break)

Scope:

- Define actor profile schema (players + NPCs).
- Define class-role mapping framework (including non-combat families).
- Add compatibility translators for legacy flags/AC/resist/vuln.

Gate to Phase II:

- Actor schema live in runtime with no regression in baseline content.
- Legacy-to-new mapping coverage for core entities >95%.

## Phase II - Combat/Casting Core Runtime Alignment

Scope:

- Combat pacing and defense model migration (layered mitigation, graded resists).
- Casting runtime unification (invocation pipeline, throughput model, profile hooks).
- Shared interrupt/control semantics.

Gate to Phase III:

- Target TTK bands stable across representative PvE/PvP scenarios.
- No dominant binary loops (dispel/flee/sanc-style) in telemetry samples.

## Phase III - Class and NPC Identity Activation

Scope:

- Enable class/kit-driven behavior budgets.
- Migrate selected NPC families from flag-only to archetype+ability packages.
- Introduce non-combat class utility contribution hooks in encounter design.

Gate to Phase IV:

- NPC archetype cohorts meet role-expression criteria.
- Party simulation tests show role differentiation without singular dominant strategy.

## Phase IV - Party Tier Enablement

Scope:

- Tier 2+ companion behavior enabled on actor-profile runtime.
- Companion tactical controls tied to class/archetype/skill packages.
- Death/recovery/economy balancing under new combat-casting runtime.

Gate to Phase V:

- Tiered companions pass stability and balance thresholds in grouped content.

## Phase V - Systemic Expansion

Scope:

- Expand casting profiles (blood/ritual and other paradigms).
- Expand class-specific depth loops and progression tracks.
- Remove legacy-only paths once parity is proven.

---

## Critical Coupling Decisions

### 1) Party Tiers Depend on Actor Model

Tier 2-4 companions must not be enabled as pure stat followers. They require:

- archetype/class identity
- skill packages
- tactical behavior profiles

### 2) Casting and Combat Must Share Timing Semantics

Channeling, interruption, control, and action windows must be one model used everywhere.

### 3) Class Design Must Include Non-Combat Value Loops

Crafter/Gatherer/Explorer should have defined encounter and party contribution channels, not only economy-side progression.

### 4) Defense Model Must Be Role-Aware

Mitigation and penetration systems must support class/role expression without hard binary counters.

---

## Suggested Governance and Working Model

- Maintain one integration board for these four systems.
- Require cross-doc updates when any one plan changes assumptions.
- Promote phases only with agreed metrics, not implementation completion alone.

Suggested primary docs:

- `PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md`
- `PLAN_CASTING_SYSTEM_REWORK.md`
- `PLAN_PARTY_SYSTEM.md`
- `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`
- `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`

This plan acts as the dependency contract between them.

---

## Immediate Next Steps

1. Define class-role matrix for all seven class families (Mage/Warrior/Cleric/Thief/Crafter/Gatherer/Explorer).
2. Define actor-profile minimum fields required before Tier 2 companion activation.
3. Define shared combat-casting timing glossary (beats, channel, interrupt, recovery).
4. Define first NPC archetype migration cohort and first companion-tier pilot cohort.
5. Add a single milestone checklist referenced by all four system plans.
