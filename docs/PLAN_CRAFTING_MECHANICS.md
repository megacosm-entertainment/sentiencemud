# Plan: Crafting Mechanics — Synthesis Loop

**Date:** March 15, 2026
**Status:** Design Draft
**Depends on:** `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`, `PLAN_CRAFTING_GATHERING_REWORK.md`

---

## Overview

This document defines the mechanical layer for the Crafter archetype classes: how synthesis works,
what actions exist, how outputs are determined, and how class levels interact with recipe access.
It is the detailed complement to the high-level direction in `PLAN_CRAFTING_GATHERING_REWORK.md`.

---

## Crafter Classes

The **Crafter** archetype contains 4-5 independent jobs, each with its own level track (via the
`CLASS_DATA`/`CLASS_LEVEL` job system) and distinct recipe domain:

| Class | Domain | Gear coverage |
|---|---|---|
| **Blacksmith** | Melee weapons, heavy armor (plate) | Primary competitive |
| **Weaver** | Light/medium armor, cloaks, cloth accessories | Primary competitive |
| **Carpenter** | Ranged/magic weapons (bows, staves), shields, deployables | Primary competitive + utility |
| **Alchemist** | Potions, poisons, food consumables, augment reagents | Consumable + augment focus |
| **Jeweler** *(optional expansion)* | Rings, earrings, necklaces, trinkets | Primary competitive for accessory slots |

Each class is leveled independently. A character may hold levels in multiple Crafter classes
simultaneously. Crafter archetype classes share the same synthesis engine but have different
recipe pools, stat scaling, and action sets.

---

## Synthesis Session

### Opening a Session

```
> craft begin iron_sword
You set up your tools and arrange the materials.
Crafting: Iron Sword [Blacksmith 15]
Progress: 0/200   Quality: 0/1200   Dura: 80/80   CP: 180/180
Condition: Normal
```

A synthesis session is a stateful, turn-based loop. The player issues `craft <action>` commands
until progress is complete, durability runs out, or the player abandons with `craft abandon`.

The session blocks movement. Interruption (combat, disconnect) triggers an auto-abandon.

### The Four Meters

**Progress** — how close the item is to completion. When Progress reaches its cap, synthesis ends
and the item is created. Actions that raise Progress are "synthesis actions."

**Quality** — the potential quality of the finished item. Higher Quality at completion = higher
chance of, or guaranteed, HQ output. Actions that raise Quality are "touch actions."

**Durability** — the structural integrity of the work in progress. Nearly all actions cost
Durability. If Durability reaches 0 before Progress caps, the synthesis fails and materials are
partially lost (see Failure below).

**CP (Crafting Points)** — the resource pool spent on actions. Regenerates on rest and food buffs.
Actions cost varying amounts of CP. Progress/Touch actions that cost 0 CP are weaker; expensive
CP actions are stronger or provide buffs.

### Conditions

Each turn has a Condition that modifies action outcomes:

| Condition | Effect |
|---|---|
| **Normal** | No modifier |
| **Good** | Action effects +50% |
| **Excellent** | Action effects +100%; next condition forced to Poor |
| **Poor** | Action effects -50% |
| **Centered** | Touch actions +25%; triggered by certain buffs |
| **Sturdy** | Durability cost -50% this turn |
| **Pliant** | CP costs -50% this turn |

Condition advances each turn in a weighted random cycle. Players learn to read Condition and
plan high-cost actions around Good/Excellent windows.

---

## Actions

Actions are unlocked by class level. Each action has a CP cost, a Durability cost, and an effect.

### Synthesis Actions (raise Progress)

| Action | Lv | CP | Dura | Effect |
|---|---|---|---|---|
| Basic Synthesis | 1 | 0 | 10 | Progress +120% potency |
| Careful Synthesis | 7 | 7 | 10 | Progress +150% potency; never fails |
| Groundwork | 18 | 18 | 20 | Progress +360% potency; halved if Dura < 50% |
| Delicate Synthesis | 26 | 32 | 10 | Progress +100% + Quality +100% (split) |
| Intensive Synthesis | 38 | 6 | 10 | Progress +400% potency; only usable on Good/Excellent |
| Prudent Synthesis | 52 | 18 | 5 | Progress +180% potency; cannot use under Waste Not |

**Potency** is a multiplier against the character's Craftsmanship stat. Higher Craftsmanship =
more Progress per action.

### Touch Actions (raise Quality)

| Action | Lv | CP | Dura | Effect |
|---|---|---|---|---|
| Basic Touch | 5 | 18 | 10 | Quality +100% potency |
| Standard Touch | 18 | 32 | 10 | Quality +125% potency; combo from Basic Touch: 18 CP |
| Advanced Touch | 43 | 46 | 10 | Quality +150% potency; combo from Standard Touch: 18 CP |
| Precise Touch | 53 | 18 | 10 | Quality +150% potency; only on Good/Excellent; +Inner Quiet stack |
| Prudent Touch | 66 | 25 | 5 | Quality +100% potency; cannot use under Waste Not |
| Trained Finesse | 90 | 32 | 0 | Quality +100% potency; only when Inner Quiet = 10 |
| Byregot's Blessing | 50 | 24 | 10 | Quality +100% + 20% per Inner Quiet stack; clears Inner Quiet |

**Touch potency** scales against the Control stat. Higher Control = more Quality per touch.

### Restoration Actions (recover Durability or CP)

| Action | Lv | CP | Dura | Effect |
|---|---|---|---|---|
| Master's Mend | 7 | 88 | 0 | Restore +30 Durability |
| Immaculate Mend | 65 | 112 | 0 | Restore full Durability |
| Tricks of the Trade | 13 | 0 | 0 | Restore +20 CP; only on Good/Excellent |

### Buff Actions

| Action | Lv | CP | Dura | Effect |
|---|---|---|---|---|
| Inner Quiet | 11 | 18 | 0 | Buff: Touch actions stack a quality multiplier (max 10) |
| Waste Not | 15 | 56 | 0 | Buff 4 turns: Dura costs halved |
| Waste Not II | 47 | 98 | 0 | Buff 8 turns: Dura costs halved |
| Veneration | 15 | 18 | 0 | Buff 4 turns: Synthesis actions +50% potency |
| Innovation | 26 | 18 | 0 | Buff 4 turns: Touch actions +50% potency |
| Great Strides | 21 | 32 | 0 | Buff: Next touch action +100% potency; consumed on use |
| Manipulation | 65 | 96 | 0 | Buff 8 turns: Restore +5 Dura at start of each turn |
| Final Appraisal | 42 | 1 | 0 | Buff: Next time Progress would cap, leave it at 1 instead |

**Combo system**: Standard Touch costs 32 CP normally but only 18 CP when used immediately after
Basic Touch. Advanced Touch similarly combos from Standard Touch. This rewards planning rotations
rather than spamming the same action.

---

## Output: NQ vs HQ

At synthesis completion:

- **NQ (Normal Quality)**: Always produced when Progress caps. Quality value is irrelevant for NQ.
- **HQ (High Quality)**: Produced when Quality at completion crosses a threshold.
  - Soft threshold (~50% of max Quality): HQ chance begins (random roll, scales with Quality %)
  - Hard threshold (~80% of max Quality): Guaranteed HQ output

HQ items have better base stats (typically +10-20% on primary stat) and are valued higher in
player trading. Some recipes produce a different item entirely at HQ (e.g., HQ Iron Sword yields
a "Honed Iron Sword").

---

## Collectibles Mode

Certain recipes can be crafted in **Collectible mode**, toggled with `craft collectible` before
beginning. In Collectible mode:

- The item cannot be equipped or used when complete — it is a turn-in only.
- Quality is tracked as a Collectibility score (0-600+).
- Turning in to a designated NPC rewards class experience, scrips (a currency), and sometimes
  unique materials unavailable from gathering.
- This is the primary way high-level Crafters advance and acquire endgame recipes.

---

## Stats

Crafting classes use three dedicated stats:

| Stat | Effect |
|---|---|
| **Craftsmanship** | Determines potency of Synthesis (Progress) actions |
| **Control** | Determines potency of Touch (Quality) actions |
| **CP** | Total pool of Crafting Points; limits action budget per session |

These stats come from:
- Class level (scales with job level)
- Crafting tools (main-hand + off-hand, both contribute)
- Crafting gear (hat, body, hands, legs, feet — purely crafting-focused set)
- Food buffs (temporary +CP, +Craftsmanship, +Control; major performance boost)
- Materia/augments (socketed into crafting gear by Alchemist)

Crafting gear does not provide combat stats. Combat gear does not provide Craftsmanship/Control.

---

## Tools and Stations

### Tools

Each Crafter class uses a dedicated tool pair (main-hand + off-hand):

| Class | Main-hand | Off-hand |
|---|---|---|
| Blacksmith | Hammer | File |
| Weaver | Needle | Thread Spool |
| Carpenter | Saw | Clamp |
| Alchemist | Mortar | Pestle |
| Jeweler | Graver | Magnifying Loupe |

Tools are tiered and crafted by other Crafters (or occasionally dropped). Tool tier determines
the recipe tier accessible and contributes Craftsmanship/Control stats.

### Stations

Synthesis requires a crafting station of appropriate type. Stations exist in:
- Crafting districts in major cities (always available)
- Carpenter-crafted portable stations (deployable in the field, limited durability)
- Some dungeons and camps (Explorer-placed)

---

## Recipe Access

Recipes are unlocked via:
1. **Level unlock** — most recipes auto-unlock at the required class level
2. **NPC purchase** — with gil or scrips from Collectible turn-ins
3. **Drop discovery** — rare recipe scrolls from content
4. **Synthesis discovery** — combining materials in unusual ratios occasionally yields a new recipe
   (GW2-style auto-discovery; limited to specific recipe families)

Recipes carry:
- Required class and level
- Required Craftsmanship and Control minimums (gate recipe attempt)
- Ingredient list with quantities and quality thresholds
- Output item + NQ/HQ variants
- Optional Collectible version flag

---

## Failure

If Durability reaches 0 before Progress caps:

- Synthesis fails.
- All materials are lost (consumed by the failed attempt).
- No item is produced.
- A small amount of class experience is still awarded (learning from failure).

This makes Durability management the primary risk axis. Actions that conserve Durability
(Waste Not, Manipulation, Prudent Synthesis/Touch) are high value in long recipes.

---

## Integration Points

- **Gathering → Crafting**: Materials come from Gatherer classes. HQ gathered materials give a
  starting Quality bonus when used in synthesis (meaningful advantage early in the loop).
- **Alchemist → Crafting gear**: Food buffs and augment materia are Alchemist outputs consumed
  by all Crafters. Alchemist is a force multiplier for the whole archetype.
- **Explorer**: Can locate rare material caches and unlock region-specific recipe scrolls.
- **Combat party utility**: Crafted consumables, augments, and deployable stations benefit
  combat roles. See `PLAN_CRAFTING_GATHERING_REWORK.md` for party integration design.
- **Economy controls**: Crafting output power is bounded by recipe tags; see rework plan for
  global control knobs.

---

## Open Questions

1. **Synthesis interruption granularity**: Should abandoning mid-synthesis return some materials,
   or always destroy them? Partial refund for high-Durability-remaining abandons is more forgiving.

2. **Recipe difficulty scaling**: Should recipes have a "difficulty spread" (low Craftsmanship
   recipes still completable, just slow), or hard gates (below minimum = cannot begin)?

3. **Condition availability**: Should all conditions (Centered, Sturdy, Pliant) be available to
   all Crafters, or gated by archetype/class? FF14 gates Centered/Sturdy/Pliant to expert recipes.

4. **Cross-class actions**: Should high-level characters with multiple Crafter jobs unlock a small
   set of cross-class actions (similar to early FF14)? Adds multiclass depth but complicates
   balance.

5. **HQ ingredient bonus**: How much starting Quality does an HQ material grant? Needs tuning
   against recipe Quality caps.

---

## See Also

- `PLAN_CRAFTING_GATHERING_REWORK.md` — High-level system direction and phased delivery
- `PLAN_GATHERING_MECHANICS.md` — Gatherer loop, node types, GP system, tool tiers
- `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md` — Job system architecture this builds on
- `PLAN_CORE_SYSTEMS_INTEGRATION.md` — Cross-system dependency contract
