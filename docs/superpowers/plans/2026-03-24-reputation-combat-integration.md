# Reputation Combat Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the reputation/faction system into combat so mobs award reputation on kill and faction standing affects NPC hostility/peace.

**Architecture:** Add explicit faction membership to mob prototypes via `MOB_FACTION_DATA` linked list on `MOB_INDEX_DATA`. Populate `CHAR_DATA.factions` LLIST at mob creation time. Implement 4 stub functions in `reputation.c` and wire them into `fight.c` (`is_safe()` and `group_gain()`).

**Tech Stack:** C, Jansson (JSON), existing LLIST/linked list patterns, Sentience MUD test framework.

**DO NOT touch `src/tests/` files** — another agent session owns test infrastructure. You may ADD new test JSON data files and test handler code, but do not modify existing test files.

---

## Reference: Key Data Structures

```c
// MOB_INDEX_DATA.mob_reputations — what rep rewards killing this mob gives
struct mob_reputation_data {
    MOB_REPUTATION_DATA *next;
    bool valid;
    REPUTATION_INDEX_DATA *reputation;
    WNUM_LOAD reputation_load;
    int16_t minimum_rank, maximum_rank;
    long points;
};

// CHAR_DATA.factions — LLIST of REPUTATION_INDEX_DATA* (which factions NPC belongs to)
// CHAR_DATA.reputations — LLIST of REPUTATION_DATA* (player standings)
// CHAR_DATA.mob_reputations — MOB_REPUTATION_DATA* chain (copied from prototype)
```

## Reference: Existing API

```c
// Reputation lookup/manipulation (reputation.c)
REPUTATION_DATA *find_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex);
REPUTATION_DATA *set_reputation_char(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, int16_t rank, long rep, bool show);
bool gain_reputation(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex, long amount, int *change, long *total_given, bool show);
bool is_reputation_rank_peaceful(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex);
bool is_reputation_rank_hostile(CHAR_DATA *ch, REPUTATION_INDEX_DATA *repIndex);

// Index lookup (reputation.c)
REPUTATION_INDEX_DATA *get_reputation_index(AREA_DATA *area, long vnum);
REPUTATION_INDEX_DATA *get_reputation_index_auid(long auid, long vnum);

// List functions (handler.c)
LLIST *list_create(bool purge);
bool list_appendlink(LLIST *lp, void *data);
bool list_hasdata(LLIST *lp, void *ptr);
void list_destroy(LLIST *lp);
int list_size(LLIST *lp);

// Widevnum helpers
bool parse_widevnum(const char *arg, AREA_DATA *area, WNUM *wnum);
bool parse_widevnum_load(const char *str, WNUM_LOAD *load);
const char *widevnum_string(AREA_DATA *area, long vnum, AREA_DATA *relative);
```

## Reference: src_20_dev Implementations

The reference implementations from `src_20_dev/reputation.c` are the target behavior:
- `group_gain_reputation()` (lines 662-710) — iterates group, checks rank eligibility, awards points
- `check_mob_factions()` (lines 393-407) — initializes player standing with NPC factions
- `check_mob_factions_peaceful()` (lines 408-430) — returns true if any NPC faction is peaceful
- `check_mob_factions_hostile()` (lines 431-454) — returns true if any NPC faction is hostile

Key difference: src_20_dev uses `victim->factions` (LLIST of REPUTATION_INDEX_DATA*) which we need to populate.

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `merc.h` | Modify | Add MOB_FACTION_DATA struct, typedef, add field to MOB_INDEX_DATA |
| `recycle.h` | Modify | Declare new/free/copy_mob_faction_data |
| `mem.c` | Modify | Implement new/free/copy_mob_faction_data, free pool, fix free_char/free_mob_index |
| `io/json/json_area.c` | Modify | Serialize/deserialize mob factions |
| `db.c` | Modify | Resolve faction WNUM_LOAD in fix_area_fields, populate CHAR_DATA.factions in create_mobile |
| `editors/mobiles/medit.c` | Modify | Add addfaction/delfaction OLC commands |
| `reputation.c` | Modify | Implement 4 stub functions |
| `fight.c` | Modify | Wire group_gain_reputation + peaceful check |

---

### Task 1: Data Structures — MOB_FACTION_DATA

**Files:**
- Modify: `src/merc.h` (near MOB_REPUTATION_DATA at line ~5155)
- Modify: `src/merc.h` (MOB_INDEX_DATA at line ~4260)

- [ ] **Step 1: Add MOB_FACTION_DATA typedef**

In `merc.h`, find the typedef section (near line 490 where MOB_REPUTATION_DATA is typedef'd). Add:

```c
typedef struct mob_faction_data MOB_FACTION_DATA;
```

- [ ] **Step 2: Add MOB_FACTION_DATA struct definition**

In `merc.h`, after the `mob_reputation_data` struct (after line ~5167), add:

```c
struct mob_faction_data
{
    MOB_FACTION_DATA *next;
    bool valid;

    REPUTATION_INDEX_DATA *faction;
    WNUM_LOAD faction_load;
};
```

- [ ] **Step 3: Add factions field to MOB_INDEX_DATA**

In `merc.h`, in `struct mob_index_data` (near line 4260 where `mob_reputations` is), add after `mob_reputations`:

```c
    MOB_FACTION_DATA *factions;
```

- [ ] **Step 4: Build to verify**

```bash
cd /sentience/src && ./build
```
Expected: Clean compile (new struct/field not yet referenced).

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(reputation): add MOB_FACTION_DATA struct and mob index field"
```

---

### Task 2: Memory Management

**Files:**
- Modify: `src/recycle.h` (near line ~229 where mob_reputation_data funcs are declared)
- Modify: `src/mem.c` (near line ~4087 where mob_reputation_data funcs are implemented)

- [ ] **Step 1: Add declarations to recycle.h**

After the `free_mob_reputation_data` declaration (line ~231), add:

```c
MOB_FACTION_DATA *new_mob_faction_data(void);
MOB_FACTION_DATA *copy_mob_faction_data(MOB_FACTION_DATA *src);
void free_mob_faction_data(MOB_FACTION_DATA *data);
```

- [ ] **Step 2: Add free pool variable to mem.c**

Near line 93 where `MOB_REPUTATION_DATA *mob_reputation_free;` is declared, add:

```c
MOB_FACTION_DATA *mob_faction_free;
```

- [ ] **Step 3: Implement new/copy/free functions in mem.c**

After `free_mob_reputation_data()` (after line ~4137), add:

```c
MOB_FACTION_DATA *new_mob_faction_data(void)
{
    MOB_FACTION_DATA *data;

    if (!mob_faction_free)
    {
        data = alloc_perm(sizeof(*data));
    }
    else
    {
        data = mob_faction_free;
        mob_faction_free = mob_faction_free->next;
    }

    memset(data, 0, sizeof(*data));
    VALIDATE(data);
    return data;
}

MOB_FACTION_DATA *copy_mob_faction_data(MOB_FACTION_DATA *src)
{
    MOB_FACTION_DATA *data;

    if (!IS_VALID(src))
        return NULL;

    data = new_mob_faction_data();
    if (!data)
        return NULL;

    data->faction = src->faction;
    data->faction_load = src->faction_load;
    data->next = NULL;

    return data;
}

void free_mob_faction_data(MOB_FACTION_DATA *data)
{
    if (!IS_VALID(data))
        return;

    INVALIDATE(data);
    data->next = mob_faction_free;
    mob_faction_free = data;
}
```

- [ ] **Step 4: Fix free_mob_index to free factions**

In `free_mob_index()` (mem.c ~line 2561), after the mob_reputations cleanup loop and before `free_questor_data`, add:

```c
    {
        MOB_FACTION_DATA *fac, *fac_next;
        for (fac = pMob->factions; fac != NULL; fac = fac_next)
        {
            fac_next = fac->next;
            free_mob_faction_data(fac);
        }
        pMob->factions = NULL;
    }
```

- [ ] **Step 5: Fix free_char LLIST leaks**

In `free_char()` (mem.c ~line 1014), after `list_destroy(ch->lstache)`, add:

```c
    list_destroy(ch->reputations);
    ch->reputations = NULL;
    list_destroy(ch->factions);
    ch->factions = NULL;
```

- [ ] **Step 6: Initialize factions LLIST in new_char**

In `new_char()` (mem.c ~line 842), after `ch->lstache = list_create(false);`, add:

```c
    ch->factions = list_create(false);
```

Note: `ch->reputations` is lazily initialized in `set_reputation_char()` — keep that pattern.

- [ ] **Step 7: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "feat(reputation): memory management for MOB_FACTION_DATA + fix LLIST leaks"
```

---

### Task 3: JSON Persistence

**Files:**
- Modify: `src/io/json/json_area.c` (near mob reputation serialization at line ~4929 and ~5245)

- [ ] **Step 1: Add faction serialization (save)**

In `json_area.c`, after the `reputation_rewards` serialization block (after line ~4955), add:

```c
    if (mob->factions)
    {
        json_t *faction_array = json_array();
        MOB_FACTION_DATA *fac;

        for (fac = mob->factions; fac; fac = fac->next)
        {
            if (!IS_VALID(fac->faction))
                continue;

            json_array_append_new(faction_array,
                json_string(widevnum_string(fac->faction->area,
                                            fac->faction->vnum,
                                            mob->area)));
        }

        if (json_array_size(faction_array) > 0)
            json_object_set_new(json, "factions", faction_array);
        else
            json_decref(faction_array);
    }
```

- [ ] **Step 2: Add faction deserialization (load)**

In `json_area.c`, after the `reputation_rewards` deserialization block (after line ~5266), add:

```c
        json_t *factions_json = json_object_get(json, "factions");
        if (json_is_array(factions_json))
        {
            MOB_FACTION_DATA *last_fac = NULL;
            size_t fac_idx;
            json_t *fac_val;

            json_array_foreach(factions_json, fac_idx, fac_val)
            {
                const char *fac_ref = json_string_value(fac_val);
                if (!fac_ref || !*fac_ref)
                    continue;

                WNUM_LOAD fac_load;
                if (!parse_widevnum_load(fac_ref, &fac_load))
                    continue;

                MOB_FACTION_DATA *new_fac = new_mob_faction_data();
                new_fac->faction_load = fac_load;
                new_fac->next = NULL;

                if (last_fac)
                    last_fac->next = new_fac;
                else
                    mob->factions = new_fac;

                last_fac = new_fac;
            }
        }
```

- [ ] **Step 3: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat(reputation): JSON serialization for mob faction membership"
```

---

### Task 4: DB Resolution and Mob Creation

**Files:**
- Modify: `src/db.c` (fix_area_fields ~line 1938, create_mobile ~line 5263)

- [ ] **Step 1: Resolve faction WNUM_LOAD in fix_area_fields**

In `fix_area_fields()` (db.c), after the `mob_reputations` resolution loop (after line ~1938), add:

```c
                for (MOB_FACTION_DATA *fac = mob->factions; fac != NULL; fac = fac->next)
                {
                    if (fac->faction_load.vnum > 0)
                    {
                        fac->faction = get_reputation_index_auid(fac->faction_load.auid, fac->faction_load.vnum);
                        if (!IS_VALID(fac->faction))
                        {
                            pbugf(LOG_ERROR,
                                  "fix_area_fields: mob %s has invalid faction %ld#%ld",
                                  widevnum_string(mob->area, mob->vnum, NULL),
                                  fac->faction_load.auid,
                                  fac->faction_load.vnum);
                        }
                    }
                    else
                    {
                        fac->faction = NULL;
                    }
                }
```

- [ ] **Step 2: Populate CHAR_DATA.factions in create_mobile**

In `create_mobile()` (db.c), after the mob_reputations copy block (after line ~5263), add:

```c
    // Populate faction membership list from prototype
    for (MOB_FACTION_DATA *fac = pMobIndex->factions; fac != NULL; fac = fac->next)
    {
        if (IS_VALID(fac->faction))
        {
            list_appendlink(mob->factions, fac->faction);
        }
    }
```

- [ ] **Step 3: Copy factions in clone_mobile**

Find `clone_mobile()` in db.c (near line ~5269). After the existing field copies, add:

```c
    // Copy faction membership from parent
    if (parent->factions != NULL)
    {
        ITERATOR it;
        REPUTATION_INDEX_DATA *repIndex;
        iterator_start(&it, parent->factions);
        while ((repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&it)))
        {
            list_appendlink(clone->factions, repIndex);
        }
        iterator_stop(&it);
    }
```

Note: `clone_mobile()` copies a live CHAR_DATA instance, so factions is an LLIST of REPUTATION_INDEX_DATA pointers. Check the actual parameter names (may be `parent`/`clone` or `ch`/`victim` etc).

- [ ] **Step 4: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(reputation): resolve faction WNUM_LOAD and populate on mob creation"
```

---

### Task 5: OLC medit Commands

**Files:**
- Modify: `src/editors/mobiles/medit.c`
- Modify: `src/olc.h` (forward declarations)

- [ ] **Step 1: Add forward declarations to olc.h**

In `olc.h`, find `DECLARE_OLC_FUN( medit_addreputation )` (near line ~421). Add nearby:

```c
DECLARE_OLC_FUN( medit_addfaction );
DECLARE_OLC_FUN( medit_delfaction );
```

- [ ] **Step 2: Add command table entries**

In `medit_table[]` (around line 70), add in alphabetical order:

```c
    {   "addfaction",   medit_addfaction },
```

And:

```c
    {   "delfaction",   medit_delfaction },
```

- [ ] **Step 3: Implement medit_addfaction**

After `medit_delreputation()`, add:

```c
MEDIT(medit_addfaction)
{
    char arg[MIL];
    MOB_INDEX_DATA *pMob;
    WNUM wnum;

    EDIT_MOB(ch, pMob);

    argument = one_argument(argument, arg);
    if (!parse_widevnum(arg, ch->in_room ? ch->in_room->area : NULL, &wnum) || !wnum.pArea || wnum.vnum < 1)
    {
        send_to_char("Syntax:  addfaction <reputation widevnum>\n\r", ch);
        send_to_char("Assigns this mob as a member of the specified faction.\n\r", ch);
        return false;
    }

    REPUTATION_INDEX_DATA *repIndex = get_reputation_index(wnum.pArea, wnum.vnum);
    if (!IS_VALID(repIndex))
    {
        send_to_char("No reputation with that widevnum.\n\r", ch);
        return false;
    }

    // Check for duplicates
    MOB_FACTION_DATA *fac;
    for (fac = pMob->factions; fac; fac = fac->next)
    {
        if (fac->faction == repIndex)
        {
            send_to_char("This mob already belongs to that faction.\n\r", ch);
            return false;
        }
    }

    MOB_FACTION_DATA *new_fac = new_mob_faction_data();
    new_fac->faction = repIndex;
    new_fac->faction_load.auid = repIndex->area ? repIndex->area->uid : 0;
    new_fac->faction_load.vnum = repIndex->vnum;
    new_fac->next = NULL;

    // Append to end of list
    for (fac = pMob->factions; fac && fac->next; fac = fac->next)
        ;

    if (fac)
        fac->next = new_fac;
    else
        pMob->factions = new_fac;

    send_to_char(formatf("Mob is now a member of faction '%s'.\n\r", repIndex->name), ch);
    return true;
}
```

- [ ] **Step 4: Implement medit_delfaction**

```c
MEDIT(medit_delfaction)
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];
    int index;
    MOB_FACTION_DATA *prev, *fac;

    EDIT_MOB(ch, pMob);

    one_argument(argument, arg);
    if (!is_number(arg) || arg[0] == '\0')
    {
        send_to_char("Syntax:  delfaction <index>\n\r", ch);
        return false;
    }

    index = atoi(arg);
    if (index < 0)
    {
        send_to_char("Please specify a non-negative index.\n\r", ch);
        return false;
    }

    for (prev = NULL, fac = pMob->factions; fac && index--; prev = fac, fac = fac->next)
        ;

    if (!fac)
    {
        send_to_char("No such faction entry.\n\r", ch);
        return false;
    }

    if (prev)
        prev->next = fac->next;
    else
        pMob->factions = fac->next;

    free_mob_faction_data(fac);

    send_to_char("Faction membership removed.\n\r", ch);
    return true;
}
```

- [ ] **Step 5: Add faction display to medit_show_special_tab**

Find the `medit_show_special_tab()` function (search for reputation display block ending around line ~672). After the reputation rewards display block, add faction display:

```c
    if (pMob->factions)
    {
        send_to_char("{CFactions:{x\n\r", ch);
        int idx = 0;
        for (MOB_FACTION_DATA *fac = pMob->factions; fac; fac = fac->next, idx++)
        {
            if (IS_VALID(fac->faction))
            {
                send_to_char(formatf("  [%d] %s (%s)\n\r",
                    idx, fac->faction->name,
                    widevnum_string(fac->faction->area, fac->faction->vnum, pMob->area)), ch);
            }
            else
            {
                send_to_char(formatf("  [%d] {R(unresolved %ld#%ld){x\n\r",
                    idx, fac->faction_load.auid, fac->faction_load.vnum), ch);
            }
        }
    }
```

- [ ] **Step 6: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(reputation): medit addfaction/delfaction OLC commands"
```

---

### Task 6: Implement Reputation Stub Functions

**Files:**
- Modify: `src/reputation.c` (lines 516-538)

- [ ] **Step 1: Implement group_gain_reputation**

Replace the stub at lines 516-520 with:

```c
/**
 * group_gain_reputation - Award reputation to group members on mob kill
 *
 * Iterates all players in the killer's group who are in the same room.
 * For each of the victim's mob_reputations entries, checks if the group
 * member's current rank falls within the entry's min/max rank range,
 * then awards the configured reputation points.
 *
 * @param ch      The character who landed the kill (group leader context)
 * @param victim  The NPC that was killed
 */
void group_gain_reputation(CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (!IS_NPC(victim))
        return;

    if (ch->in_room == NULL)
        return;

    if (victim->mob_reputations == NULL)
        return;

    LLIST *seen_reps = list_create(false);

    CHAR_DATA *gch;
    for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
    {
        if (!is_same_group(gch, ch) || IS_NPC(gch))
            continue;

        list_clear(seen_reps);

        MOB_REPUTATION_DATA *mob_rep;
        for (mob_rep = victim->mob_reputations; mob_rep; mob_rep = mob_rep->next)
        {
            if (!IS_VALID(mob_rep->reputation))
                continue;

            if (list_hasdata(seen_reps, mob_rep->reputation))
                continue;

            REPUTATION_DATA *rep = find_reputation_char(gch, mob_rep->reputation);
            int rankNo = IS_VALID(rep) ? rep->current_rank : mob_rep->reputation->initial_rank;

            if (mob_rep->minimum_rank > 0 && rankNo < mob_rep->minimum_rank)
                continue;
            if (mob_rep->maximum_rank > 0 && rankNo > mob_rep->maximum_rank)
                continue;

            gain_reputation(gch, mob_rep->reputation, mob_rep->points, NULL, NULL, true);

            list_appendlink(seen_reps, mob_rep->reputation);
        }
    }

    list_destroy(seen_reps);
}
```

- [ ] **Step 2: Implement check_mob_factions**

Replace the stub at lines 522-526 with:

```c
/**
 * check_mob_factions - Initialize player's standing with NPC factions
 *
 * When a player first engages an NPC in combat, ensures the player has
 * a reputation entry for each of the NPC's factions. Creates initial
 * standing if the player has no prior record with that faction.
 *
 * @param ch      The player character
 * @param victim  The NPC being engaged
 */
void check_mob_factions(CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (IS_NPC(ch) || !IS_NPC(victim))
        return;

    if (victim->factions == NULL || list_size(victim->factions) < 1)
        return;

    ITERATOR it;
    REPUTATION_INDEX_DATA *repIndex;
    iterator_start(&it, victim->factions);
    while ((repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&it)))
    {
        set_reputation_char(ch, repIndex, repIndex->initial_rank, repIndex->initial_reputation, false);
    }
    iterator_stop(&it);
}
```

- [ ] **Step 3: Implement check_mob_factions_peaceful**

Replace the stub at lines 528-533 with:

```c
/**
 * check_mob_factions_peaceful - Check if NPC is peaceful toward player
 *
 * Returns true if the player's current rank with ANY of the NPC's factions
 * has the REPUTATION_RANK_PEACEFUL flag set. Used in is_safe() to prevent
 * attacking friendly NPCs.
 *
 * @param ch      The player character
 * @param victim  The NPC to check against
 * @return        true if the NPC should be protected from attack
 */
bool check_mob_factions_peaceful(CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (IS_NPC(ch) || !IS_NPC(victim))
        return false;

    if (victim->factions == NULL || list_size(victim->factions) < 1)
        return false;

    ITERATOR it;
    REPUTATION_INDEX_DATA *repIndex;
    iterator_start(&it, victim->factions);
    while ((repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&it)))
    {
        if (is_reputation_rank_peaceful(ch, repIndex))
        {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}
```

- [ ] **Step 4: Implement check_mob_factions_hostile**

Replace the stub at lines 535-538 with:

```c
/**
 * check_mob_factions_hostile - Check if NPC is hostile toward player
 *
 * Returns true if the player's current rank with ANY of the NPC's factions
 * has the REPUTATION_RANK_HOSTILE flag set. Can be used for aggro behavior,
 * combat modifiers, or other faction-based hostility mechanics.
 *
 * @param ch      The player character
 * @param victim  The NPC to check against
 * @return        true if the NPC considers the player an enemy
 */
bool check_mob_factions_hostile(CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (IS_NPC(ch) || !IS_NPC(victim))
        return false;

    if (victim->factions == NULL || list_size(victim->factions) < 1)
        return false;

    ITERATOR it;
    REPUTATION_INDEX_DATA *repIndex;
    iterator_start(&it, victim->factions);
    while ((repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&it)))
    {
        if (is_reputation_rank_hostile(ch, repIndex))
        {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}
```

- [ ] **Step 5: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(reputation): implement faction combat functions"
```

---

### Task 7: Wire Into fight.c

**Files:**
- Modify: `src/fight.c`

- [ ] **Step 1: Add group_gain_reputation call after group_gain**

In `fight.c`, find the `group_gain(ch, victim)` call at line ~1965. Immediately after it, add:

```c
        group_gain_reputation(ch, victim);
```

- [ ] **Step 2: Add faction peaceful check to is_safe**

In `is_safe()`, find the NPC victim section (around line 2250+ where other NPC-specific safety checks are). Add before the existing NPC checks (but after the immortal HOLYAURA bypass):

```c
    // Faction peaceful check — don't attack NPCs the player is friendly with
    if (!IS_NPC(ch) && IS_NPC(victim) && check_mob_factions_peaceful(ch, victim))
    {
        if (show)
        {
            act("$N is friendly to you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }
        return true;
    }
```

- [ ] **Step 3: Add faction initialization on combat start**

In `fight.c`, find the `set_fighting()` function. At the start of the function, after initial validation, add:

```c
    check_mob_factions(ch, victim);
```

This ensures factions are initialized when combat begins.

- [ ] **Step 4: Build to verify**

```bash
cd /sentience/src && ./build
```

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(reputation): wire faction checks into combat system"
```

---

### Task 8: Build and Test

**Files:** None (verification only)

- [ ] **Step 1: Build with tests enabled**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile with no warnings related to reputation/faction code.

- [ ] **Step 2: Run existing tests**

```bash
cd /sentience && ./sent -test
```

Expected: All existing tests pass (no regressions). The reputation_system_tests should still pass.

- [ ] **Step 3: Verify no memory issues with ASAN (if available)**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test 2>&1 | head -50
```

Check for any sanitizer warnings related to faction/reputation code.

- [ ] **Step 4: Commit test verification note (if needed)**

If any fixes were needed, commit them.

---

### Task 9: Documentation

**Files:**
- Modify: `src/docs/PLAN_backport_reputation_system.md`
- Modify: `src/docs/ROADMAP.md`

- [ ] **Step 1: Update reputation plan**

In `PLAN_backport_reputation_system.md`, mark Stage D combat integration as complete. Update the combat section to note:
- 4 stub functions implemented
- fight.c wired (group_gain_reputation, is_safe peaceful check, set_fighting faction init)
- MOB_FACTION_DATA added for explicit faction membership
- OLC medit addfaction/delfaction commands added
- JSON persistence for mob factions added
- Memory leak fix for ch->reputations and ch->factions in free_char

- [ ] **Step 2: Update ROADMAP.md**

Note reputation combat integration as complete under the reputation system entry.

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "docs: update reputation plan and roadmap for combat integration"
```

---

## Dependency Graph

```
Task 1 (structs) → Task 2 (memory) → Task 3 (JSON) → Task 4 (DB) → Task 5 (OLC)
                                                                          ↓
Task 6 (stub impls) depends on: Task 1 (structs), Task 2 (memory)
                                                                          ↓
Task 7 (fight.c wiring) depends on: Task 6
                                                                          ↓
Task 8 (build/test) depends on: all above
                                                                          ↓
Task 9 (docs) depends on: Task 8
```

Tasks 1-5 are sequential (each builds on the previous).
Task 6 can start after Task 2 (doesn't need JSON/DB/OLC).
Task 7 depends on Task 6.
Tasks 8-9 are final verification and documentation.
