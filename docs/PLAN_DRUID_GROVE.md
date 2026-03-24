# Druid Grove System

**Status:** Design — not yet implemented
**Depends on:** `PLAN_CLASS_EXTENDED_DATA.md`, existing instance/blueprint system (`blueprint.c`, `dungeon.c`)

---

## Overview

Each druid character owns a personal **grove** — a small instanced area that persists
indefinitely, can be loaded and unloaded on demand to conserve memory, and expands
over time as the druid levels up and tends it. The grove provides ambient bonuses while
the druid is inside, and those bonuses scale with grove level.

This design builds on two foundations:

1. **Class extended data** (`PLAN_CLASS_EXTENDED_DATA.md`) — `CLASS_LEVEL.custom_data`
   stores the grove's persistent metadata; `DRUID_EXT` holds the live `INSTANCE *` pointer.
2. **Instance system** (`blueprint.c`) — the grove is an `INSTANCE` created from a
   shared blueprint, with two new lifecycle states (persistent + dormant) and support for
   adding blueprint sections incrementally.

---

## New Instance Concepts

The existing instance system creates instances and either keeps them live until all players
leave (whereupon they are extracted and freed), or keeps them permanently with
`INSTANCE_NO_IDLE`. Neither fits a persistent personal grove. Two new concepts are needed:

### `INSTANCE_PERSISTENT` flag

Marks an instance as never-destroyed. When the idle timeout fires and the instance is
orphaned, instead of calling `extract_instance()`, the system calls `sleep_instance()`.

### `INSTANCE_DORMANT` flag

Set by `sleep_instance()`. Indicates that all cloned rooms have been freed from memory
but the `INSTANCE *` struct and its metadata remain in `loaded_instances`. The instance
can be revived at any time by calling `wake_instance()`.

### `sleep_instance(INSTANCE *inst)`

1. Move any remaining players to `inst->environ` (the fallback exit room) or the
   world fallback room.
2. Save full room state to the instance's persist JSON file (see *Persistence* below).
3. Call `extract_clone_room()` on every room in `inst->rooms`.
4. Clear `inst->sections`, `inst->rooms`, `inst->entrance`, `inst->exit`, `inst->recall`.
5. Set `INSTANCE_DORMANT`. Clear `INSTANCE_NO_IDLE` equivalent.
6. The `INSTANCE *` struct remains on `loaded_instances`.

### `wake_instance(INSTANCE *inst)`

1. Assert `INSTANCE_DORMANT` is set.
2. Re-run the selective section cloning path (see *Expandable Sections* below), using
   `inst->active_sections` to determine which sections to recreate.
3. Overlay saved `room_states` onto the freshly cloned rooms (restore custom descriptions,
   placed objects, etc.).
4. Re-run exit wiring for all active sections.
5. Re-run `instance_apply_specialrooms()` and `reset_instance()`.
6. Clear `INSTANCE_DORMANT`.

---

## Expandable Blueprint Sections

The grove blueprint defines all possible expansion areas upfront. Only a subset is
instantiated at any time, controlled by `grove_level`. New sections are added without
recreating the whole instance.

### `BSECREF_DEFERRED` flag on `BLUEPRINT_SECTION_REF`

```c
struct blueprint_section_ref {
    union { WNUM_LOAD load; long vnum; } section_ref;
    BLUEPRINT_SECTION *section;
    char *name;     /* NEW: named reference for script/command access */
    int   flags;    /* NEW: BSECREF_DEFERRED etc. */
};

#define BSECREF_DEFERRED  (1 << 0)  /* Skip at create time; add via add_section_to_instance() */
```

`generate_static_instance()` skips any `BLUEPRINT_SECTION_REF` with `BSECREF_DEFERRED`.

### `bp_section_index` on `INSTANCE_SECTION`

`STATIC_BLUEPRINT_LINK` references sections by their index in `bp->sections`. When
sections are added out of order (deferred ones later), list position no longer matches
blueprint definition order. Fix: store the original blueprint position on each
`INSTANCE_SECTION`, and update `instance_get_section()` to search by that value rather
than list position.

```c
struct instance_section_data {
    /* ... existing fields ... */
    int bp_section_index;   /* NEW: index in bp->sections this occupies */
};
```

`instance_get_section(instance, idx)` becomes a search:
```c
INSTANCE_SECTION *instance_get_section(INSTANCE *inst, int idx) {
    ITERATOR it;
    INSTANCE_SECTION *sec;
    iterator_start(&it, inst->sections);
    while ((sec = iterator_nextdata(&it))) {
        if (sec->bp_section_index == idx) {
            iterator_stop(&it);
            return sec;
        }
    }
    iterator_stop(&it);
    return NULL;
}
```

Because `generate_static_instance()` already skips NULL results in the wiring loop,
deferred sections simply result in unresolved (skipped) links — their placeholder exits
in the core rooms remain `EX_ENVIRONMENT` (sealed/blocked) until the section is added.

### `add_section_to_instance(INSTANCE *inst, const char *section_name)`

```
1. Find the BLUEPRINT_SECTION_REF in bp->sections by name; record its list index.
2. Verify the section is marked BSECREF_DEFERRED and not already in inst->sections.
3. Call clone_blueprint_section() to produce a new INSTANCE_SECTION.
4. Set new_section->bp_section_index = that list index.
5. Set new_section->blueprint = inst->blueprint; new_section->instance = inst.
6. Append to inst->sections and inst->rooms.
7. Re-run the STATIC_BLUEPRINT_LINK wiring loop (only links where one endpoint is
   the new section's bp_section_index — the other side is already resolved).
8. Re-run instance_apply_specialrooms() for the new section.
9. Call reset_instance() (or section-scoped reset) to populate mobs/objects.
10. Append the new bp_section_index to inst->active_sections.
11. Save the instance JSON.
```

The wiring step converts the sealed `EX_ENVIRONMENT` exit in the existing core room into
a live bidirectional exit connecting to the new section's entrance room. From the druid's
perspective, a previously overgrown or sealed archway opens.

---

## Grove Blueprint Layout

One shared blueprint defines the full expanded grove. All druids clone from the same
template; individual customisation is stored in `room_states` per instance.

```
Area: druid_grove_template  (AREA_BLUEPRINT)
  Rooms 9001–9010: core grove clearing (5-6 rooms)
  Rooms 9011–9016: eastern glade expansion
  Rooms 9017–9022: ancient pool expansion
  Rooms 9023–9030: spirit hollow expansion

Blueprint: grove
  Sections:
    [0] grove_core          — NOT deferred; always present
    [1] grove_east_glade    — BSECREF_DEFERRED; added at grove_level 2
    [2] grove_ancient_pool  — BSECREF_DEFERRED; added at grove_level 4
    [3] grove_spirit_hollow — BSECREF_DEFERRED; added at grove_level 6

  Static links:
    [0,link:east]  ↔  [1,link:west]    (core ↔ eastern glade)
    [0,link:north] ↔  [2,link:south]   (core ↔ ancient pool)
    [1,link:north] ↔  [3,link:east]    (eastern glade ↔ spirit hollow)
```

Blueprint entrance: `[0, link:entry]` — the portal exit that connects to the outside world.

---

## Character Data

### `CLASS_LEVEL.custom_data` (persistent)

```json
{
    "grove_instance_uid": [12345, 0],
    "grove_level": 3,
    "last_tended": 1740000000,
    "bonuses": {
        "regen_pct": 20,
        "mana_regen_pct": 15
    }
}
```

### `DRUID_EXT` (runtime, in `CLASS_LEVEL.ext`)

```c
typedef struct {
    INSTANCE *  grove;          /* NULL if not yet created or not yet woken */
    int         grove_level;    /* Cached from custom_data for fast bonus lookup */
    time_t      last_tended;
} DRUID_EXT;
```

### `ext_init` (called for all joined classes at character load; also on class join)

`ext_init` only resolves references — it does **not** wake the instance. The grove
stays dormant until the player explicitly enters it via `do_grove`. This matters because
`ext_init` runs at character load for every joined class, and waking an instance is
potentially expensive and undesirable for characters loading in an unrelated zone.

```
1. Read grove_instance_uid from custom_data.
2. Find INSTANCE * in loaded_instances by UID.
3. If not found and uid is valid: log warning (instance missing after reboot?).
4. Cache grove pointer and grove_level in DRUID_EXT.
   Leave INSTANCE_DORMANT state untouched — do NOT call wake_instance() here.
```

This also means cross-class abilities that reference `DRUID_EXT.grove` always get a valid
pointer (or NULL if the grove hasn't been created yet), regardless of whether the druid
is the active class. The instance wakes lazily from `do_grove`, not from ext_init.

### `ext_sync` (called before save for all joined classes)

```
1. Update custom_data["grove_level"] and custom_data["last_tended"] from DRUID_EXT.
2. If grove is live (not INSTANCE_DORMANT): save grove instance JSON.
   (Dormant instances were already saved when they were put to sleep.)
```

Note: `ext_sync` is called before every character save across all joined classes.
It is NOT called on class switch — the `leave` callback handles any game-effect
side effects (messages, stat changes), not ext state management.

### `ext_free` (called only in `free_class_level()` — character unload or class removal)

```
1. If grove is live and no other players inside: call sleep_instance().
2. free(cl->ext); cl->ext = NULL.
```

---

## Grove Lifecycle

### First creation

Triggered by a class reward (`REWARD_SCRIPT`) at a specific ranger level or by completing
a druid quest:

```
1. create_instance(grove_blueprint)    — clones only non-deferred sections (core)
2. SET_BIT(inst->flags, INSTANCE_PERSISTENT)
3. Register druid as player_owner via instance_addowner_player()
4. Store inst->id[0/1] in cl->custom_data["grove_instance_uid"]
5. Cache in DRUID_EXT.grove
6. Save instance JSON immediately
```

### Entering the grove (`do_grove`)

```
1. Get DRUID_EXT from current CLASS_LEVEL.
2. If grove == NULL: "Your grove has not yet awakened."
3. If INSTANCE_DORMANT: wake_instance(grove).
4. Transfer player to grove->entrance.
```

### Tending (`do_tend`)

Increases `grove_level` over time (cooldown enforced via `last_tended`). At new grove
levels, calls `add_section_to_instance()` for the next deferred section.

```
grove_level 1: core only
grove_level 2: add grove_east_glade  → eastern archway opens
grove_level 4: add grove_ancient_pool → northern path clears
grove_level 6: add grove_spirit_hollow → spirit hollow accessible from glade
```

Also updates ambient bonus values in `custom_data["bonuses"]`.

### Idle / leaving

`instance_update()` for a persistent, orphaned instance calls `sleep_instance()` instead
of `extract_instance()`. The `INSTANCE *` remains on `loaded_instances`.

On reboot, persistent instance JSONs are loaded as dormant structs — no rooms allocated
until the druid next enters.

---

## Ambient Grove Bonuses

Applied by a tick handler or `enter_room` hook when the druid is inside their own grove:

```c
bool is_in_own_grove(CHAR_DATA *ch) {
    if (IS_NPC(ch) || !ch->pcdata->current_class) return false;
    DRUID_EXT *ext = (DRUID_EXT *)ch->pcdata->current_class->ext;
    if (!ext || !ext->grove) return false;
    return room_is_in_instance(ch->in_room, ext->grove);
}
```

Bonus scaling (example — tune via `custom_data["bonuses"]`):

| Grove Level | Regen Bonus | Mana Regen Bonus | Notes |
|---|---|---|---|
| 1 | +10% | +5% | Core clearing only |
| 2 | +15% | +10% | Eastern glade accessible |
| 4 | +20% | +15% | Ancient pool, healing water |
| 6 | +25% | +20% | Spirit hollow, full grove |

---

## Persistence Schema

`persist/instances/instance_<uid0>_<uid1>.json`:

```json
{
    "uid": [12345, 0],
    "flags": ["persistent", "dormant"],
    "blueprint_wnum": { "auid": 5, "vnum": 100 },
    "player_owners": ["Sylvara"],
    "active_sections": [0, 1],
    "room_states": [
        {
            "source_vnum": 9001,
            "extra_descr": [],
            "resets": []
        },
        {
            "source_vnum": 9011,
            "extra_descr": [
                { "keyword": "flowers", "description": "Bright flowers have bloomed here." }
            ],
            "resets": []
        }
    ]
}
```

`active_sections` is the list of `bp_section_index` values that have been added so far.
On `wake_instance()`, only these indices are cloned; the rest remain deferred.

---

## File Changes Summary

### Instance system (generic — needed for groves but reusable)

| File | Change |
|---|---|
| `merc.h` | `INSTANCE_PERSISTENT`, `INSTANCE_DORMANT` flags; `bp_section_index` on `INSTANCE_SECTION`; `name` + `flags` on `BLUEPRINT_SECTION_REF`; `BSECREF_DEFERRED` constant |
| `blueprint.c` | `sleep_instance()`, `wake_instance()`, `add_section_to_instance()`; update `instance_get_section()` to search by `bp_section_index`; skip deferred sections in `generate_static_instance()`; set `bp_section_index` when cloning |
| `blueprint.c` | `instance_update()`: call `sleep_instance()` instead of `extract_instance()` for `INSTANCE_PERSISTENT` orphaned instances |
| `io/json/json_instance.c` | Save/load `active_sections` array and `room_states` per room; save/load `INSTANCE_PERSISTENT`/`INSTANCE_DORMANT` flags |

### Grove-specific

| File | Change |
|---|---|
| `act_druid.c` (new) | `DRUID_EXT` struct; `druid_ext_init/sync/free`; `do_grove`, `do_tend`; ambient bonus application |
| `class_data.c` | Register `druid_ext_init`, `druid_ext_sync`, `druid_ext_free` in callback resolution table |
| `data/world/blueprints/` | Grove blueprint definition with deferred sections and static links |
| `area/druid_grove_template.json` (new) | Template rooms for all four grove sections (`AREA_BLUEPRINT`) |

---

## Open Questions

1. **Multiple druids, same grove template:** Each druid gets their own `INSTANCE *` but
   all clone from the same blueprint template. Custom room descriptions added by tending
   live in `room_states` per instance — no cross-contamination.

2. **Grove while not active class:** Should grove bonuses and `do_tend` be accessible
   when the druid is not currently in the druid class? Current plan: bonuses require
   active class; grove entry is always allowed (it's their space). `do_tend` requires
   active class.

3. **Grove on character deletion:** `free_class_level()` calls `ext_free`, which calls
   `sleep_instance()`. A separate cleanup pass should call `extract_instance()` on
   persistent instances owned only by deleted characters.

4. **Section rollback:** If a druid somehow loses grove levels (admin action, bug), should
   expanded sections be removed? Current plan: sections are never removed once added —
   the grove only grows.

5. **Grove sharing:** Should a druid be able to invite others to their grove? The instance
   already supports `player_owners` and `players` lists. A `do_grove invite <name>` command
   could add a temporary player entry without making them an owner. Deferred for later.
