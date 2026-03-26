# Design: Data-Driven Loot Tables, Attack Patterns, and Requirements Integration

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Overview

Three interrelated systems to replace script-heavy mob configuration with data-driven
tables, unified by the existing requirements DSL as a shared conditional evaluation
layer.

1. **Loot Tables** — define what a mob drops on death or carries at repop
2. **Attack Patterns** — define how a mob fights each combat round
3. **Requirements DSL Extensions** — new check types so the requirements system
   can evaluate combat state, mob state, and varset variables

All three are JSON-backed, OLC-editable, and designed to complement (not replace)
mprogs. The common case becomes data; the exceptional case stays scripted.

---

## 1. Requirements DSL Extensions

The requirements system is the conditional layer shared by both tables. Today it
evaluates player/quest state. We extend it to also evaluate mob state, combat state,
and varset variables.

### Current Architecture (unchanged)

- **Evaluator**: `requirements_eval_node()` in `requirements.c` — recursive,
  stateless, read-only
- **Context**: `REQUIREMENT_CONTEXT` in `requirements.h` — carries actor + owning
  entity pointers
- **DSL**: text parser (`requirements_text_to_json`) and decompiler
  (`requirements_json_to_text`)
- **Combinators**: `all_of` (AND), `any_of` (OR), nestable 16 deep
- **Escape hatch**: `script` check type fires a prog trigger for custom logic

### Extended REQUIREMENT_CONTEXT

```c
typedef struct requirement_context {
    /* Existing fields — unchanged */
    CHAR_DATA          *actor;
    CHAR_DATA          *self_mob;
    OBJ_DATA           *self_obj;
    ROOM_INDEX_DATA    *self_room;
    TOKEN_DATA         *self_token;
    QUEST_INDEX_V2_DATA *self_quest;

    /* New: table evaluation state (set by caller, read-only to evaluator) */
    int                 combat_round;     /* rounds since fight started, -1 if N/A */
    CHAR_DATA          *fighting;         /* who the mob is fighting (may be NULL) */
    json_t             *cooldown_state;   /* tag -> last_used_round map */
} REQUIREMENT_CONTEXT;
```

The table systems populate these fields before calling `requirements_evaluate_*()`.
The evaluator remains stateless — it reads but never writes these fields. Cooldown
state is managed by the attack table system (see section 3).

### New Check Types

#### Player/Mob State Checks

| DSL Syntax | JSON | Description |
|---|---|---|
| `hp_percent [op] N` | `{"hp_percent": N}` or `{"hp_percent": {"op":"<=","value":50}}` | Actor's HP as percentage of max_hit |
| `mana_percent [op] N` | `{"mana_percent": ...}` | Actor's mana as percentage of max_mana |
| `move_percent [op] N` | `{"move_percent": ...}` | Actor's move as percentage of max_move |
| `affect WORD` | `{"affect": "haste"}` | Actor has named affect (AFF flag or spell affect) |
| `mob_flag WORD` | `{"mob_flag": "boss"}` | Self mob has ACT flag (NPC equivalent of plr_flag) |
| `fighting` | `{"fighting": true}` | Actor is currently in combat |
| `group_size [op] N` | `{"group_size": ...}` | Number of PCs in actor's group |

When evaluating for a mob (as `self_mob`), `hp_percent` etc. can reference
`self_mob` instead of `actor` via a `self` prefix:

| DSL Syntax | Description |
|---|---|
| `self.hp_percent <= 50` | The mob (self) is at or below 50% HP |
| `self.affect berserk` | The mob (self) has the berserk affect |

This distinction matters: in a loot table `actor` is the killing player, `self_mob`
is the dead mob. In an attack table `self_mob` is the attacking mob, `actor` is the
target (victim).

#### Combat Round Checks

| DSL Syntax | JSON | Description |
|---|---|---|
| `round [op] N` | `{"round": ...}` | Current combat round number (1-based) |
| `round_mod N` | `{"round_mod": N}` | `combat_round % N == 0` (every Nth round) |

#### Cooldown Check

| DSL Syntax | JSON | Description |
|---|---|---|
| `cooldown TAG N` | `{"cooldown": {"tag":"fire_breath","rounds":3}}` | True if N+ rounds have elapsed since TAG was last marked |

The cooldown check is **read-only**. It inspects `context->cooldown_state[TAG]`
and compares `combat_round - last_used >= N`. The attack table system is
responsible for recording `last_used` when an attack fires.

If the tag has never been used, the cooldown is considered satisfied (allows first
use).

#### Varset Variable Check

| DSL Syntax | JSON | Description |
|---|---|---|
| `var NAME [op] N` | `{"var": {"name":"phase","op":">=","value":2}}` | Integer variable check on self entity |
| `var NAME STRVAL` | `{"var": {"name":"mode","value":"enraged"}}` | String variable check on self entity |
| `var.actor NAME [op] N` | `{"var_actor": {"name":...}}` | Variable check on the actor instead |

Variables are read from the entity's `progs->vars` list using `variable_get()`.
The `self` entity is determined from the `REQUIREMENT_CONTEXT` (whichever of
`self_mob`, `self_obj`, `self_token`, `self_room` is populated).

For integer variables, comparison operators work as with `tot_level`. For string
variables, `eq`/`ne` comparisons are used (default `eq`).

This is the existing varset system — `VARIABLE` structs on `PROG_DATA->vars`,
supporting types including `VAR_INTEGER`, `VAR_STRING`, `VAR_BOOLEAN`. Variables
can be set via the `varset` script command or through `index_vars` on the template.
They can optionally persist (the `save` flag on each variable).

#### Probability Check

| DSL Syntax | JSON | Description |
|---|---|---|
| `chance N` | `{"chance": 30}` | N% random chance (1-100). Evaluated fresh each check. |

Useful for loot drop rates and attack variation.

### Implementation Notes

- Each new check type is a `requirements_eval_*` function following the existing
  pattern (see `requirements_eval_tot_level` as a template)
- Add to the `requirements_eval_leaf()` dispatcher and the `rp_parse_atom()` DSL
  parser
- Add decompiler cases to `req_decompile_node()`
- The `self.` prefix for mob-state checks needs a small parser extension: when the
  DSL encounters `self.hp_percent`, it emits `{"self_hp_percent": ...}` and the
  evaluator reads from `context->self_mob` instead of `context->actor`

---

## 2. Loot Tables

### Problem

Today, all variable loot is handled by mprogs with `TRIG_REPOP` (items placed on
mob at spawn) and `TRIG_DEATH` (items created before corpse). Static loot uses
G/E resets. There is no way to query, audit, or bulk-tune drop rates without
reading individual scripts.

### Data Model

```c
/* Loot roll modes */
#define LOOT_ALWAYS     0   /* Every entry in the group drops */
#define LOOT_ONE_OF     1   /* Pick exactly one entry (weighted) */
#define LOOT_EACH       2   /* Each entry rolls independently (by chance) */

typedef struct loot_entry_data {
    LOOT_ENTRY_DATA    *next;
    WNUM                obj_wnum;       /* object to create */
    int                 weight;         /* weight for ONE_OF mode */
    int                 chance;         /* percentage for EACH mode (1-100) */
    int                 qty_min;        /* minimum quantity (default 1) */
    int                 qty_max;        /* maximum quantity (default 1) */
    char               *requires;       /* requirements DSL (NULL = unconditional) */
} LOOT_ENTRY_DATA;

typedef struct loot_group_data {
    LOOT_GROUP_DATA    *next;
    char               *name;           /* builder label, e.g. "rare_drops" */
    int                 mode;           /* LOOT_ALWAYS / LOOT_ONE_OF / LOOT_EACH */
    int                 max_drops;      /* cap total drops from this group (0=no cap) */
    char               *requires;       /* group-level requirement (gate the whole group) */
    LOOT_ENTRY_DATA    *entries;        /* linked list of entries */
} LOOT_GROUP_DATA;

typedef struct loot_table_data {
    LOOT_TABLE_DATA    *next;
    LOOT_GROUP_DATA    *groups;         /* linked list of groups */
} LOOT_TABLE_DATA;
```

Field on `MOB_INDEX_DATA`:
```c
LOOT_TABLE_DATA *loot_table;   /* NULL = no data-driven loot */
```

### JSON Format

Stored inline in the mob's area JSON or in a separate loot table section:

```json
{
  "loot_table": {
    "groups": [
      {
        "name": "guaranteed",
        "mode": "always",
        "entries": [
          { "obj": "5#100", "qty_min": 2, "qty_max": 5 }
        ]
      },
      {
        "name": "common_drops",
        "mode": "each",
        "entries": [
          { "obj": "5#101", "chance": 40 },
          { "obj": "5#102", "chance": 25 },
          { "obj": "5#103", "chance": 10, "requires": "tot_level >= 100" }
        ]
      },
      {
        "name": "rare_drop",
        "mode": "one_of",
        "requires": "chance 50",
        "entries": [
          { "obj": "5#200", "weight": 70 },
          { "obj": "5#201", "weight": 25 },
          { "obj": "5#202", "weight": 5, "requires": "quest_completed 2#50" }
        ]
      }
    ]
  }
}
```

### When Loot Tables Are Evaluated

Two trigger points, matching the existing script trigger points:

1. **At repop** — after `create_mobile()` and G/E resets, before `TRIG_REPOP` fires.
   Objects created by the loot table are added to the mob's inventory/equipment.
   This replaces repop mprogs that just do `oload`.

2. **At death** — during `make_corpse()`, after gold but before existing inventory
   is transferred. Objects created by the loot table go directly into the corpse.
   This replaces death mprogs that just do `oload`.

Each group/entry specifies `"trigger": "repop"` or `"trigger": "death"` (default
`"death"`).

The loot table fires **before** the corresponding mprog trigger, so scripts can
still inspect/modify what the table produced.

### Evaluation Flow

```
For each group in loot_table->groups:
    1. Check group->requires against REQUIREMENT_CONTEXT
       - actor = killing player (death) or NULL (repop)
       - self_mob = the mob
    2. If group passes:
       switch (group->mode):
         LOOT_ALWAYS:
           For each entry: check entry->requires, if pass create qty objects
         LOOT_ONE_OF:
           Filter entries by entry->requires
           Weighted random pick from passing entries
           Create qty objects for the picked entry
         LOOT_EACH:
           For each entry: check entry->requires, then roll entry->chance%
           If both pass, create qty objects
```

### OLC: `medit loot`

```
medit loot                          -- show loot table
medit loot add <group_name> <mode>  -- add a loot group
medit loot <group> add <obj_wnum> [chance N | weight N] [qty N-M]
medit loot <group> requires <DSL>   -- set group requirement
medit loot <group> <N> requires <DSL>  -- set entry requirement
medit loot <group> remove <N>       -- remove entry
medit loot remove <group>           -- remove group
```

### Imm Commands

```
lootinfo <mob>          -- display effective loot table for a mob
lootfind <obj>          -- find all mobs whose loot tables reference this object
lootaudit <area>        -- summary of all drop rates across an area
```

---

## 3. Attack Patterns

### Problem

`mob_hit()` calls `one_hit()` N times with the same damage type, then randomly
tries one offensive skill. All mobs of the same template fight identically.
Making a mob fight interestingly requires fight mprogs, which are clunky for
defining per-attack-slot behavior.

### Data Model

```c
/* Attack pattern flags */
#define MATTK_AREA          (A)   /* This attack hits all combatants */
#define MATTK_SECONDARY     (B)   /* Uses secondary weapon */

typedef struct mob_attack_pattern {
    MOB_ATTACK_PATTERN *next;
    int16_t             slot;           /* attack order (1-N) */
    int16_t             attack_type;    /* index into attack_table[] */
    DICE_DATA           damage;         /* override dice ({0,0,0} = use mob default) */
    int16_t             chance;         /* percentage chance to execute (100=always) */
    char               *cooldown_tag;   /* tag for cooldown tracking (NULL=none) */
    int16_t             cooldown_rounds;/* rounds between uses */
    char               *requires;       /* requirements DSL (NULL=unconditional) */
    long                flags;          /* MATTK_* flags */
    char               *skill_name;     /* if non-NULL, execute this skill instead of one_hit */
    char               *message;        /* custom attack message (NULL=use attack_table) */
} MOB_ATTACK_PATTERN;

typedef struct mob_attack_table {
    MOB_ATTACK_TABLE   *next;
    MOB_ATTACK_PATTERN *patterns;       /* linked list of attack slots */
    int                 base_attacks;   /* fallback attack count if no patterns match */
} MOB_ATTACK_TABLE;
```

Field on `MOB_INDEX_DATA`:
```c
MOB_ATTACK_TABLE *attack_table;   /* NULL = use legacy mob_hit() behavior */
```

Runtime cooldown state on `CHAR_DATA` (or on `PROG_DATA`):
```c
json_t *attack_cooldowns;   /* { "tag": last_used_round, ... } */
int     combat_round;       /* incremented each violence_update while fighting */
```

### JSON Format

```json
{
  "attack_table": {
    "base_attacks": 2,
    "patterns": [
      {
        "slot": 1,
        "attack": "claw",
        "damage": "4d8+10",
        "chance": 100
      },
      {
        "slot": 2,
        "attack": "claw",
        "damage": "4d8+10",
        "chance": 100
      },
      {
        "slot": 3,
        "attack": "bite",
        "damage": "6d10+15",
        "chance": 100,
        "requires": "self.hp_percent <= 75"
      },
      {
        "slot": 4,
        "attack": "fire_breath",
        "damage": "8d12+20",
        "chance": 100,
        "cooldown_tag": "fire_breath",
        "cooldown_rounds": 3,
        "requires": "self.hp_percent <= 50"
      },
      {
        "slot": 5,
        "skill": "bash",
        "chance": 40
      },
      {
        "slot": 6,
        "attack": "death_throes",
        "damage": "10d12+40",
        "flags": ["area"],
        "cooldown_tag": "death_throes",
        "cooldown_rounds": 5,
        "requires": "self.hp_percent <= 25 AND var phase >= 2"
      }
    ]
  }
}
```

### How Attack Patterns Replace `mob_hit()`

In `mob_hit()`, when `ch->pIndexData->attack_table != NULL`:

```
1. Build REQUIREMENT_CONTEXT:
     actor         = victim (the target)
     self_mob      = ch (the attacking mob)
     combat_round  = ch->combat_round
     cooldown_state = ch->attack_cooldowns

2. For each pattern in attack_table->patterns (ordered by slot):
     a. Roll pattern->chance — skip if fails
     b. If pattern->cooldown_tag:
          Check cooldown_state[tag], skip if not enough rounds elapsed
     c. Evaluate pattern->requires against context — skip if fails
     d. Execute the attack:
          - If pattern->skill_name: call do_<skill>(ch, victim_name)
          - Else: call one_hit() with pattern->attack_type and pattern->damage
     e. If pattern->cooldown_tag:
          Record cooldown_state[tag] = combat_round
     f. If MATTK_AREA: also hit other combatants (same as current OFF_AREA_ATTACK)

3. If no patterns matched at all: fall back to base_attacks × one_hit()
   (ensures the mob always does something)
```

### Interaction With Existing Systems

- **`pIndexData->attacks` / `pIndexData->dam_type`**: ignored when `attack_table`
  is set. The attack table completely replaces the old attack count + uniform
  damage type model.
- **`off_flags` skill roulette**: replaced by explicit skill slots in the pattern
  table. Mobs with attack tables do not roll the random bash/kick/disarm at the
  end of `mob_hit()`.
- **`spec_fun` (breath/casting)**: can be replaced by attack pattern entries with
  skill names. Existing spec_funs continue to work alongside attack tables for
  backward compatibility.
- **`TRIG_FIGHT` mprogs**: still fire after the attack table resolves, same as
  today. Scripts can layer additional behavior on top.
- **`TRIG_PREROUND`**: still fires before attack resolution.
- **`AFF_HASTE` / `AFF_SLOW`**: haste adds an extra execution of the last
  matching pattern; slow limits to the first matching pattern only.
- **`attacks = -1` (scripted only)**: still respected — if `attacks == -1` and
  no `attack_table`, no automatic attacks. If `attack_table` is set, it takes
  precedence.

### Phased Fights via Varset

For boss encounters with distinct phases, builders combine the requirements DSL
`var` check with `TRIG_HPCNT` or `TRIG_FIGHT` mprogs that set variables:

**Mprog (TRIG_HPCNT at 50%):**
```
mob varset self phase 2 save
mob echoaround $n roars with fury, entering a new phase!
```

**Attack table entries:**
```json
{ "slot": 4, "attack": "enraged_slam", "damage": "6d12+25",
  "requires": "var phase >= 2" }
```

The mprog handles the transition logic and messaging. The attack table reads the
variable to determine which attacks are available. Clean separation of concerns.

### OLC: `medit attacks`

```
medit attacks                           -- show attack table
medit attacks base <N>                  -- set base_attacks fallback
medit attacks add <slot> <attack_type> <damage_dice> [chance N]
medit attacks <slot> requires <DSL>     -- set requirements
medit attacks <slot> cooldown <tag> <rounds> -- set cooldown
medit attacks <slot> skill <skill_name> -- make this a skill slot
medit attacks <slot> flags <flags>      -- set MATTK flags
medit attacks remove <slot>             -- remove pattern
```

### Imm Commands

```
attackinfo <mob>        -- display effective attack table for a mob
```

### Attack Patterns Under COMBAT_DATA (Multi-Engagement / Multi-Room)

`PLAN_COMBAT_ENTITY.md` changes combat from a single `ch->fighting` loop to
participant-scoped `COMBAT_DATA` with potentially multiple simultaneous
engagements. Attack patterns should execute inside that model with the
following contract.

#### Round and Cooldown Scope

- **Round source**: for requirements checks (`round`, `round_mod`) use the
  participant's active engagement round (`ENGAGEMENT_DATA.round_count + 1`), not
  a global room tick.
- **Cooldown source**: cooldowns are tracked per-attacker, per-combat
  participant state (not globally on the mob template).
- **Cooldown clock**: store last-used against `COMBAT_DATA.total_rounds` (or an
  equivalent combat-monotonic round id) so cooldowns remain stable across
  movement and pursuit.
- **Persistence across engagements**: cooldown history persists when an
  engagement closes and a new one opens in the same combat. This explicitly
  prevents flee/re-engage from resetting attack-table cooldowns.
- **Reset condition**: cooldown history clears when the mob leaves or resolves
  the combat, matching other combat-scoped runtime state.

#### Targeting and Reachability

- Attack pattern execution starts from `ch->fighting` when valid.
- If `ch->fighting` is NULL, dead, or unreachable under engagement range rules,
  choose a new target from the mob's threat table (highest hostile threat that
  is reachable now).
- A pattern cannot execute against unreachable targets. If no reachable target
  exists, the mob spends its turn without forcing cross-room invalid actions.
- `MATTK_AREA` applies to hostile participants in the **same active
  engagement** that are reachable by that attack profile; it is not combat-wide
  across unrelated simultaneous engagements.

#### REQUIREMENT_CONTEXT Mapping

When executing one attack-pattern slot in a combat-entity round:

- `self_mob` = attacking mob participant
- `actor` = selected target for this slot
- `fighting` = current primary target (`ch->fighting` after retarget if any)
- `combat_round` = active engagement round index (1-based)
- `cooldown_state` = participant-scoped cooldown map for this combat

This keeps requirements evaluation stateless while allowing multi-engagement
combat state to drive decisions.

#### Turn Resolution Order (Within `violence_update` Participant Turn)

For a mob with `attack_table`:

1. Resolve/validate target from engagement reachability + threat table
2. Fire `TRIG_PREROUND`
3. Evaluate attack slots in slot order (chance -> cooldown -> requires -> execute)
4. Record cooldown tags for slots that fired
5. Fire `TRIG_FIGHT`

For a mob without `attack_table`, keep legacy `multi_hit()` path.

#### Interaction With COMBAT_DATA Open Questions

- **OQ-9 (adjacent-room rules)** directly gates which pattern slots can fire;
  attack tables consume the final reachability rule but do not define it.
- **OQ-12 (threat formula)** controls retarget quality when `ch->fighting`
  becomes invalid mid-combat.
- **OQ-6/OQ-7 (movement/disengage costs)** affect attack opportunity frequency,
  but not slot semantics.

#### Companion AI Integration (PLAN_PARTY_SYSTEM)

Attack tables are also the execution surface for Tier 2-4 companion combat
behavior from `PLAN_PARTY_SYSTEM.md`:

- **Tier 2 (henchmen):** use constrained role-specific pattern subsets
  (e.g., tank taunt slots, healer support slots) and simple stance gates.
- **Tier 3/4 (heroes/alts):** use full pattern sets with tactical preference
  weighting (target priority, skill usage preference) before slot evaluation.
- **Stance mapping:** `aggressive/defensive/passive` modifies pattern
  eligibility or priority, not hard-coded per-skill branching in combat loop.
- **Target preference mapping:** companion `target_pref` influences the
  threat-table candidate chosen by `combat_resolve_target()` before a slot runs.

This keeps companion intelligence data-driven: party tactics selects intent,
COMBAT_DATA selects legal/reachable targets, and attack patterns choose concrete
actions per round.

#### Ranged Slot Integration (`shoot.c` Migration)

To replace ad-hoc ranged command flow, attack patterns should support ranged
execution semantics directly:

- Add ranged-capable slot metadata (weapon/profile requirement + max range).
- Allow optional `aim_rounds` on a pattern (channel/windup before fire).
- During windup, keep a stable target handle (combat participant id), not only
  a name string.
- On resolve, re-check engagement reachability and LOS via shared combat
  helpers before applying hit/miss logic.
- If windup is interrupted (control, forced movement, LOS loss), emit
  interruption event and clear pending shot.

This lets builders author ranged behavior in the same table system as melee and
skills, while keeping multi-room rules consistent with COMBAT_DATA.

---

## 4. Implementation Sequence

### Phase 1: Requirements Extensions

Files: `requirements.c`, `requirements.h`

1. Extend `REQUIREMENT_CONTEXT` with new fields
2. Implement new eval functions:
   - `requirements_eval_hp_percent` / `mana_percent` / `move_percent`
   - `requirements_eval_affect`
   - `requirements_eval_mob_flag`
   - `requirements_eval_fighting`
   - `requirements_eval_group_size`
   - `requirements_eval_round` / `round_mod`
   - `requirements_eval_cooldown`
   - `requirements_eval_var`
   - `requirements_eval_chance`
   - Self-prefixed variants (`self_hp_percent`, etc.)
3. Extend DSL parser and decompiler for new keywords
4. Unit tests for each new check type

No changes to existing consumers. New fields in `REQUIREMENT_CONTEXT` default
to zero/NULL, so existing callers (quests, objects, channels) are unaffected.

### Phase 2: Loot Tables

Files: new `loot.c` / `loot.h`, changes to `merc.h`, `db.c`, `fight.c`,
`editors/mobiles/medit.c`

1. Define data structures in `merc.h`
2. Implement JSON load/save in `loot.c`
3. Implement evaluation logic in `loot.c`
4. Hook into `reset_room()` (repop trigger point) and `make_corpse()` (death)
5. Add `medit loot` OLC commands
6. Add `lootinfo` / `lootfind` / `lootaudit` imm commands
7. Update both `Makefile` and `CMakeLists.txt`

### Phase 3: Attack Patterns

Files: new `attack_pattern.c` / `attack_pattern.h`, changes to `merc.h`,
`fight.c`, `editors/mobiles/medit.c`

1. Define data structures in `merc.h`
2. Add `combat_round` and `attack_cooldowns` to `CHAR_DATA`
3. Implement JSON load/save in `attack_pattern.c`
4. Implement evaluation logic in `attack_pattern.c`
5. Modify `mob_hit()` to dispatch through attack table when present
6. Increment `combat_round` in `violence_update()`; reset on fight end
7. Add `medit attacks` OLC commands
8. Add `attackinfo` imm command
9. Update both `Makefile` and `CMakeLists.txt`

### Phase 4: Migration Tooling (Optional)

1. `lootconvert <mob>` — analyze a mob's repop/death mprogs and suggest a loot
   table equivalent
2. `attackconvert <mob>` — analyze a mob's fight mprogs, off_flags, and spec_fun
   and suggest an attack table equivalent

---

## 5. Design Decisions and Rationale

### Why requirements stay read-only

Keeping the evaluator stateless means it is safe to call from any context — quests,
channels, OLC validation, display code — without worrying about side effects. The
table systems own their mutable state (cooldowns, round counters) and pass it in
via the context. This also means the same requirement expression can be evaluated
multiple times without changing its result (except `chance`, which is inherently
non-deterministic).

### Why varset instead of a custom variable system

The varset system already exists on every entity with progs. Builders already know
how to use `varset` in scripts. Variables can persist across reboots (the `save`
flag). Index-level variables (`index_vars`) provide template defaults. There is no
reason to build a second variable system when this one covers all the needed types
(`VAR_INTEGER`, `VAR_STRING`, `VAR_BOOLEAN`).

### Why not merge loot and attack tables into a single generic "action table"

While they share the requirements evaluation layer, their execution semantics are
fundamentally different. Loot tables produce objects; attack tables dispatch combat
actions with cooldowns and round tracking. A generic abstraction would add
complexity without meaningful benefit. Sharing the requirements DSL gives 90% of
the unification value with none of the cost.

### Backward compatibility

- Mobs without `loot_table` or `attack_table` behave exactly as today
- G/E resets continue to work (they fire before loot tables)
- Mprogs continue to work (they fire after table evaluation)
- `off_flags`, `spec_fun`, `attacks`, `dam_type` are all still functional for
  mobs that don't use attack tables
- No existing data needs to change; migration is opt-in per mob
