# Design: Script/Prog Grouping on Entities

## Problem Statement

Currently, script (prog) attachments on entities are stored and displayed as flat tuples of `(script_vnum, trigger_type, phrase)`. The same script can appear multiple times on the same entity with different triggers, each shown as an independent numbered row:

```
Number Vnum        Trigger    Phrase     Status       Name
------ ----------- -------    ------     ------------ -----
[   0] 500         random     25         [COMPILED]   GuardAI
[   1] 500         grall      *          [COMPILED]   GuardAI
[   2] 500         fight      *          [COMPILED]   GuardAI
[   3] 501         death      *          [COMPILED]   DeathLoot
```

This creates several problems:

1. **Unclear grouping** - It's not obvious that entries 0-2 all belong to the same script. Builders must mentally match vnums.

2. **Ambiguous deletion** - `delmprog 1` removes the `grall` trigger but leaves the script attached via the other two triggers. Was the intent to remove the script entirely, or just that one trigger?

3. **No reverse lookup** - There's no way to look at a script and find all entities that use it. This makes it hard to assess the impact of script changes.

4. **Redundant display** - The vnum, status, and name are repeated for every trigger of the same script.

## Proposed Solution

Group triggers under their parent script for display, editing, and serialization:

```
Scripts:
[  1] 500 "GuardAI" [COMPILED]
       random 25
       grall *
       fight *
[  2] 501 "DeathLoot" [COMPILED]
       death *
```

This makes the relationship explicit, keeps long trigger phrases readable, and enables clear semantics for add/remove operations.

## Design Decisions

### Derive, Don't Store

The grouped view is **derived on demand** from the existing slot-based `PROG_LIST` entries. No new persistent data structures are added to entities. The `PROG_LIST` struct, `LLIST **progs` arrays, and all runtime dispatch code in `scripts.c` remain completely untouched.

**Rationale:** The slot-based dispatch system is performance-critical (15 trigger slots, ~200+ field accesses in scripts.c). Adding a parallel persistent data structure would create synchronization complexity. Since grouping is only needed for OLC display, OLC commands, and JSON serialization (all infrequent operations), deriving it on demand is simpler and safer.

### 1-Based Indexing

Script group indexes are 1-based in the new display and commands. This is more intuitive for builders than the current 0-based indexing.

### Duplicate Detection

When `add*prog` adds a trigger to an already-attached script, it checks for exact `(vnum, trigger_type, phrase)` duplicates and rejects them with a warning.

### Trigger Deletion Matches Type AND Phrase

The `deltrigger` command requires both trigger name and phrase because the same trigger type can appear multiple times with different phrases (e.g., two `speech` triggers with different keywords).

### Reverse Lookup via `uses` Command

Rather than a standalone `scriptfind` command requiring a type filter, the reverse lookup is implemented as a `uses` command inside each script editor (mpedit, opedit, rpedit, etc.). The editor context naturally provides the type filter.

---

## Current Architecture

### Data Structures

```c
// merc.h - One per trigger attachment (the atomic unit)
struct prog_list {
    int         trig_type;      // Index into trigger_table[]
    char *      trig_phrase;    // Trigger parameter (e.g., "25" for 25% random)
    int         trig_number;    // atoi(trig_phrase)
    bool        numeric;        // Whether phrase is numeric
    long        vnum;           // Script vnum
    SCRIPT_DATA *script;        // Resolved pointer (set during fix_*progs)
    PROG_LIST * next;
    bool        valid;
};

// merc.h - Wrapper for entities needing runtime script state
struct prog_data {
    PROG_DATA * next;
    LLIST **    progs;          // Array of TRIGSLOT_MAX (15) LLIST*
    CHAR_DATA * target;
    int         delay;
    long        tog_flags;
    int         lastreturn;
    long        entity_flags;   // PROG_NODESTRUCT, PROG_AT, etc.
    int         script_ref;
    bool        extract_when_done;
    bool        extract_fPull;
    pVARIABLE   vars;
};
```

### Entity Types and Prog Storage

| Entity | Storage | Access Pattern |
|--------|---------|----------------|
| MOB_INDEX_DATA | `LLIST **progs` (direct) | `mob->progs[slot]` |
| OBJ_INDEX_DATA | `LLIST **progs` (direct) | `obj->progs[slot]` |
| TOKEN_INDEX_DATA | `LLIST **progs` (direct) | `token->progs[slot]` |
| BLUEPRINT | `LLIST **progs` (direct) | `bp->progs[slot]` |
| DUNGEON_INDEX_DATA | `LLIST **progs` (direct) | `dng->progs[slot]` |
| ROOM_INDEX_DATA | `PROG_DATA *progs` | `room->progs->progs[slot]` |
| AREA_DATA | `PROG_DATA *progs` | `area->progs->progs[slot]` |

### Trigger Slot System

150+ trigger types organized into 15 slots for efficient dispatch:

```
TRIGSLOT_GENERAL (0)     TRIGSLOT_SPEECH (1)      TRIGSLOT_RANDOM (2)
TRIGSLOT_MOVE (3)        TRIGSLOT_ACTION (4)      TRIGSLOT_FIGHT (5)
TRIGSLOT_REPOP (6)       TRIGSLOT_VERB (7)        TRIGSLOT_ATTACKS (8)
TRIGSLOT_HITS (9)        TRIGSLOT_DAMAGE (10)     TRIGSLOT_SPELL (11)
TRIGSLOT_INTERRUPT (12)  TRIGSLOT_COMBATSTYLE (13) TRIGSLOT_ANIMATE (14)
```

### Current OLC Commands

All 7 editors follow the same pattern:
- **Add:** `add*prog <widevnum> <trigger> <phrase>` - creates a PROG_LIST, appends to slot
- **Delete:** `del*prog <index>` - removes the Nth trigger (0-based, flat across all slots)
- **Show:** Flat table with 0-based indexing

### Current JSON Format

```json
{
  "progs": [
    {"vnum": 500, "trigger": "random", "phrase": "25", "numeric": true, "number": 25},
    {"vnum": 500, "trigger": "grall", "phrase": "*", "numeric": false},
    {"vnum": 500, "trigger": "fight", "phrase": "*", "numeric": false},
    {"vnum": 501, "trigger": "death", "phrase": "*", "numeric": false}
  ]
}
```

### Runtime Dispatch

When a trigger fires (e.g., combat), `scripts.c` iterates `progs[TRIGSLOT_FIGHT]` and checks each `PROG_LIST` entry's `trig_type`. This path handles 200+ field accesses and must remain unchanged.

### Post-Load Resolution

After all areas load, `fix_mobprogs()` / `fix_objprogs()` / etc. in `db.c` iterate all entities and resolve `trigger->script = get_script_index_global(trigger->vnum, type)`.

---

## Proposed Changes

### Temporary Grouping Structure (OLC-only, not in merc.h)

```c
// editors/common.h - Transient helper, not persisted
#define MAX_PROG_GROUP_TRIGGERS 64
#define MAX_PROG_GROUPS 32

typedef struct prog_group_entry {
    PROG_LIST *entry;   // The PROG_LIST from the slot
    int slot;           // Which TRIGSLOT it lives in
} PROG_GROUP_ENTRY;

typedef struct prog_group {
    long vnum;
    SCRIPT_DATA *script;
    PROG_GROUP_ENTRY triggers[MAX_PROG_GROUP_TRIGGERS];
    int trigger_count;
} PROG_GROUP;

// Build grouped view from slot arrays. Returns count of unique groups.
int prog_build_groups(LLIST **progs, PROG_GROUP *groups, int max_groups);
```

### New OLC Display

```
Scripts:
[  1] 500 "GuardAI" [COMPILED]
       random 25
       grall *
       fight *
[  2] 501 "DeathLoot" [COMPILED]
       death *
```

### Updated OLC Commands

#### `add*prog <widevnum> <trigger> <phrase>`
- If script not yet attached: creates new PROG_LIST, adds to slot (same as current)
- If script already attached: adds trigger to existing group (new PROG_LIST in appropriate slot)
- If exact (vnum, trigger, phrase) triple exists: warns and rejects

#### `del*prog <index>`
- Index is now 1-based and refers to the script group number
- Removes ALL PROG_LIST entries matching that script's vnum across all slots
- Reports how many triggers were removed

#### `deltrigger <script#> <trigger_name> <phrase>` (new)
- Removes a specific trigger from a script group
- Matches BOTH trigger type AND phrase (handles duplicate trigger types)
- Example: `deltrigger 1 random 25`
- If removing the last trigger for a script, the script becomes fully detached

### New JSON Format

```json
{
  "progs": [
    {"vnum": 500, "triggers": [
      {"type": "random", "phrase": "25"},
      {"type": "grall", "phrase": "*"},
      {"type": "fight", "phrase": "*"}
    ]},
    {"vnum": 501, "triggers": [
      {"type": "death", "phrase": "*"}
    ]}
  ]
}
```

The `numeric` and `number` fields are dropped (derivable from phrase at load time).

**Backward compatibility:** The deserializer detects format by checking the first array element for a `"triggers"` key. Old flat format continues to load indefinitely. Areas migrate automatically when re-saved.

### Reverse Lookup: `uses` Command

Available inside script editors (mpedit, opedit, rpedit, tpedit, apedit, ipedit, dpedit):

```
> mpedit 500
> uses
Script 500 "GuardAI" used by:
  [12345#100] A city guard
       random 25
       grall *
       fight *
  [12345#101] A palace sentry
       random 10
       fight *
2 entities found.
```

The editor context provides the entity type filter (mob script editor searches mobs, etc.).

---

## What Does NOT Change

- `struct prog_list` in merc.h
- `struct prog_data` in merc.h
- `LLIST **progs` on all 7 entity types
- Runtime dispatch in scripts.c (~200+ PROG_LIST field accesses)
- Fix functions in db.c (fix_mobprogs, fix_objprogs, etc.)
- Memory management in mem.c (new_trigger, free_trigger, new_prog_bank, etc.)
- Legacy .are format save in olc_save.c
- No new .c files needed
- No changes to Makefile or CMakeLists.txt

---

## Implementation Phases

### Phase 1: Grouped OLC Display
- Add `prog_build_groups()` and `olc_show_progs_grouped()` to editors/common.c
- Switch all 7 editors to use new display function
- **Risk: Very Low** - Display-only, no data mutation

### Phase 2: Updated OLC Commands
- Add shared helpers: `edit_script_attached()`, `edit_trigger_exists()`, `edit_delscript()`, `edit_deltrigger_specific()` to olc_act.c
- Update `add*prog` in all 7 editors (auto-group + duplicate detection)
- Update `del*prog` in all 7 editors (1-based group deletion)
- Add `deltrigger` command to all 7 editors
- **Risk: Low-Medium** - Changes OLC behavior, not runtime

### Phase 3: JSON Format Change
- Rewrite `json_area_serialize_progs()` to use grouping
- Split `json_area_deserialize_progs()` into format-detecting dispatcher + old/new parsers
- **Risk: Medium** - Changes persistence, but backward-compatible

### Phase 4: Reverse Lookup
- Add `edit_show_script_uses()` to olc_act.c
- Add `uses` command to all 7 script editors
- **Risk: Very Low** - Read-only command

Each phase is independently testable and deployable. Phase 4 has no dependency on Phases 1-3 and can be done at any time.

---

## Key Source Files

| File | Role |
|------|------|
| `merc.h` | PROG_LIST, PROG_DATA, TRIGSLOT_* defines (unchanged) |
| `editors/common.c` | `olc_show_progs()` → `olc_show_progs_grouped()`, `prog_build_groups()` |
| `editors/common.h` | PROG_GROUP types, new function declarations |
| `olc_act.c` | Shared helpers: `edit_delscript()`, `edit_deltrigger_specific()`, `edit_show_script_uses()` |
| `olc.h` | Function declarations for shared helpers |
| `olc.c` | Command tables for medit, oedit, redit, aedit, tedit |
| `blueprint.c` | Command table for bpedit |
| `dungeon.c` | Command table for dngedit |
| `editors/mobiles/medit.c` | Mob editor prog commands |
| `editors/objects/oedit.c` | Object editor prog commands |
| `editors/rooms/redit.c` | Room editor prog commands |
| `editors/tokens/tedit.c` | Token editor prog commands |
| `editors/areas/aedit.c` | Area editor prog commands |
| `editors/blueprints/bpedit.c` | Blueprint editor prog commands |
| `editors/dungeons/dngedit.c` | Dungeon editor prog commands |
| `io/json/json_area.c` | JSON serialize/deserialize for progs |
| `scripts.c` | Runtime trigger dispatch (unchanged) |
| `db.c` | fix_*progs() post-load resolution (unchanged) |
| `mem.c` | Memory management for PROG_LIST (unchanged) |
