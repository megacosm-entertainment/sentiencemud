# Plan: Casting System Rework

**Date:** February 24, 2026
**Status:** Early Design / Directional Plan

---

## Motivation

The current casting ecosystem has multiple delivery paths with inconsistent resource behavior:

1. Direct cast (`cast "spell" [target]`) consumes mana and has cast beats.
2. Quaff applies potion spells to self and does not participate in normal casting economics.
3. Recite uses scroll timing/economics separate from direct casting.
4. Gear-applied spell effects bypass normal cast pacing and can become mandatory loadout pressure.

Combined with a high number of catalyst/spell types and strong binary effects, this creates balance complexity and repetitive gameplay loops.

---

## Current State (Observed)

### Delivery Paths

- Direct cast flow is centralized in `do_cast` + `cast_end` (`src/magic.c`), with explicit `min_mana`, `beats`, target validation, and success/failure states.
- Potions (`do_quaff`) and scrolls (`do_recite`/`recite_end`) execute object spell lists through `obj_cast_spell` (`src/act_obj.c`, `src/magic.c`).
- Skill/spell metadata is already data-driven in JSON (`min_mana`, `beats`, targets, spell function name) via `skill_data.c`.

### Catalysts and Affinities

- Catalysts are deeply integrated via `has_catalyst` / `use_catalyst` (`src/handler.c`) and script interfaces.
- Sector affinities are currently keyed to catalyst types and modify damage output (`src/fight.c`, `src/sectors_runtime.c`).
- Catalyst taxonomy currently spans many elemental/domain variants.

### Resource Model

- Direct casting uses mana + manastore, and beats are delay-based.
- Non-direct invocation paths (especially gear) do not consistently use the same mana-throughput model.

---

## Design Goals

1. **Unify spell economics** across cast/quaff/recite/gear pathways.
2. **Shift from pool-dump casting to throughput/channeling pacing**.
3. **Reduce catalyst/type complexity** while preserving flavor and editor/script compatibility.
4. **Make gear spell power a tradeoff**, not pure upside.
5. **Support party-focused combat at effective level 30** with predictable pacing and lower binary outcomes.
6. **Support multiple casting paradigms** (mana, blood, ritual, future systems) without per-spell hardcoded special cases.

---

## Core Proposal

## A) Unified Invocation Layer

Introduce a normalized spell invocation pipeline that all delivery modes use:

- Invocation source: direct, potion, scroll, gear, script token.
- Shared prechecks: silence/nomagic/rules/context.
- Shared pacing model: throughput budget + channel duration.
- Shared post-processing: interrupts, partial completion, event output.

Direct cast remains explicit player action; quaff/recite/gear become specialized invocation sources with modifiers, not separate economic systems.

---

## B) Throughput-First Casting Model

Replace primary dependence on large mana pool spending with channel throughput.

### Concepts

- **Mana Capacity**: total magical reserve (still exists).
- **Mana Throughput**: per-round/per-beat channel bandwidth.
- **Spell Load**: how much throughput a spell consumes while channeling.
- **Commit Window**: minimum channel time before release resolves.

### Behavior

- Fast/simple spells: low load, short commit.
- Powerful spells: high load, longer commit, stronger interrupt risk.
- Overload attempts can fail, backfire, or force partial output.

This preserves tactical casting while reducing one-button burst dumping.

---

## C) Gear as Capacity Reservation

Gear with magical effects reserves part of the caster's maximum magical capacity.

- Each active gear-spell effect applies a **capacity reservation cost**.
- Higher-tier/passive-combat effects reserve more.
- Reservation reduces available casting headroom and/or throughput ceiling.

Outcome: players choose between passive magical loadout and active casting flexibility.

---

## D) Catalyst/Type Simplification (Key Change)

Current catalyst/type granularity appears excessive for balance and readability.

### Recommendation

Collapse many catalyst/spell types into a smaller set of **domains**.

Example domain families (illustrative):

- **Primal** (fire/cold/shock/earth/water/air)
- **Vital** (body/nature/toxin/blood)
- **Arcane** (mana/energy/cosmic/astral)
- **Spiritual** (holy/light/soul/law)
- **Umbral** (darkness/death/chaos/mind)

Each legacy type maps to a domain for runtime behavior.

### Why this helps

- Fewer balance knobs with clearer identity.
- Easier affinity and catalyst design.
- Better player comprehension.
- Lower content maintenance overhead.

---

## E) Sector Affinity Rework

Sector affinities should move from narrow type boosts toward domain-level modifiers.

- Affinities remain data-driven in sector runtime tables.
- Modifiers apply to domain-tagged effects, not every micro-type.
- Preserve optional positive/negative values for regional identity.

---

## F) Scroll/Potion Repositioning

Quaff and recite should be tactical utility channels, not bypasses.

- Potions: immediate utility, reduced peak throughput impact, constrained effect classes.
- Scrolls: preloaded channel packages with known delay/risk profile.
- Both should integrate with unified casting constraints where appropriate.

Crafting cost/value can remain high, but combat throughput should be coherent with core casting.

---

## G) Control and Interrupt Philosophy

To support healthier combat loops:

- Interrupt/control (silence/web-like effects) should create windows, not total lockouts.
- Repeated control on same target gets diminishing reliability or duration.
- Channel interruptions should produce partial failure states instead of hard binary in most cases.

---

## H) Multi-Modal Casting Systems (Blood, Ritual, etc.)

Casting should support multiple resource/paradigm profiles through one invocation framework.

### Casting Profiles

- **Arcane Channeling** (default): spends throughput/capacity using mana economics.
- **Blood Magic**: spends HP (or max HP reservation/bleed debt) instead of or in addition to mana.
- **Ritual Magic**: multi-stage casting with longer setup, optional movement/interrupt constraints, and stronger effects.
- **Hybrid/Domain Profiles**: future profiles can combine costs (e.g., mana + catalyst + health strain).

### Common Rules Across Profiles

- All profiles use shared targeting, interruption, and event output semantics.
- Profile determines cost source, pacing modifiers, and failure/backlash tables.
- Defensive and control interactions remain consistent (no profile-specific bypass of core counterplay).

### Blood Magic Guardrails

- Blood costs should respect survivability floors (cannot self-delete without explicit opt-in effect).
- Repeated blood casting applies escalating strain (healing penalty, throughput penalty, or backlash chance).
- Blood profile should have clear upside (faster commit or stronger scaling under constraints), not just a mana substitute.

### Ritual Guardrails

- Rituals use staged progress (`prepare -> channel -> resolve`) with visible telegraphing.
- Optional multi-participant contribution (party/channel links) should increase stability or throughput.
- Ritual interruption should cause partial outcomes (fizzle, backlash, reduced effect), not always total failure.
- Ritual power budget should be balanced around setup risk and coordination overhead.

---

## Data and Editor Implications

Skill/spell editor and JSON model should gain explicit support for:

- `domain`
- `casting_profile` (arcane/blood/ritual/...)
- `throughput_load`
- `commit_beats` (or channel profile)
- `capacity_reservation` (for persistent effects/gear contributions)
- `interruption_profile`
- `resource_costs` (mana/hp/catalyst/debt vectors)
- `ritual_stage_count` and optional `ritual_participant_rules`

Legacy fields (`min_mana`, `beats`) can be retained during migration as compatibility fields.

---

## Migration Strategy

## Phase 0 - Instrumentation

Track baseline metrics:

- cast frequency by source (direct/quaff/recite/gear)
- mana spend vs output contribution
- interrupt/fizzle rates
- catalyst usage distribution by type

Telemetry acquisition strategy when population is low:

- Build deterministic scenario packs (duels, small-party encounters, sustained PvE rotations).
- Run scripted casting benchmarks under fixed seeds to compare pre/post changes.
- Capture profile-level metrics from synthetic runs first; layer in live telemetry later.

Acceptance note:

- During dormancy/progression overhaul, synthetic benchmark deltas are the primary decision input for Phase 0-2 tuning.

## Phase 1 - Compatibility Layer

- Add domain mapping table from existing catalyst/types.
- Keep legacy type enums and script interfaces intact.
- Route runtime calculations through domain translation.

## Phase 2 - Throughput Introduction

- Add throughput and reservation stats to character runtime.
- Implement unified invocation framework behind feature flags.
- Start with selected spell families before full migration.

## Phase 2.5 - Profile Framework

- Add casting profile runtime support (`arcane`, `blood`, `ritual`) and per-profile cost handlers.
- Keep profile defaults backward-compatible for all existing spells.
- Implement profile-level telemetry (failure type, interruption stage, resource strain).

## Phase 3 - Gear Reservation Activation

- Assign reservation costs to gear spell effects.
- Provide player-facing feedback/UI text on reserved capacity.
- Tune costs to avoid invalidating non-caster builds.

## Phase 4 - Content Conversion

- Convert skills/spells to domain + throughput profiles.
- Update sectors/affinities to domain behavior.
- Rebalance catalysts around domain-level consumables.
- Pilot blood-magic and ritual spell sets with explicit encounter-role goals.

## Phase 5 - Cleanup

- Deprecate unused micro-types after conversion complete.
- Keep script-level aliases for compatibility where possible.
- Remove dead balancing paths.

---

## Backward Compatibility Requirements

1. Existing scripts using catalyst checks should continue to function during transition.
2. Existing items with legacy catalyst tags must map deterministically to new domains.
3. Existing spells remain castable while migrated incrementally.
4. OLC/editor workflows must support mixed legacy/new definitions during rollout.
5. Unprofiled legacy spells must default to `arcane` with legacy-equivalent behavior.

---

## Risks and Mitigations

### Risk: Migration complexity across spells/items/scripts

Mitigation:

- Explicit legacy->domain mapping table and staged feature flags.

### Risk: Casters feel nerfed by reservation/throughput constraints

Mitigation:

- Rebalance around sustained control/utility value and clearer role power.

### Risk: Potions/scrolls become irrelevant

Mitigation:

- Preserve differentiated tactical niches and crafting economy hooks.

### Risk: Domain collapse loses flavor

Mitigation:

- Keep flavor at spell/content layer while simplifying systemic math layer.

### Risk: Profile complexity causes tuning instability

Mitigation:

- Start with one profile at parity (`arcane`), then add constrained pilot lists for `blood` and `ritual`.

---

## Success Criteria

- Casting outcomes are less binary and less dominated by a few spell interactions.
- Direct/scroll/potion/gear invocation obey coherent pacing and resource rules.
- Catalyst and affinity systems are understandable and maintainable.
- Gear spell loadouts involve meaningful tradeoffs.
- Party combat supports sustained role gameplay rather than burst/reset loops.
- Blood and ritual casting are viable alternatives with clear strengths/weaknesses and no dominant exploit loop.

---

## Immediate Follow-up Tasks

1. Define legacy catalyst -> domain mapping table.
2. Define initial throughput/reservation formulas and bounds.
3. Identify first 20 spells for pilot conversion.
4. Add telemetry points for cast source and interruption outcomes.
5. Draft player-facing messaging for reservation and channel load.
6. Define blood-magic HP cost/strain model and safety floors.
7. Define ritual staging model, participant contribution rules, and interruption outcomes.
