# TODO: Core Systems Integration Milestones

**Date:** February 24, 2026
**Scope:** Cross-system gates for combat, casting, party, classes, and net-new crafting/gathering/exploration systems.

---

## Program Gates

- [ ] **Gate I: Foundations Stable**
  - [ ] Actor profile schema defined and versioned
  - [ ] Class-role matrix finalized for Mage/Warrior/Cleric/Thief/Crafter/Gatherer/Explorer
  - [ ] Legacy translation layer active for core combat/casting stats
  - [ ] Telemetry baseline dashboards in place (synthetic-first, live-augmented)

- [ ] **Gate II: Runtime Alignment**
  - [ ] Combat pacing model passes target TTK bands
  - [ ] Casting invocation/throughput model integrated with combat timing
  - [ ] Defense/weapon migration in dual-calc mode
  - [ ] No dominant binary loop in benchmark scenarios

- [ ] **Gate III: Identity Activation**
  - [ ] NPC archetype templates live (first cohort)
  - [ ] Player secondary stat interactions enabled
  - [ ] Non-combat class contribution channels defined in encounter framework
  - [ ] Companion behavior profile API available

- [ ] **Gate IV: Party Tier Enablement**
  - [ ] Tier 2 companions enabled on actor profiles
  - [ ] Tier 3 tactical controls and role behaviors validated
  - [ ] Companion recovery/death economics validated
  - [ ] Group content pass with no single dominant composition

- [ ] **Gate V: Crafting/Gathering/Exploration System Integration**
  - [ ] Resource node framework and world distribution pass
  - [ ] Crafting recipe/progression model connected to class-role identity
  - [ ] Gathering and crafting loops feed combat/casting/party economies cleanly
  - [ ] Explorer class loop validated (survey, routecraft, discovery, expedition utility)
  - [ ] Utility classes (Crafter/Gatherer/Explorer) have meaningful party value

- [ ] **Gate VI: Expansion and Cleanup**
  - [ ] Blood/ritual casting profiles rolled out to pilot sets
  - [ ] Legacy-only combat/casting paths deprecated where parity is achieved
  - [ ] Content migration coverage target reached
  - [ ] Cross-doc assumptions audited and synchronized

- [ ] **Gate VII: Wilderness Weather and NPC Ship Integration**
  - [ ] Wilderness simulation contract finalized for persistent world actors
  - [ ] Weather actor simulation active and persistent
  - [ ] NPC ship actor model active independent of loaded-room count
  - [ ] Weather effects integrated into ship navigation/encounter logic
  - [ ] Readability/performance validation complete for wilderness-scale operation

- [ ] **Gate VIII: Ship System Completion (Interiors/Crew/Combat/Fleet)**
  - [ ] World ship actor <-> interior instance sync contract implemented and validated
  - [ ] Crew roles/readiness/skill systems materially affect ship outcomes
  - [ ] Naval combat core (maneuver + exchange + boarding transitions) implemented
  - [ ] Following/convoy/fleet mechanics implemented with intent-level command model
  - [ ] NPC ship ecosystem operates stably with faction/route/weather interactions

---

## Phase Work Packs

### WP-1: Metrics and Validation Harness
- [ ] Build test matrix (PvE solo, party PvE, PvP, mixed utility-party)
- [ ] Define pass/fail thresholds per gate
- [ ] Add regression report template
- [ ] Add deterministic scenario packs (fixed seeds/loadouts) for combat/casting comparisons
- [ ] Add scripted bot-run harness for repeated benchmark sweeps
- [ ] Define live-validation overlay criteria for post-dormancy population recovery

### WP-2: Data Model Convergence
- [ ] Finalize shared stat and effect vocabulary
- [ ] Finalize domain/type mapping tables
- [ ] Finalize actor/archetype schema contracts

### WP-3: Economy and Progression Coherence
- [ ] Align class progression rewards with role budgets
- [ ] Align crafting outputs with combat/casting constraints
- [ ] Align gathering availability with region/biome progression

### WP-4: Rollout and Communication
- [ ] Define feature flags and rollout order
- [ ] Draft player-facing change notes per gate
- [ ] Define rollback criteria per gate
