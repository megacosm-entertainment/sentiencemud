# Plan: Gathering Mechanics — Node Interaction Loop

**Date:** March 15, 2026
**Status:** Design Draft
**Depends on:** `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`, `PLAN_CRAFTING_GATHERING_REWORK.md`

---

## Overview

This document defines the mechanical layer for the Gatherer archetype classes: how node interaction
works, what gathering skills exist, how stats scale, and how tools gate content progression.
It is the detailed complement to `PLAN_CRAFTING_GATHERING_REWORK.md`.

---

## Gatherer Classes

The **Gatherer** archetype contains three independent jobs, each with its own level track
(via the `CLASS_DATA`/`CLASS_LEVEL` job system) and distinct node domain:

| Class | Domain | Primary materials |
|---|---|---|
| **Miner** | Underground nodes | Ore, stone, gems, coal, crystals |
| **Botanist** | Surface/canopy nodes | Wood, plants, fibers, resins, seeds |
| **Fisher** | Water nodes | Fish, shells, sea plants, deep-water reagents |

Each class levels independently. A character may hold levels in multiple Gatherer classes.
The three classes share the same node interaction engine but have different node pools, skill
sets, and stat weights.

---

## Stats

| Stat | Effect |
|---|---|
| **Gathering** | Base yield quantity per attempt; determines success against higher-tier nodes |
| **Perception** | Reveals hidden items at nodes; improves HQ material chance |
| **GP (Gathering Points)** | Resource pool spent on gathering skills; recovers between nodes and on rest |

Stats come from:
- Class level (primary scaling)
- Gathering tools (main-hand determines tier and contributes Gathering; off-hand contributes Perception)
- Gathering gear (hat, body, hands, legs, feet — dedicated set, no combat stats)
- Food buffs (temporary stat boosts; meaningful at high tiers)
- Cordials (GP restoration items; crafted by Alchemist — see Integration)

---

## Tools

Each Gatherer class uses a dedicated tool pair:

| Class | Main-hand | Off-hand |
|---|---|---|
| Miner | Pickaxe | Miner's Sledge |
| Botanist | Hatchet | Scythe |
| Fisher | Fishing Rod | Bait Pouch |

**Tool tier determines maximum node tier accessible.** A character with a Tier 2 pickaxe cannot
work Tier 3 veins regardless of class level. This creates an explicit dependency: Crafters
(Blacksmith for mining tools, Carpenter for botanical and fishing tools) must produce high-tier
tools for Gatherers to advance.

| Tool tier | Node tiers unlocked | Approx. class level required |
|---|---|---|
| Tier 1 | Tier 1-2 | 1-20 |
| Tier 2 | Tier 1-3 | 21-40 |
| Tier 3 | Tier 1-4 | 41-60 |
| Tier 4 | Tier 1-5 | 61-80 |
| Tier 5 (masterwork) | All tiers + legendary | 81-100 |

---

## Node Types

### Standard Nodes

Always available within a region. Refresh once per game day (tied to the in-game day cycle).
Provide the bulk of everyday materials.

```
Iron Vein [Tier 3]  [Standard Node]
Region: Stoneback Ridge   Biome: Mountain
Gathering: 320 req   Attempts: 6
Items visible: Iron Ore, Iron Ore, Iron Ore, Chunk of Magnetite
```

Attempts are consumed one per gather action. When attempts reach 0, the node is depleted for
the current day.

### Unspoiled Nodes

Time-gated: appear for a limited window (e.g., 3 in-game hours, roughly 9 real minutes) on
a predictable schedule tied to the in-game clock. Provide rarer materials not found at standard
nodes.

```
[UNSPOILED] Darksteel Vein  [Tier 4]  [Active for: 8 minutes]
Region: Ironhold Depths   Attempts: 4
Warning: Node expires when the bell strikes the 10th hour.
```

Locating unspoiled nodes requires either Explorer scouting, the Miner-specific `detect_unspoiled`
skill, or prior knowledge of the schedule. Part of high-level Gatherer play is routing efficiently
between scheduled nodes.

### Legendary Nodes

Extremely rare. Require high Gathering and Perception stats to detect and work. Spawn infrequently
in fixed regions on a longer schedule than unspoiled nodes. Yield unique materials not obtainable
anywhere else.

```
[LEGENDARY] Aurite Crystal Formation  [Tier 5]
Region: Sunken Vault   Attempts: 3
Gathering: 580 req   Perception: 520 req
Items: Aurite Crystal, Aurite Crystal (HQ), Prismatic Shard (hidden)
```

Legendary node windows are broadcast as vague region events ("You sense a rare resonance in the
Sunken Vault."). Explorer characters with Regional Mastery can pin the exact location.

### Fishing Nodes (Fisher-specific)

Fisher nodes are slightly different in structure. Instead of static Attempts, Fishing uses a
**Patience** model: each cast consumes time and bait. Node depletion is based on fish population
rather than fixed attempts.

---

## Node Interaction Flow

### Arriving at a Node

When a character enters a room containing an active node:

```
You notice a rich iron vein in the rock face.  [examine vein]
```

`examine <node>` shows the node details (tier, item list, Gathering/Perception requirements,
remaining attempts) without consuming an attempt. Perception affects how many hidden items are
revealed in the examine output.

### Basic Gather Attempt

```
> gather attempt
You strike the vein with your pickaxe.
Yield: 2x Iron Ore
[Iron Vein — Attempts: 5 remaining]
```

A basic attempt costs 0 GP but yields the minimum based on base Gathering stat vs. node tier.

### Using Gathering Skills

Skills are used before or instead of a basic attempt:

```
> gather kings_yield_ii
You read the stress lines in the rock and plan your strikes. (+100% yield, 1 attempt)
> gather attempt
You strike with practiced force: 4x Iron Ore
[Iron Vein — Attempts: 4 remaining]
```

Multiple skills can be stacked in the same attempt window if their buff types are compatible.

### Hidden Items

Items above the character's Perception threshold appear greyed out or hidden:

```
Items: Iron Ore (✓✓✓), Chunk of Magnetite (?? — Perception: 350 req)
```

Once Perception meets the threshold (via gear, buffs, or skills), hidden items become available
and yield on gather attempts.

---

## Gathering Skills

Skills are unlocked by class level. All skills cost GP unless noted.

### Miner Skills

| Skill | Lv | GP | Effect |
|---|---|---|---|
| Sharp Vision I | 4 | 50 | Perception +50 for 3 attempts |
| Sharp Vision II | 15 | 100 | Perception +100 for 3 attempts |
| Sharp Vision III | 55 | 150 | Perception +150 for 3 attempts |
| King's Yield I | 12 | 400 | Next attempt: yield +50% |
| King's Yield II | 40 | 500 | Next attempt: yield +100% |
| Mountaineer I | 25 | 200 | Gathering +50 for next 2 attempts |
| Unearth I | 35 | 200 | Perception +100 + reveals 1 hidden item |
| Collector's Glove | 50 | 0 | Toggle: switch to Collectible mode |
| Legendary Detection | 68 | 200 | Check if a legendary node is active in the current region |
| Solid Reason | 72 | 300 | Restore 3 Integrity to node (see Integrity below) |

### Botanist Skills

| Skill | Lv | GP | Effect |
|---|---|---|---|
| Leaf Turn I | 4 | 50 | Perception +50 for 3 attempts |
| Leaf Turn II | 15 | 100 | Perception +100 for 3 attempts |
| Leaf Turn III | 55 | 150 | Perception +150 for 3 attempts |
| Brunt's Harvest I | 12 | 400 | Next attempt: yield +50% |
| Brunt's Harvest II | 40 | 500 | Next attempt: yield +100% |
| Arbor Call I | 35 | 200 | Reveals 1 hidden plant/fiber item |
| Collector's Glove | 50 | 0 | Toggle: switch to Collectible mode |
| Eponymous | 68 | 200 | Check if an unspoiled node is active nearby |
| Ageless Words | 72 | 300 | Restore 3 Integrity to node |

### Fisher Skills

| Skill | Lv | GP | Effect |
|---|---|---|---|
| Snagging | 4 | 50 | Next catch: increase mooch-eligible fish chance |
| Patience I | 15 | 600 | Patience buff: all catches are HQ eligible; reduces yield count |
| Patience II | 40 | 600 | Improved Patience; better HQ chance |
| Powerful Hookset | 28 | 200 | Next bite: guaranteed catch (no fail chance) |
| Surface Slap | 44 | 200 | Release current fish back, maintain node population |
| Collector's Glove | 50 | 0 | Toggle: Collectible mode |
| Fish Eyes | 63 | 700 | Force a window in which rare fish appear for 60 seconds |

### Shared Skills (all Gatherer classes)

| Skill | Lv | GP | Effect |
|---|---|---|---|
| Toil of the Pioneer | 2 | 0 | Survey: check remaining attempts on current node |
| GP Cordial | — | 0 | (Item) Restore 300 GP; crafted by Alchemist |
| Hi-Cordial | — | 0 | (Item) Restore 400 GP; crafted by Alchemist |
| Collector's Glove | 50 | 0 | Toggle Collectible mode |

---

## Collectible Gathering

Like Collectible crafting, gathered items can be targeted for Collectible mode. This applies to
specific node items flagged as collectible-eligible.

In Collectible mode:
- Each attempt builds a **Collectibility** score rather than yielding items.
- Quality skills (Sharp Vision, King's Yield) contribute to Collectibility differently.
- When Collectibility is high enough, `gather collect` finalizes a single collectible item.
- Turn in to an NPC for class experience, scrips, and exclusive materials.

This is the primary high-level advancement and scrip-earning loop for Gatherers.

---

## Node Integrity

Some Tier 4-5 nodes have an **Integrity** meter in addition to Attempts. Integrity represents
the node's structural stability. Aggressive gathering (using max-yield skills every attempt)
degrades Integrity faster.

When Integrity reaches 0, the node collapses early, losing remaining attempts. Characters
with the `Solid Reason` / `Ageless Words` skill can partially restore Integrity, enabling
efficient full extraction at high-tier nodes.

This mechanic rewards investment in the restoration skills and punishes pure throughput optimization
at legend-tier content.

---

## Material Tiers

Five tiers of materials, aligned with content and class level brackets:

| Tier | Class level | Gatherer examples | Crafter uses |
|---|---|---|---|
| 1 — Common | 1-20 | Iron Ore, Oak Log, Sardine, Cotton Boll | Apprentice gear, early potions |
| 2 — Refined | 21-40 | Mythril Ore, Mahogany, Carp, Flax | Journeyman gear, mid potions |
| 3 — Rare | 41-60 | Darksteel Ore, Ancient Wood, Deepwater Bass, Spider Silk | Expert gear, advanced consumables |
| 4 — Exotic | 61-80 | Adamantite, Ironwood, Stormfish, Moonweed | Masterwork gear, endgame consumables |
| 5 — Legendary | 81-100 | Aurite Crystal, World-Tree Heartwood, Abyssal Shark, Dreambloom | Best-in-slot gear, unique items |

### Refinement Step

Raw materials can be refined into processed materials before crafting:

- Iron Ore → Iron Ingot (at a smelter; Miner can do this at base efficiency)
- Oak Log → Oak Lumber (at a sawmill; Botanist at base efficiency)
- Blacksmith/Carpenter performing the refinement get +yield and occasional special outputs
  (e.g., High-quality Iron Ingot from ordinary ore)

This separates the logistics loop (Gatherer harvests and refines raw) from the crafting loop
(Crafter processes refined material into gear).

---

## HQ Materials

Gathering can yield HQ versions of materials (e.g., HQ Iron Ore) with a chance based on
Perception vs. node difficulty.

HQ materials provide a Quality head start when used in synthesis:
- NQ material: synthesis begins at 0 Quality
- HQ material: synthesis begins at ~20% of max Quality

When a recipe uses multiple materials, only HQ versions of flagged "quality materials" provide
the bonus. Filler materials don't contribute Quality even as HQ.

This creates a direct economic link: HQ materials command higher prices and are sought by
high-end Crafters chasing guaranteed HQ outputs.

---

## Fisher Special Mechanics

Fishing uses additional mechanics not present in Mining/Botanist:

**Bait selection**: Different baits attract different fish. Bait is consumed per cast.
High-tier bait is crafted by Alchemist or purchased.

**Bite timing**: Fish bite at a specific timing window after casting. Missing the window
(using `gather hook` too early or too late) reduces catch quality or causes a miss.

**Mooch**: Catching a high-value small fish can be used as live bait to catch a larger
predator fish. `gather mooch` converts the caught fish to bait for the next cast.

**Weather dependency**: Certain legendary fish only appear under specific weather conditions
(storm, clear sky, fog). Explorer's weather prediction abilities are directly useful here.

---

## Explorer Integration

Explorer is a separate archetype whose class abilities interact with Gathering:

- **Survey**: Before committing to a node, an Explorer can assess yield potential and hazard
  level. Reveals hidden items without spending a Gatherer's Perception.
- **Node Detection**: Explorer skills can locate unspoiled and legendary nodes without the
  Gatherer needing the detection skills themselves.
- **Regional Mastery**: Explorer's long-term region progression improves node spawn rates,
  extends unspoiled windows, and reveals node schedules as persistent knowledge.
- **Hazard Dampening**: Some high-tier gathering locations have environmental hazards. Explorer
  can neutralize or reduce these, enabling safe gathering in dangerous zones.

This creates a natural party incentive for Gatherers + Explorers to work together on high-value
routes.

---

## Integration Points

- **Gathering → Crafting**: HQ materials grant Quality bonus in synthesis. Material tier gates
  which recipes are available to Crafters.
- **Alchemist → Gathering**: GP Cordials (GP restoration items) are crafted by Alchemist and
  consumed by all Gatherers during long gathering sessions. Bait for fishing is Alchemist-crafted.
- **Carpenter/Blacksmith → Gathering**: High-tier gathering tools are Crafter outputs. Tool tier
  gates node access.
- **Explorer → Gathering**: Node detection, routing, hazard mitigation.
- **Economy**: Node refresh rate, rarity tier spawn rates, and material sink velocity are
  global control knobs. See `PLAN_CRAFTING_GATHERING_REWORK.md` for economy design.

---

## Open Questions

1. **Attempt regeneration**: Should depleted nodes have a partial refresh mid-day (e.g., half
   attempts back at the midday bell), or strictly once per day?

2. **Shared node depletion**: Do all players share a single node instance per room, or does each
   player get their own attempt pool per node? Shared depletes faster but creates group coordination
   dynamics. Per-player is simpler but removes the social layer.

3. **Fisher bite timing window**: How forgiving should the timing window be in a text client?
   A text MUD cannot rely on visual cues. Options: a brief prompt ("Something bites!"), a
   configurable timer, or a pure skill-check model for Fisher.

4. **Legendary node broadcast radius**: Should the region event hint be area-wide, zone-wide,
   or global? Zone-wide creates competition; area-wide is too local to be useful for routing.

5. **Collectible scoring**: Should Collectibility accumulate across multiple attempts on the same
   node (using skills to build score), or resolve on a single premium attempt?

6. **Tool crafting dependency depth**: At what tier does tool quality become the hard gate vs.
   class level? Too early = punishes solo play; too late = undermines the Crafter dependency.

---

## See Also

- `PLAN_CRAFTING_MECHANICS.md` — Crafter synthesis loop, actions, output quality system
- `PLAN_CRAFTING_GATHERING_REWORK.md` — High-level system direction and phased delivery
- `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md` — Job system architecture this builds on
- `PLAN_CORE_SYSTEMS_INTEGRATION.md` — Cross-system dependency contract
