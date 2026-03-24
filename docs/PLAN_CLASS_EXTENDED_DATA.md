# Class Extended Data System

**Status:** Design — not yet implemented
**Relates to:** `PLAN_backport_skills_classes.md` (Phase 6), `PLAN_DRUID_GROVE.md`

---

## Problem Statement

Some classes need rich, class-specific state that goes beyond scalar level and XP tracking.
Examples:

- **Ranger** — a stable of named, persistent companion pets with individual levels and loyalty
- **Druid** — a personal grove instance tied to world locations, expandable over time
- **Blacksmith** — per-recipe-category mastery counters, unlocked techniques
- **Explorer** — discovered location registry, mapping progress

`CLASS_LEVEL` already has a `json_t *custom_data` field for free-form persistence (added in
Phase 6b), and that field is fully wired for save/load. However, complex runtime state —
live `CHAR_DATA *` pointers to active pet mobs, resolved `ROOM_INDEX_DATA *` pointers to
grove rooms, etc. — cannot live in JSON. JSON is persistence-only; a second, C-typed layer
is needed for runtime.

---

## Two-Layer Model

```
CLASS_LEVEL.custom_data   (json_t *)  —  persistence only
CLASS_LEVEL.ext           (void *)    —  runtime state only
```

These two layers are strictly separated by purpose:

| Layer | Type | Lifetime | Purpose |
|---|---|---|---|
| `custom_data` | `json_t *` | Save/load cycle | Serialisable state: vnums, names, integers, timestamps |
| `ext` | `void *` | Session only | Live pointers, resolved references, runtime caches |

Classes that only need scalar persistent data (mastery counters, timestamps) use
`custom_data` directly and never need `ext`. Classes that need live C objects (pet mobs,
grove rooms) use both: `ext` is populated from `custom_data` on class enter, and flushed
back on class leave or save.

---

## Data Model Changes

### `CLASS_LEVEL` (merc.h)

Add one pointer:

```c
struct class_level {
    CLASS_LEVEL *       next;
    bool                valid;

    CLASS_DATA *        clazz;
    int                 level;
    long                xp;
    char *              active_title;

    struct json_t *     custom_data;    /* Persistent: free-form JSON — already exists */
    void *              ext;            /* NEW: Runtime: class-defined C struct, NULL if unused */
};
```

### `CLASS_DATA` (merc.h / class_data.h)

Add three optional lifecycle callbacks alongside the existing `enter`/`leave` pair:

```c
typedef void CLASS_EXT_INIT_FUN (CLASS_LEVEL *cl, CHAR_DATA *ch);
typedef void CLASS_EXT_SYNC_FUN (CLASS_LEVEL *cl, CHAR_DATA *ch);
typedef void CLASS_EXT_FREE_FUN (CLASS_LEVEL *cl);

struct class_data {
    /* ... existing fields ... */

    CLASS_ENTER_FUN *   enter;              /* Called on class switch TO (already exists) */
    CLASS_LEAVE_FUN *   leave;              /* Called on class switch FROM (already exists) */

    CLASS_EXT_INIT_FUN *ext_init;           /* NEW: alloc ext + populate from custom_data */
    CLASS_EXT_SYNC_FUN *ext_sync;           /* NEW: flush ext state back into custom_data */
    CLASS_EXT_FREE_FUN *ext_free;           /* NEW: free ext (custom_data is handled separately) */

    char *              ext_init_fun_name;  /* Serialised function name */
    char *              ext_sync_fun_name;
    char *              ext_free_fun_name;
};
```

All three are optional (`NULL` = no-op). A class that only uses `custom_data` scalars
leaves them NULL.

---

## Lifecycle

`ext` lifetime is **independent of which class is currently active**. This is the key
invariant that makes cross-class skills and traits work correctly: `ext` is initialised
for all joined classes at character load and freed only when the class is removed from
the character entirely. Switching active class does not free or reinitialise `ext`.

The `enter`/`leave` callbacks on `CLASS_DATA` are for game-effect side effects only
(messages, stat adjustments, world interactions). They must not manage `ext`.

### On character load (json_char.c)

After all `CLASS_LEVEL` entries are restored from JSON, `ext_init` is called for
**every joined class**, not just the active one:

```c
ITERATOR it;
CLASS_LEVEL *cl;
iterator_start(&it, ch->pcdata->classes);
while ((cl = iterator_nextdata(&it))) {
    if (cl->clazz && cl->clazz->ext_init)
        cl->clazz->ext_init(cl, ch);
}
iterator_stop(&it);
```

### On class join (new class added to character)

When a class is first added to a character (quest grant, level unlock, etc.):

```c
if (cl->clazz->ext_init)
    cl->clazz->ext_init(cl, ch);
```

### On class switch TO/FROM (act_class.c)

Class switching **does not touch `ext`**. Only the game-effect callbacks fire:

```c
/* On switch FROM: */
if (prev_cl->clazz->leave)
    prev_cl->clazz->leave(ch);

/* On switch TO: */
if (new_cl->clazz->enter)
    new_cl->clazz->enter(ch);
```

`ext_sync` is NOT called on switch — only on save. `ext_free` is NOT called on switch.

### On character save

Sync all joined classes, not just the active one:

```c
ITERATOR it;
CLASS_LEVEL *cl;
iterator_start(&it, ch->pcdata->classes);
while ((cl = iterator_nextdata(&it))) {
    if (cl->clazz && cl->clazz->ext_sync && cl->ext)
        cl->clazz->ext_sync(cl, ch);
}
iterator_stop(&it);
/* Then serialise custom_data as normal — already handled by json_char.c */
```

### On `free_class_level()` (class_data.c) — the only place `ext_free` is called

`ext_free` fires when the `CLASS_LEVEL` is actually being destroyed: on character
unload, or when a class is explicitly removed from the character.

```c
if (cl->clazz && cl->clazz->ext_sync && cl->ext)
    cl->clazz->ext_sync(cl, ch);   /* final flush before freeing */
if (cl->clazz && cl->clazz->ext_free && cl->ext)
    cl->clazz->ext_free(cl);
cl->ext = NULL;
/* custom_data json_decref() — already exists */
```

---

## Cross-Class Skill and Trait Access

The existing reward scope system (`REWARD_SCOPE_*` in `class_data.h`) already handles
whether a skill or trait is available outside its granting class. `ext` complements this
without adding new rules — because `ext` is always live for every joined class, any code
path that can run under a cross-class scope can simply walk `pcdata->classes` to reach
the relevant `CLASS_LEVEL` and its `ext`.

### Access pattern for cross-class ability code

```c
/* A SCOPE_ALWAYS ranger ability running while Warrior is active: */
CLASS_DATA  *ranger_class = class_find("ranger");
CLASS_LEVEL *ranger_cl    = get_class_level(ch, ranger_class);

if (!ranger_cl || !ranger_cl->ext)
    return;   /* hasn't joined ranger, or ext_init not defined */

RANGER_EXT *rex = (RANGER_EXT *)ranger_cl->ext;
/* use rex->pet_chars[...] etc. */
```

`get_class_level(ch, clazz)` already exists and is the correct accessor. No new
infrastructure is needed.

### What the scope system controls vs. what `ext` controls

| Concern | Handled by |
|---|---|
| Is this skill usable right now? | `REWARD_SCOPE_*` on `SKILL_ENTRY` + `get_skill()` |
| Is this trait active right now? | `ch_get_trait_*()` layered query |
| Where is the runtime state for this class? | `CLASS_LEVEL.ext` (always live) |
| Where is the persistent state for this class? | `CLASS_LEVEL.custom_data` (always live) |

`ext` has no opinion on scope — it is simply always populated for joined classes.
Scope-gating happens at the skill/trait layer before any `ext` access is attempted.

### Grove example under cross-class

A druid might have a SCOPE_ALWAYS "commune with grove" spell that works regardless of
active class. That spell accesses `DRUID_EXT.grove` (the `INSTANCE *`) to check grove
level or apply a grove-sourced effect. Because `ext` is never freed on class switch, the
`INSTANCE *` pointer is valid throughout the session. The spell does not need to know
whether druid is the active class.

```c
CLASS_LEVEL *druid_cl = get_class_level(ch, class_find("druid"));
DRUID_EXT   *dex      = druid_cl ? (DRUID_EXT *)druid_cl->ext : NULL;

if (!dex || !dex->grove)  { /* no grove yet */ return; }
/* use dex->grove_level, dex->grove, etc. */
```

---

## Examples

### Ranger Pet Stable

```c
#define MAX_RANGER_PETS 5

typedef struct {
    int          count;
    WNUM         pet_wnums[MAX_RANGER_PETS];    /* vnum for respawning */
    char *       pet_names[MAX_RANGER_PETS];    /* NULL = unnamed */
    int          pet_levels[MAX_RANGER_PETS];
    int          pet_loyalty[MAX_RANGER_PETS];  /* 0-100 */
    bool         pet_bonded[MAX_RANGER_PETS];
    CHAR_DATA *  pet_chars[MAX_RANGER_PETS];    /* NULL if not currently in world */
} RANGER_EXT;
```

`custom_data` schema:
```json
{
    "pets": [
        { "auid": 3, "vnum": 42, "name": "Fenris", "level": 12, "loyalty": 85, "bonded": true },
        { "auid": 3, "vnum": 51, "name": null,     "level": 6,  "loyalty": 40, "bonded": false }
    ],
    "stable_capacity": 3
}
```

- `ext_init`: parse `custom_data["pets"]`, fill `RANGER_EXT`, attempt to find/create
  live mob instances in world
- `ext_sync`: walk `RANGER_EXT.pet_chars`, update health/loyalty, write back to
  `custom_data["pets"]`
- `ext_free`: `free(cl->ext)` — pet mobs persist in world independently

### Druid Grove

```c
typedef struct {
    INSTANCE *   grove;             /* The persistent grove instance */
    int          grove_level;       /* Cached from custom_data */
    time_t       last_tended;
} DRUID_EXT;
```

`custom_data` schema:
```json
{
    "grove_instance_uid": [12345, 0],
    "grove_level": 3,
    "last_tended": 1740000000
}
```

- `ext_init`: read instance UID, look up `INSTANCE *` in `loaded_instances`, wake if
  dormant, cache pointer in `DRUID_EXT`
- `ext_sync`: update `last_tended`, `grove_level` in `custom_data`; save instance JSON
- `ext_free`: call `sleep_instance(grove)` if no other players inside; `free(cl->ext)`

See `PLAN_DRUID_GROVE.md` for the full grove system design.

---

## File Changes Summary

| File | Change |
|---|---|
| `merc.h` | Add `void *ext` to `CLASS_LEVEL`; add three callback typedefs and fields to `CLASS_DATA` |
| `class_data.h` | Declare `CLASS_EXT_INIT_FUN`, `CLASS_EXT_SYNC_FUN`, `CLASS_EXT_FREE_FUN` typedefs |
| `class_data.c` | Callback name→pointer resolution table; `free_class_level()` calls `ext_free`; copy/move logic handles `ext = NULL` |
| `act_class.c` | Hook `ext_init` at class-enter, `ext_sync`+`ext_free` at class-leave |
| `io/json/json_char.c` | Call `ext_init` after class levels loaded; call `ext_sync` before serialising |
| `class_data.c` (JSON) | Save/load `ext_init_fun_name`, `ext_sync_fun_name`, `ext_free_fun_name` |
| `act_ranger.c` (new) | `RANGER_EXT` definition, ranger ext callbacks, pet management commands |
| `act_druid.c` (new) | `DRUID_EXT` definition, druid ext callbacks, grove commands |

---

## Design Notes

- `ext` is **session-only**. It is never serialised. Only `custom_data` crosses the
  save/load boundary. This keeps the serialisation surface clean and simple.
- `ext_sync` must be idempotent — it may be called multiple times before a save.
- `free_class_level()` must call `ext_free` before freeing the struct. The `clazz`
  pointer must be valid at this point (checked before calling).
- Classes that don't need runtime state leave all three callbacks NULL. The overhead is
  a single NULL check at each lifecycle point.
- The function-name resolution table in `class_data.c` follows the same pattern as
  `enter`/`leave` and the existing spell function table.
