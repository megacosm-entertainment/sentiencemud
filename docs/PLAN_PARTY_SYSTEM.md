# Plan: Tiered Party System

**Date:** February 8, 2026
**Status:** Early Design / Sketch

---

## Motivation

Sentience is shifting toward a party-based game where players travel with NPC companions of varying capability and attachment. Rather than the current flat follower/pet model, party members are organized into tiers that define their autonomy, customizability, and relationship to the player.

This builds on the `GROUP_DATA` refactoring proposed in `PLAN_group_analysis.md`, which replaces the implicit leader-pointer grouping with an explicit group entity.

---

## Tier Definitions

### Tier 0 — Temporary NPCs

Disposable, context-specific NPCs that tag along for a limited purpose.

- **Examples:** Escort quest targets, rescued prisoners, quest guides
- **Lifespan:** Duration of quest or event; removed when objective ends
- **Control:** None — they follow and may have scripted behavior, but the player has no commands over them
- **Combat:** May participate passively (hiding behind the group) or not at all
- **Persistence:** None — not saved to the player file
- **Customization:** None
- **Source:** Spawned by quest/script triggers

### Tier 1 — Pets & Summons

The current pet system, formalized. Loyal creatures bound to the player.

- **Examples:** Tamed animals, summoned familiars, bought pets
- **Lifespan:** Until dismissed, killed, or player logs out (depending on type)
- **Control:** Basic commands — follow, stay, attack, heel
- **Combat:** Fights alongside the player using its base stats
- **Persistence:** Saved with the player (as today)
- **Customization:** Minimal — name, possibly basic gear (collar/saddle)
- **Source:** Pet shops, tame skill, summon spells
- **Limit:** 1-2 active at a time

### Tier 2 — Henchmen

Hired professionals with their own identity. The player pays for their services but doesn't shape who they are.

- **Examples:** Hired guard, mercenary swordsman, traveling healer, torchbearer
- **Lifespan:** Until contract expires, dismissed, or killed
- **Control:** Limited tactical commands — aggressive/defensive/passive stance, guard target, focus target
- **Combat:** Has a class, level, and a subset of skills appropriate to their role. Acts semi-autonomously using their own AI routines
- **Persistence:** Contract terms saved to player file; the henchman template is world data
- **Customization:** None — you hire what's available. Different henchmen have different capabilities
- **Source:** Taverns, mercenary guilds, quest rewards
- **Cost:** Ongoing gold drain (wages) or a flat contract fee
- **Limit:** 1-2 active, scaling with player level or reputation

### Tier 3 — Heroes

Named NPCs with a personal bond to the player. The party members you care about.

- **Examples:** A rescued knight who swears allegiance, a mage's apprentice, a childhood friend NPC
- **Lifespan:** Permanent (until story event or dismissal)
- **Control:** Full tactical commands — stance, target priority, skill usage preferences, formation position
- **Combat:** Has a full class, skills, and levels. Can gain XP and level up alongside the player
- **Persistence:** Full save to player file — stats, gear, skills, progression
- **Customization:**
  - Equipment: Player can manage their gear (with some restrictions)
  - Skills: Player can choose skill focus / training priorities
  - Tactics: Detailed AI behavior configuration
  - Appearance: Basic cosmetic options
- **Source:** Quest rewards, story milestones, reputation unlocks
- **Limit:** 2-3 in the player's roster, 1-2 active in party at a time

### Tier 4 — Alts-as-Heroes

A player's own alternate character, manifested as an NPC companion.

- **Examples:** Player's alt "Kael the Ranger" joins their main's party as an AI-controlled companion
- **Lifespan:** Until dismissed; always available for re-summoning
- **Control:** Tactical commands only (stance, target priority, formation) — no gear or skill changes, since the alt's build is defined by its real character data
- **Combat:** Generated NPC based on the alt's actual stats, gear, skills, and level at time of summoning (snapshot)
- **Persistence:** Reference to the alt character saved; NPC is regenerated on summon
- **Customization:** Tactics only — the alt's gear/skills are whatever the player set them to when last playing that character
- **Source:** Player's own account — must have an alt of sufficient level
- **Restrictions:**
  - Cannot summon an alt that is currently logged in
  - Alt's gear is "locked" while summoned (no transferring items to yourself)
  - Snapshot is refreshed each time the alt is summoned, not live-updated
  - Level restrictions to prevent a level 90 alt carrying a level 5 main
- **Limit:** 1 active at a time

---

## Architecture

### Party Slots

A player's party has a fixed number of **slots** determined by level, skills, or perks. Slots are tier-agnostic — a slot can hold any tier of companion, but higher-tier companions may consume more slots.

```
Party Capacity: 4 slots (example at mid-level)

  Slot 1: [Tier 3] Sera the Healer (Hero)        — 1 slot
  Slot 2: [Tier 2] Hired Mercenary                — 1 slot
  Slot 3: [Tier 1] Wolf Pet                       — 1 slot
  Slot 4: [Tier 0] Escort: Merchant Aldric        — 0 slots (Tier 0 doesn't consume slots)
```

Possible slot costs:
| Tier | Slot Cost | Rationale |
|------|-----------|-----------|
| 0 | 0 | Temporary, no combat value |
| 1 | 1 | Basic combat participant |
| 2 | 1 | Capable but not player-shaped |
| 3 | 1-2 | Powerful, customizable |
| 4 | 2 | Effectively a second player character |

### Relationship to GROUP_DATA

The `GROUP_DATA` refactoring from `PLAN_group_analysis.md` provides the foundation. The party system layers on top:

```
GROUP_DATA                          (group entity with unique_id)
  ├── leader: CHAR_DATA *           (the player)
  ├── members: list                 (all grouped characters)
  ├── settings: loot/xp rules
  │
  └── party: PARTY_DATA *           (new — the player's NPC companions)
        ├── capacity: int           (total slots)
        ├── used_slots: int
        └── companions[]
              ├── COMPANION_DATA    (Tier 3 Hero: Sera)
              ├── COMPANION_DATA    (Tier 2 Henchman: Mercenary)
              └── COMPANION_DATA    (Tier 1 Pet: Wolf)
```

`GROUP_DATA` handles inter-player grouping (multiple PCs). `PARTY_DATA` handles a single player's NPC companions. They coexist: a player can have a party of NPCs and also be in a group with other players (who each have their own parties).

### COMPANION_DATA

```c
typedef struct companion_data {
    int              tier;            /* COMPANION_TIER_0 through _4 */
    int              slot_cost;
    CHAR_DATA       *ch;              /* the live NPC in the world */
    char            *name;            /* display name */

    /* Tier-specific data */
    union {
        struct {
            /* Tier 0: quest reference */
            long quest_id;
        } escort;

        struct {
            /* Tier 1: pet data (mostly what we have today) */
            long mob_vnum;
        } pet;

        struct {
            /* Tier 2: henchman contract */
            long template_vnum;       /* base mob template */
            int  contract_duration;   /* pulses remaining, or -1 for permanent */
            long wage_cost;           /* gold per game day */
            int  stance;              /* aggressive/defensive/passive */
        } henchman;

        struct {
            /* Tier 3: hero progression */
            long         template_vnum;
            int          hero_level;
            int          hero_xp;
            int          class_id;
            LLIST       *skills;       /* subset of skills learned */
            int          stance;
            int          target_pref;  /* target priority setting */
            json_t      *save_data;    /* full persistent state */
        } hero;

        struct {
            /* Tier 4: alt reference */
            long         account_id;
            char        *char_name;    /* which alt */
            int          stance;
            int          target_pref;
            /* NPC is generated from char data at summon time */
        } alt;
    } data;
} COMPANION_DATA;
```

### NPC AI by Tier

Each tier has different AI behavior, ranging from dumb to smart:

| Tier | AI Behavior |
|------|-------------|
| 0 | Follow leader. Flee or cower in combat. Script-driven actions only. |
| 1 | Follow leader. Attack leader's target. No skill use beyond basic melee/spell. |
| 2 | Role-based AI. Healer henchmen heal. Tank henchmen taunt. Uses a small skill pool based on class. Responds to stance commands. |
| 3 | Full tactical AI. Uses skills intelligently based on player-configured priorities. Responds to all tactical commands. Can be set to focus healing, DPS, crowd control, etc. |
| 4 | Same AI as Tier 3, but skill/gear loadout is fixed to the alt's actual build. |

AI routines could live in a dedicated `party_ai.c` or integrate with the existing `special.c` / scripting system.

---

## Commands

### Core Party Commands

```
party list                     — Show current party members, tiers, and slots
party dismiss <name>           — Remove a companion from the party
party summon <alt-name>        — Summon an alt as Tier 4 companion
party tactics <name> <setting> — Set companion AI behavior
party stance <name> <stance>   — Set aggressive/defensive/passive
```

### Tier-Specific Interactions

```
hire <npc>                     — Hire a henchman (Tier 2) from a tavern/guild
pet <command>                  — Existing pet commands, now routed through party
hero equip <name> <item>       — Manage a Hero's gear (Tier 3)
hero skills <name>             — View/configure a Hero's skill priorities (Tier 3)
hero train <name> <skill>      — Spend XP to train a Hero's skill (Tier 3)
```

---

## Persistence

| Tier | What's Saved | Where |
|------|-------------|-------|
| 0 | Nothing — recreated by quest/script state | Quest system |
| 1 | Pet mob vnum, name, basic stats | Player file (as today) |
| 2 | Contract terms, template vnum, remaining duration | Player file, `"companions"` section |
| 3 | Full state: level, XP, skills, gear, tactics | Player file, `"companions"` section (large) |
| 4 | Alt character name reference, stance, tactics | Player file, `"companions"` section (small — data lives in the alt's char file) |

This aligns with the module serialization approach from `PLAN_MODULE_SYSTEM.md` — the party system would own its own `to_json()` / `from_json()` for the `"companions"` section of the player file.

---

## XP and Rewards

Party composition affects XP distribution:

- **Base rule:** XP is divided among all combat-participating party members (Tiers 1-4)
- **Tier 0:** Does not share XP (non-combatant)
- **Tier 1 (Pets):** Takes a reduced XP share (e.g., 50%) — prevents XP drain
- **Tier 2 (Henchmen):** Takes a full share — this is the tradeoff for their cost
- **Tier 3 (Heroes):** Takes a full share — but they level up, so it's an investment
- **Tier 4 (Alts):** Takes a full share — but the XP is discarded (the alt earns XP by being played, not by being summoned)

Loot rules are controlled by `GROUP_DATA` settings and apply uniformly to all participants.

---

## Companion Death & Recovery

Standard NPCs hit `extract_char(victim, true)` in `raw_kill` and are permanently removed from the world. Companions at Tier 2+ should not work this way — they should emulate the player death system instead, entering a recoverable death state rather than being destroyed.

### Player Death Model (Current)

When a PC dies in `raw_kill` (`fight.c#L3895-L3952`):

1. Stats set to 1 hp/mana/move
2. `victim->dead = true`
3. Death timer set: `time_left_death = MINS_PER_DEATH + 1`
4. Healed to full (ghost form has full stats)
5. Moved to `room_death`
6. Given fly/detect invis/detect hidden/infravision (ghost abilities)
7. Carried/worn items marked `ITEM_UNSEEN`
8. `TRIG_AFTERDEATH` fires
9. Timer decrements in `update.c` until resurrection

The key: the PC is never extracted. They persist in a ghost state until the timer expires or they are resurrected.

### Persistent Mob Model (Current)

Persistent mobs (`ch->persist = true`) survive reboots via JSON serialization in `data/persist/mobiles/`. They are full `CHAR_DATA` instances with saved inventory, equipment, and script variables. Currently used for ship crews and similar permanent NPCs.

### Companion Death: Combining Both Models

Tier 2-3 companions combine these: they are **persistent mobs** (survive reboots, have saved state) that **die like players** (ghost state + recovery) rather than being extracted.

#### Death Flow by Tier

| Tier | On "Death" | Recovery | Permanent Destruction |
|------|-----------|----------|----------------------|
| 0 | `extract_char` — standard mob death | None — they're disposable | N/A |
| 1 | `extract_char` — standard mob death (current pet behavior) | Re-summon or buy a new one | N/A |
| 2 | Ghost state (like PC) | Auto-recovery after timer, or pay to resurrect faster at a healer | Contract expires while dead = permanent loss. Otherwise, revives at the henchman's guild/origin |
| 3 | Ghost state (like PC) | Timer-based or quest-based resurrection. Hero returns to a "sanctuary" location | Only via explicit story event or player choice (`party destroy <name>`) |
| 4 | Dismissed — the alt NPC is removed | Re-summon the alt | N/A — the alt character still exists |

#### Implementation in raw_kill

The `IS_NPC(victim)` branch at `fight.c#L3840-3848` currently does:
```c
if (IS_NPC(victim))
{
    victim->pIndexData->killed++;
    p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, corpse, NULL, TRIG_AFTERDEATH, NULL);
    extract_char(victim, true);
    return corpse;
}
```

For companions, this needs a check before extraction:

```c
if (IS_NPC(victim))
{
    victim->pIndexData->killed++;

    if (victim->companion && victim->companion->tier >= COMPANION_TIER_2) {
        /* Companion death — enter ghost state like a player */
        companion_death(victim, corpse);
        return corpse;
    }

    p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, corpse, NULL, TRIG_AFTERDEATH, NULL);
    extract_char(victim, true);
    return corpse;
}
```

`companion_death()` would mirror the PC death path:
```c
void companion_death(CHAR_DATA *victim, OBJ_DATA *corpse) {
    COMPANION_DATA *comp = victim->companion;

    victim->dead = true;
    victim->hit  = victim->max_hit;
    victim->mana = victim->max_mana;
    victim->move = victim->max_move;

    /* Move to a recovery location based on tier */
    ROOM_INDEX_DATA *recovery = companion_get_recovery_room(comp);
    char_from_room(victim);
    char_to_room(victim, recovery);

    /* Set recovery timer — henchmen recover faster than heroes */
    if (comp->tier == COMPANION_TIER_2)
        victim->time_left_death = MINS_PER_DEATH / 2;
    else
        victim->time_left_death = MINS_PER_DEATH;

    /* Notify the owner */
    if (comp->owner && comp->owner->desc) {
        printf_to_char(comp->owner,
            "{R%s has fallen in battle and is recovering.{x\n\r",
            victim->short_descr);
    }

    /* Persist the state — don't lose this companion to a crash */
    persist_save_mobile(victim);

    p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL,
        corpse, NULL, TRIG_AFTERDEATH, NULL);
}
```

#### Recovery Locations

| Tier | Recovery Room |
|------|--------------|
| 2 | The guild/tavern where the henchman was hired |
| 3 | A hero-specific "sanctuary" room (home base, temple, etc.) — stored in `COMPANION_DATA` |

The companion waits at the recovery location until:
- The death timer expires (auto-recovery)
- The player visits and pays for resurrection
- The player uses a `party recall <name>` command (costs resources)

#### Persistence Across Reboots

Since Tier 2-3 companions use the persistent mob system, their death state survives reboots. If the server crashes while a hero is dead, they'll still be in the recovery room with their death timer when the server comes back up. The `companion` pointer on `CHAR_DATA` would need to be serialized and restored as part of the persistent mob save/load cycle.

#### Henchman Contract + Death Interaction

If a henchman dies and their contract has a duration:
- The death timer ticks down, but so does the contract timer
- If the contract expires while the henchman is dead, they are permanently dismissed (extracted on next update tick)
- The player can renew the contract at the guild to prevent this

This creates an interesting cost dynamic: letting a henchman die too often eats into the contract you're paying for.

---

## Open Questions

1. **Party size scaling:** How does party capacity grow? Per-level? Via skills/perks? Class-dependent (e.g., a "Commander" class gets more slots)?

2. **Hero acquisition:** How are Tier 3 Heroes acquired? Purely quest rewards, or can they be recruited from the world? Can they die permanently?

3. **Henchman economy:** How do wages scale? Is there a market of available henchmen, or fixed NPCs in specific locations? Can henchmen be poached by other players?

4. **Alt snapshot staleness:** When a Tier 4 alt is summoned, it snapshots the alt's current state. If the player logs into the alt and changes gear, the summoned NPC doesn't update until re-summoned. Is this acceptable, or should there be a refresh mechanism?

5. **PvP implications:** Can companions participate in PvP? Tier 4 alts in PvP raises significant balance concerns.

6. **Multi-player party balance:** When two players group together and each has a full party of NPCs, the encounter could involve 8-10 entities. How does combat scaling handle this?

7. **Existing pet system migration:** The current `ch->pet` pointer and pet shops would need to be wrapped into the Tier 1 companion system. Is this a clean migration, or does the pet system need deeper changes first?

---

## Relationship to Other Plans

| Document | Relationship |
|----------|-------------|
| `PLAN_group_analysis.md` | Strong dependency for full party/group features, but **not a blocker** for quest/mission foundation delivery (character-scope-first) |
| `PLAN_MODULE_SYSTEM.md` | Party system is a natural module candidate (`sentience_party`); companion serialization follows module-owned persistence |
| `PLAN_SKILL_REFACTOR.md` | Tier 2-3 companions need the skill system to be data-driven (JSON skills) so their skill subsets can be configured |
| `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md` | Tier 2-3 companions need classes; the class/job system defines what's available |
