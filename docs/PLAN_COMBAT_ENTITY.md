# Plan: Combat Entity (COMBAT_DATA)

**Date:** March 1, 2026
**Status:** Early Design / Directional Plan

**Program dependency:** See `PLAN_CORE_SYSTEMS_INTEGRATION.md` for cross-system sequencing. This design feeds directly into Phase 4 (Encounter and Threat Foundation) of `PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md`.

---

## Motivation

The current combat model has no first-class "fight" entity. Combat state is distributed across individual `CHAR_DATA` nodes via a single `fighting` pointer per character. This creates several structural problems:

- No history of who participated in a fight, when they entered, or when they left.
- Flee completely resets combat state — no re-engage memory, no cost, no pursuit context.
- The flee/re-engage loop (attack → flee → re-engage) is a dominant exploit pattern with no systemic friction.
- Multi-participant fights emerge from assist flags, not intentional design.
- Telemetry (Phase 0 of the combat rework) has nowhere natural to land.
- NPC target selection has no threat context — mobs cannot intelligently switch targets.
- Group membership conflated with combat participation — bystanders get kill credit.
- No support for multi-room combat, boss phase mechanics, or scripted combat scenarios.

Introducing `COMBAT_DATA` as a first-class entity solves all of these and is the prerequisite foundation for the encounter/threat model described in the combat rework plan.

---

## Core Concepts

### Combat vs. Engagement

A **combat** is a conflict between parties. It is defined by its participants, not its location. It persists across rooms, pauses, and re-engagements until it resolves.

An **engagement** is where that conflict is actively happening — a specific skirmish, potentially spanning a room cluster. A single combat can contain multiple simultaneous active engagements, as well as a history of past ones.

```
COMBAT_DATA  ←── the conflict (participant-scoped, persists)
  └── ENGAGEMENT_DATA[]  ←── all skirmishes, past and present
        ├── Engagement 1: Room 4412, rounds 1-4, ended: A fled north      [closed]
        ├── Engagement 2: Room 4412-4413, rounds 5-9, active...           [active]
        └── Engagement 3: Room 4415, rounds 6-7, active...                [active]
```

Multiple engagements can be active simultaneously — for example, when adds pursue a participant into a separate room while the main fight continues elsewhere. The combat tracks all of them; the pursuit window only opens when *all* active engagements have closed.

### Participant-Driven, Not Room-Driven

A combat is scoped to its participants, not to a room. Two separate fights in the same room remain two distinct `COMBAT_DATA` instances. They only merge if a participant from one takes a combat-relevant action targeting a participant from the other.

- Two duels in a tavern do not auto-merge.
- A bystander in a room with an active fight is not in that combat until they generate threat.
- The `COMBAT_DATA` carries no primary room field — room is tracked per `ENGAGEMENT_DATA`.

### Threat as the Participation Mechanism

Threat generation is both the merge trigger and the participation threshold. You enter a combat by taking an action that puts you on a hostile participant's threat table — attack, debuff, or healing/buffing an allied participant. Passively standing in a group while your allies fight does not pull you into the `COMBAT_DATA`. No threat generated = no participation = no kill credit or loot.

Group membership (`GROUP_DATA`) is irrelevant to `COMBAT_DATA`. The combat tracks who actually contributed.

### Multi-Room Engagements

Engagements are not strictly single-room. A participant may deliberately reposition to an adjacent room and continue participating in the same engagement via ranged or spell attacks. This is tactical positioning, not disengagement.

| Position | Available actions |
|---|---|
| Primary room | All — melee, ranged, spell |
| Adjacent room | Ranged and spell only; melee requires moving in |
| 2+ rooms away | Out of engagement; still in combat (disengage state) |

A participant who moves to an adjacent room with no valid actions from that position has effectively disengaged, not repositioned. The system determines this automatically — if no valid targets are reachable from the new room, the participant's `active` flag is cleared and the pursuit window opens.

### Ranged Combat Integration (Migration from `shoot.c`)

Current ranged flow is implemented as a delayed channel (`ch->ranged` with
`ranged_end()`), independent of engagement initiative. This must be absorbed
into `COMBAT_DATA` turn/action semantics.

#### Current Gaps to Close

- Ranged windup/resolve is pulse-driven outside combat initiative ordering.
- Target lock is name-based (`projectile_victim`), not participant identity.
- Direction/range search currently relocates the attacker during lookups
    (`char_from_room`/`char_to_room`), which is fragile and expensive.
- Multi-room ranged does not share the same reachability authority as
    engagement logic.
- Legacy code has duplication and drift (`ranged` countdown in both `comm.c`
    and `update.c`) and known correctness issues (e.g. throw-path traversal and
    blocker checks implemented in ad-hoc paths).

#### Target Model Under COMBAT_DATA

Ranged actions become first-class engagement actions:

1. **Aim start** records a combat target handle (`combat_id`, target
     `participant_id`, and origin room snapshot), not a raw name.
2. **Aim channel** consumes action budget/time in the participant's engagement.
3. **Resolve step** validates target identity + current reachability using the
     same engagement range rules as other actions.
4. If primary target is invalid, retargeting uses threat + companion tactical
     filters (where applicable), or the action is cancelled.

#### Reachability Rules (Shared Authority)

- Same room: all ranged profiles allowed.
- Adjacent room: ranged/spell profiles allowed per engagement range policy.
- Beyond allowed range: no fire; participant either repositions or disengages.

Ranged commands must call the same reachability evaluator used by
`combat_resolve_target()` so melee/ranged/casting cannot diverge in edge cases.

### Positioning Model: Front / Middle / Back (No Facing)

To align with existing group-row gameplay and avoid facing complexity, each
participant in an active engagement has a discrete row position:

- `ROW_FRONT`
- `ROW_MIDDLE`
- `ROW_BACK`

Row is engagement-scoped state (not room-global), so a participant can occupy
different tactical positions in different simultaneous engagements.

#### Row-Based Action Gates

- **Melee**: requires same room and target in reachable melee row (typically
    front vs front; selective reach abilities may include middle).
- **Reach/polearm**: same room, can target one row deeper than base melee.
- **Ranged/throw/spell**: same room all rows, plus adjacent-room targets if
    engagement range policy permits.
- **Shield/intercept/guard**: primarily front-row effects unless ability says
    otherwise.

#### Movement Between Rows

- Row change is an explicit combat action (costs action budget), not free.
- Forced movement effects (bash/push/pull) may change row and can break channels
    (including ranged aim).
- No facing: orientation is abstracted away; only row + room + reachability are
    evaluated.

#### Weapon Range Integration

Weapon profiles should define tactical range class used by the shared
reachability evaluator:

- `RANGE_MELEE`
- `RANGE_REACH`
- `RANGE_RANGED_SHORT`
- `RANGE_RANGED_LONG`

This range class combines with row + room distance to determine legal targets.
`shoot.c` migration should map existing weapon range values into these runtime
classes (compatibility layer first, then full data-driven profiles).

#### Short-Term Hardening Before Full Migration

While legacy `shoot.c` remains active, apply low-risk stabilization:

- Single owner for ranged countdown/update (remove duplicate tick paths).
- Replace room-hopping target search with pure room-walk helpers.
- Replace name-only projectile lock with stable target identity where possible.
- Normalize blocker/shield/cover checks into one reusable resolver.
- Add combat telemetry events for `aim_start`, `aim_interrupt`,
    `projectile_travel`, `projectile_hit`, `projectile_miss`.

---

## Data Structures

### COMBAT_DATA

```c
typedef struct combat_data COMBAT_DATA;

struct combat_data {
    COMBAT_DATA         *next;              /* global active list */
    bool                 valid;
    unsigned long        id[2];

    LLIST               *participants;      /* all PARTICIPANT_DATA ever in this combat */
    LLIST               *engagements;       /* all ENGAGEMENT_DATA, past and active */
    LLIST               *active_engagements;/* subset of engagements currently underway */
                                            /* empty = between engagements (pursuit window) */

    /* combat flags */
    bool                 no_merge;          /* cannot absorb or be absorbed by other combats */
    bool                 manual_terminate;  /* one or more mobs control termination */
    bool                 force_terminate;   /* script override: end immediately */

    int                  pursuit_timer;     /* pulses remaining before combat resolves naturally */
    bool                 pvp;              /* any PC vs PC participant */
    bool                 resolved;

    /* rolling telemetry */
    long                 start_time;
    long                 total_damage;
    int                  total_flees;
    int                  total_rounds;
};
```

### ENGAGEMENT_DATA

```c
typedef enum {
    ENGAGEMENT_END_ACTIVE    = 0,
    ENGAGEMENT_END_FLEE      = 1,
    ENGAGEMENT_END_DEATH     = 2,
    ENGAGEMENT_END_DISENGAGE = 3,   /* controlled exit */
} engagement_end_t;

typedef struct engagement_data ENGAGEMENT_DATA;

struct engagement_data {
    int                  number;        /* assigned at open time, monotonically incrementing */
                                        /* not positional — multiple may be active at once */
    ROOM_INDEX_DATA     *primary_room;  /* where main combatants are */
    LLIST               *rooms;         /* all rooms with active participants */
    LLIST               *participants;  /* subset active in this engagement */
    int                  round_count;
    long                 damage_dealt;
    long                 start_time;
    long                 end_time;
    engagement_end_t     end_reason;
};
```

### PARTICIPANT_DATA

```c
typedef struct threat_entry    THREAT_ENTRY;
typedef struct participant_data PARTICIPANT_DATA;

struct threat_entry {
    PARTICIPANT_DATA    *source;
    unsigned long        source_id[2];
    long                 threat;
};

struct participant_data {
    CHAR_DATA           *ch;
    unsigned long        ch_id[2];      /* validity mirror of ch->id */

    /* current engagement state */
    int                  initiative;
    bool                 acted_this_round;
    bool                 active;            /* in an active engagement */
    ENGAGEMENT_DATA     *engagement;        /* which engagement this participant is in */

    /* threat */
    LLIST               *threat_table;  /* THREAT_ENTRY list — who threatens this participant */
    long                 threat_generated; /* total threat generated this combat */

    /* death state */
    bool                 dead;          /* HP hit 0, not yet released */
    bool                 released;      /* chose death plane — remove from combat next tick */

    /* combat-wide history */
    int                  flee_count;
    int                  engagements_in;
    bool                 fled_last;
    long                 dmg_dealt;
    long                 dmg_taken;
    int                  rounds_total;
};
```

### CHAR_DATA additions

```c
COMBAT_DATA  *combat;    /* non-NULL if this char is in a combat */

/* ch->fighting is retained — immediate target pointer.
   ch->combat is the combat context. A char can be in a combat
   (combat != NULL) without currently fighting (fighting == NULL),
   e.g., stunned, repositioned, or between engagements. */
```

---

## set_fighting() Merge Logic

When `set_fighting(attacker, victim)` is called:

```
Let combat_a = attacker->combat  (NULL if not in any combat)
Let combat_b = victim->combat    (NULL if not in any combat)

Case A: both NULL
    → Create new COMBAT_DATA, add both participants, open Engagement 1.

Case B: one NULL, one non-NULL
    → If non-NULL combat has no_merge set, block and return false.
    → Otherwise join the existing COMBAT_DATA. Add uninvolved party as new participant.

Case C: both in the same COMBAT_DATA
    → Already merged. Update ch->fighting pointers only.
    → If attacker and victim share no active engagement, open a new one for them.
    → If active_engagements is empty (pursuit window), open a new engagement.

Case D: both in different COMBAT_DATA instances
    → If either has no_merge set, block and return false.
    → Merge: absorb smaller into larger (or arbitrary if equal).
    → Combine participant lists (no duplicates).
    → Combine active_engagements lists.
    → Combine telemetry totals.
    → If attacker and victim share no active engagement post-merge, open a new one.
```

Merging does not change any `ch->fighting` pointers. Each participant continues targeting whoever they were targeting. The `COMBAT_DATA` is the pool of potential conflict; who you're actively targeting is a separate decision.

---

## Threat

### Generation

Threat accumulates on hostile participants' threat tables via:

- Damage dealt: `threat += damage_amount`
- Heals given to an allied participant: `threat += heal_amount × factor` (applied to all hostile participants in the combat)
- Buffs applied to an allied participant: `threat += fixed_value`
- Other actions (debuffs, control effects): as defined per ability

Generating threat against a participant in another `COMBAT_DATA` is the merge trigger — the action that pulls you into their conflict.

### NPC Target Selection

NPCs select targets by querying their threat table for the highest-threat living participant within reachable range. "Reachable" means:

- Same room (melee and ranged both valid)
- Adjacent room (ranged/spell capable NPCs only)

NPCs may switch targets freely each round based on current threat rankings. This is what enables adds to hunt down the ranged caster in another room.

For companion NPCs (Tier 2-4 in `PLAN_PARTY_SYSTEM.md`), this selection is
filtered by tactical settings (stance, guard/focus target, target priority)
before final threat ranking, so party AI intent constrains target choice without
breaking core reachability/engagement rules.

### Player Target Switching

Players switch targets freely via targeting commands. Switching targets does not reset threat — threat already generated remains on existing threat tables.

---

## Disengage as a Spectrum

Flee and deliberate movement are both mechanisms for exiting the current engagement. The combat persists either way. The difference is speed, safety, and cost.

| Method | Speed | Exposure window | Can fail? | Flee count |
|---|---|---|---|---|
| `flee` | Immediate | Large — attacker gets a free strike | Yes (bashed, rooted) | +1 |
| Deliberate move | Costs round action | Small | No | +1 |
| `disengage` skill (future) | Moderate | Minimal or none | No (or rarely) | +1 |

**Flee is not a special escape hatch.** It is a fast, reckless disengage. The risk is the exposure window — if the attacker's opportunistic strike kills you, you do not escape. The move itself succeeds; surviving is the question.

**Movement in combat is always possible.** The current behavior where `ch->fighting` blocks `do_move()` must change. The cost is the action and the exposure window, not a hard block.

**Rooting effects** (bash, web, paralysis) become "cannot exit engagement" conditions rather than "cannot move" conditions, while active.

### Repositioning vs. Disengaging

Both involve movement. The distinction is whether you maintain valid combat actions from the new position:

- **Tactical repositioning:** Move to adjacent room, can still attack/cast the target. Still `active` in the engagement. No flee count. Exposure window still applies.
- **Disengaging:** Move somewhere with no valid combat actions. `active` cleared. Flee count +1. Pursuit window opens.

The system determines this automatically — no explicit player declaration needed.

---

## Pursuit

When a participant exits an engagement by moving to another room, any other participant may follow. If they attack the fled participant, `set_fighting()` recognizes both as existing participants in the same `COMBAT_DATA` and opens a new engagement for them — no new combat is created. Other engagements within the same combat continue unaffected.

Pursuit is not a special mechanic. It is movement followed by `set_fighting()`. Whether to pursue is:

- **Players:** Always their choice.
- **NPCs:** Determined by behavior profile (`pursue`, `territorial`, `hold_ground`, etc.). Binary for initial implementation; richer behavior belongs to the actor profile design.

NPC pursuit does not grant a free attack on arrival. Moving costs the NPC's action; the new engagement opens normally on the next round.

---

## Dead Participants

Dead participants are not removed from `COMBAT_DATA` on death. They remain in the participant list with `dead = true` and may:

- Choose to **release** (`released = true`) — moved to death plane on the next tick, removed from combat.
- **Wait** — remain in the combat in dead state until it resolves.

**Auto-release** occurs when:
- The combat resolves while the participant is dead.
- The participant dies solo (only surviving combatant on their side) — resolution condition immediately met, combat ends, auto-release.

This preserves the current death-plane mechanic. Dead participants do not act, do not appear in initiative order, and cannot generate threat. They remain in the participant list for loot and kill-credit accounting.

---

## Combat Resolution

Resolution condition:

```
Combat resolves when:

  force_terminate is set by script,

  OR all of the following are true:
    - No living participant pair (A, B) exists where A is hostile to B
      AND both are alive AND A can reach B (within engagement range)
    - All MANUAL_TERMINATE mobs have been checked and none vetoed
```

This is not "no active engagement" — it is "no one has anyone left to fight." A participant in an adjacent room with the last surviving boss still alive keeps the combat live even if everyone else in the boss's room is dead.

### Termination Sequence

When the resolution condition is met:

```
1. Fire TRIG_COMBAT_TERMINATE_CHECK on ALL mobs flagged MANUAL_TERMINATE
     → Each is checked independently; all must pass for termination to proceed
     → Any veto keeps combat alive; the vetoing mob may act (spawn, resurrect, etc.)
     → All pass → proceed to step 2

2. Combat terminates

3. Fire TRIG_COMBAT_RESET on all mobs flagged RESET_ON_TERMINATE
     → Mob resets HP, position, state per script (boss resets after party wipe)

4. Extract all mobs flagged EXTRACT_ON_TERMINATE
     → Adds and scripted mobs that should not persist after the fight

5. Auto-release all dead participants to death plane

6. Distribute kill credit and loot to all qualifying participants (COMBAT_DATA.participants)

7. Broadcast combat-end event to all participants regardless of room

8. COMBAT_DATA marked resolved; cleaned up after short delay
```

### Mob Combat Flags

| Flag | Behavior |
|---|---|
| `MANUAL_TERMINATE` | Mob gets `TRIG_COMBAT_TERMINATE_CHECK` when resolution would occur; can veto |
| `EXTRACT_ON_TERMINATE` | Mob is extracted from world when combat ends |
| `RESET_ON_TERMINATE` | Mob gets `TRIG_COMBAT_RESET` when combat ends; resets state |

These are flags on individual mobs, not on `COMBAT_DATA` itself. When any participant mob has `MANUAL_TERMINATE`, `COMBAT_DATA.manual_terminate` is set to `true` as a fast check before iterating.

---

## Entity Fuse and Split

Boss encounters may involve participants merging into a single entity or splitting into multiple.

**Fuse (A + B → C):**
- Remove A and B from `COMBAT_DATA` participant list (without triggering resolution check)
- Add C as new participant
- C inherits the combined threat tables of A and B (merged, summed by source)

**Split (A → B + C):**
- Remove A from participant list
- Add B and C as new participants
- Distribute A's threat table between B and C (by script-defined ratio, or split evenly)

The scripting API must support explicit participant manipulation: `combat_remove_participant()`, `combat_add_participant()`, `combat_transfer_threat()`. The resolution check is suppressed during fuse/split operations and re-evaluated after the new participants are in place.

---

## Combat Events and Notifications

All combat events broadcast to all participants regardless of room. Message flavor may differ by proximity:

- Same room as event: full message
- Adjacent room: attenuated version ("You hear a roar from the north!")
- Further / disengage state: minimal notification ("You sense something has changed in the battle.")

Key events that always reach all participants:
- Boss death / major kill
- Add spawns
- Phase transitions (via `MANUAL_TERMINATE` veto scripts)
- Combat resolution

This broadcast model is also what enables loot and kill-credit distribution to participants in other rooms.

---

## Kill Credit and Loot

`COMBAT_DATA.participants` is the authoritative list for kill credit and loot eligibility. All qualifying participants receive consideration regardless of which room they were in at time of kill.

**Qualifying participant:** must have generated at least some threat during the combat (`threat_generated > 0`). Exact minimum threshold is an open question (see OQ-16).

Loot distribution rules from `GROUP_DATA` apply to the qualifying participant set.

---

## violence_update() Under This Model

Currently `violence_update()` iterates all loaded characters checking `ch->fighting`. Under the new model it iterates the global `COMBAT_DATA` list:

```
for each COMBAT_DATA in active_combats:

    if combat->active_engagements is empty:
        decrement pursuit_timer
        if pursuit_timer <= 0:
            check_resolution(combat)
        continue

    for each ENGAGEMENT_DATA in active_engagements:

        build_initiative_order(engagement)   /* sort this engagement's participants */

        for each PARTICIPANT_DATA in initiative_order:
            if participant->dead or participant->released: continue
            if !participant->active: continue
            if participant->acted_this_round: continue

            combat_resolve_target(ch, participant->engagement)      /* validates reachability, threat retarget */
            combat_execute_turn_actions(ch)                          /* attack_table path or legacy multi_hit */
            check_assist(ch, ch->fighting)
            fire_combat_triggers(ch)
            participant->acted_this_round = true

        engagement->round_count++

    combat->total_rounds++
    reset acted_this_round for all participants across all active engagements
```

Only combat participants are processed, not all loaded characters.

---

## Initiative

Initiative ordering replaces the current arbitrary linked-list traversal.

Each round:
1. Roll per active participant: `d20 + speed_modifier + haste_bonus - fatigue_penalty`
2. Sort descending.
3. Characters joining mid-round go to end of order for that round.
4. Participants who fled most recently take an initiative penalty next engagement (`fled_last = true`).

Initiative is a round-level concept — it rerolls each round.

---

## Spawned Adds

When a boss script spawns adds mid-combat, the adds are automatically added to the parent `COMBAT_DATA` as participants. The mechanism:

- Script spawns add via existing mob-creation path
- Script explicitly calls `combat_add_participant(combat, add)`, or
- `set_fighting(add, target)` is called and recognizes target is in an existing combat, joining it

The add's threat table starts empty. It accumulates threat immediately as it acts. NPC targeting uses the threat table to select from the full participant pool, including participants in adjacent rooms.

---

## Backward Compatibility

- `ch->fighting` is retained and kept valid throughout. Scripts and triggers that read `ch->fighting` continue to work.
- Scripts that set `ch->fighting` directly (bypassing `set_fighting()`) need an audit — they create fighting relationships without `COMBAT_DATA` context.
- `stop_fighting()` must be updated to also remove the character from `COMBAT_DATA` or mark them inactive, and close engagements if applicable.
- All existing combat triggers (`TRIG_START_COMBAT`, `TRIG_FIGHT`, `TRIG_FLEE`, `TRIG_PREROUND`, etc.) continue to fire at their existing points. `COMBAT_DATA` context can be added as an additional argument over time.
- `NO_MERGE` blocks the merge path in `set_fighting()` — existing scripts that call `set_fighting()` across combat boundaries will fail cleanly rather than silently merging.

---

## Combat Recording

`COMBAT_DATA` and `ENGAGEMENT_DATA` accumulate a structured history of a fight by design. Combined with the unified combat event pipeline planned in `PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md` (Section E), this enables full combat recording and replay.

If each engagement maintains a structured event log alongside its summary fields, individual actions (attacks, misses, spells, flees, deaths) can be replayed in order. The event log is the structured form of what currently flows through `dam_message()` and `act()` — the messaging unification work and the recording work are the same pipeline.

Use cases: player post-mortems, balance telemetry, admin investigation, boss-script decision making ("if fire damage exceeded 30% of total, trigger immunity phase"), and tutorial replay.

See `docs/guides/COMBAT_ROUND_WALKTHROUGH.md` (Future State section) for the `combat_event` struct sketch and storage considerations.

---

## Telemetry Hooks (Phase 0)

`COMBAT_DATA` is the natural home for the Phase 0 metrics from `PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md`:

| Metric | Source |
|---|---|
| Median / percentile damage per round | `ENGAGEMENT_DATA.damage_dealt / round_count` per engagement |
| Fight duration by content tier | `COMBAT_DATA.total_rounds`, `start_time` |
| Concurrent engagement count | `active_engagements` list length peak per combat |
| Flee frequency and re-engage timing | `COMBAT_DATA.total_flees`, engagement gap timestamps |
| Burst spike distribution | Per-round damage snapshots |
| Multi-room pursuit rate | Engagements with different rooms than Engagement 1 |
| Dispel impact | Tracked in engagement events |
| Participation distribution | `threat_generated` per participant |

---

## Open Questions

### OQ-1: Pursuit window length
How many `PULSE_VIOLENCE` beats should a combat stay live between engagements? Current thinking: 2–3 pulses (~1.5–2.5 seconds). Long enough for a pursuer moving normally to catch up; short enough to not ghost.

### OQ-2: Who can open a new engagement within an existing combat?
Only existing participants? Or can a third party attacking an existing participant be absorbed into this combat's next engagement? The latter enables ambush scenarios but complicates `NO_MERGE` semantics.

### OQ-3: Three-way and multi-sided combats
Three-way fight: one `COMBAT_DATA` or multiple sharing participants? Current merge rule (participant-driven) naturally collapses three-ways into one combat once any cross-engagement occurs. Is this always desirable?

### OQ-4: Mutual disengage
Can both sides agree to end an engagement cleanly with no exposure window? Useful for duel/arena contexts and NPC behavior that decides combat isn't worth continuing.

### OQ-5: NPC pursuit behavior granularity
Binary (pursue / don't pursue) for initial implementation. Richer behavior (pursue to N rooms, pursue if HP above threshold) deferred to actor behavior profile design.

### OQ-6: do_move() action cost
When deliberately moving out of an engagement: lose current-round action (if not yet acted), or take initiative penalty next round (if already acted)? Likely both based on timing.

### OQ-7: Disengage skill design
General ability at some base level for all, with class skills improving it (reducing exposure window), or class-specific only? Exact class mapping deferred to class/role identity work.

### OQ-8: Merge trigger — beneficial actions
Threat generation is the merge trigger. If A heals C (in another combat), A generates threat on C's enemies, which merges A's combat into C's. This is the clean definition. Edge case: does applying a buff that generates zero threat (e.g., detect magic) trigger a merge? Answer: only actions that generate threat merge.

### OQ-9: Adjacent room range rules
What defines "adjacent" for engagement purposes? Direct exit to primary room? Line-of-sight? Terrain blocking range? Needs a concrete rule before implementation.

### OQ-10: Add spawn inheritance
When a boss script spawns adds mid-combat, does the script call `combat_add_participant()` explicitly, or does `set_fighting()` handle it when the add first attacks? Explicit call is safer and more predictable.

### OQ-11: Broadcast message filtering
Same-room gets full message; adjacent room gets attenuated version; further gets minimal notification. Exact message templates and distance thresholds need definition.

### OQ-12: Threat generation formula
Exact values for damage-to-threat conversion, heal-to-threat conversion, and buff/debuff flat values. Does threat decay over time or persist for the full combat? Does switching targets affect threat in any way?

### OQ-13: NO_MERGE scope
Flag lives on the mob — any combat involving this mob cannot merge. `COMBAT_DATA.no_merge` is derived from participant flags when the combat is created. Is `no_merge` symmetric (blocks all merges in both directions) or one-way?

### OQ-14: MANUAL_TERMINATE trigger interface
What does `TRIG_COMBAT_TERMINATE_CHECK` receive and what can it return? What actions are permitted within the trigger (spawn, resurrect, modify participants)? Needs a defined scripting API before implementation.

### OQ-15: Entity fuse/split scripting API
Exact function signatures for `combat_remove_participant()`, `combat_add_participant()`, `combat_transfer_threat()`. How is threat distributed on split — even, script-defined ratio, or by percentage of remaining HP?

### OQ-16: Participation minimum for spoils
Is any threat generation sufficient, or is there a minimum threshold? Edge case: player casts one spell and immediately dies — do they qualify? Recommendation: any non-zero threat generation qualifies, but open to adjustment.

### OQ-17: RESET_ON_TERMINATE script scope
What can `TRIG_COMBAT_RESET` modify? Full HP/mana/move reset, position reset, affect purge? Should this be a trigger with full script access or a fixed reset behavior defined by mob flags?

---

## Relationship to Other Plans

| Document | Relationship |
|---|---|
| `PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md` | This implements Phase 4 (Encounter and Threat Foundation); telemetry structure feeds Phase 0 |
| `PLAN_DATA_DRIVEN_TABLES.md` | Attack-pattern execution consumes engagement reachability/targeting from COMBAT_DATA; cooldowns persist across engagements within a combat |
| `PLAN_CORE_SYSTEMS_INTEGRATION.md` | Required before Gate III (Identity Activation) — companion behavior profiles need a combat context |
| `PLAN_PARTY_SYSTEM.md` | Tier 2+ companion AI needs `COMBAT_DATA` for targeting and behavior decisions |
| `PLAN_CASTING_SYSTEM_REWORK.md` | Channel/interrupt semantics operate inside an engagement; engagement state informs interruption timing |
