# Character Data GMCP Packages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement 9 new/expanded GMCP packages providing rich character data for the Sentience web client.

**Architecture:** Each package follows the existing builder pattern — a pure function taking an input struct and returning `json_t*`. Pulse-based dirty-flag detection (with event-triggered invalidation for class swap/reputation changes) drives automatic pushes. All new code goes in existing files (`gmcp_sentience.c/h`, `channels/channel_gmcp.c`).

**Tech Stack:** C, Jansson (JSON), existing GMCP infrastructure in `gmcp_sentience.c`.

**Spec:** `docs/superpowers/specs/2026-03-25-preferences-inventory-equipment-design.md`

**Test baseline:** 34/34 GMCP tests pass. Build: `cd /sentience/src && ./build tests`. Run: `cd /sentience && ./sent -test:gmcp`.

---

## File Map

**Modify:**
- `gmcp_sentience.h` (~295 lines) — input structs, cache fields, dirty flags, function declarations
- `gmcp_sentience.c` (~1271 lines) — builders, senders, update loop additions
- `channels/channel_gmcp.c` (~187 lines) — thread `report_id` through broadcast/directed
- `channels/channel_service.c` — pass `report_id` at GMCP call sites
- `protocol.h` (~858 lines) — GMCP support enum additions
- `protocol.c` (~4275 lines) — GMCPReceiveTable for preferences expansion
- `gmcp_sentience.c:972+` — identity population expansion (class/race extended data)
- `reputation.c` — cache invalidation hooks in `gain_reputation()`, `set_reputation_rank()`, `group_gain_reputation()`
- `act_class.c` or wherever `do_setclass` lives — abilities cache invalidation
- `tests/unit/gmcp_sentience_tests.c` (~911 lines) — new scenario runners
- `tests/data/unit/gmcp_sentience_unit_tests.json` (~1217 lines) — new test data
- `tests/data/test_config.json` — if new suites needed
- `docs/GMCP_WEB_CLIENT_REFERENCE.md` — full documentation for all new packages

**No new source files.** All builders go in `gmcp_sentience.c`.

---

### Task 1: Cache & Dirty Flag Infrastructure

Add all new cache fields and dirty flags needed by Tasks 2–9. This is the foundation everything else builds on.

**Files:**
- Modify: `gmcp_sentience.h:44-93` (cache struct), `gmcp_sentience.h` (dirty flags enum)

- [ ] **Step 1: Add new cache fields to `sentience_gmcp_cache_t`**

In `gmcp_sentience.h`, after the existing `was_fighting` field (~line 90), add:

```c
    /* Phase 4: Inventory/Equipment fingerprint */
    int inventory_count;
    int equipment_count;

    /* Phase 4: Abilities fingerprint (set to -1 to force rebuild) */
    int abilities_count;

    /* Phase 4: Reputations fingerprint (set to -1 to force rebuild) */
    int reputation_count;

    /* Phase 4: Church change detection */
    bool has_church;
    long church_uid;

    /* Phase 4: Race change detection */
    int16_t race_uid;
```

- [ ] **Step 2: Add new dirty flag values**

In `gmcp_sentience.h`, extend the dirty flags enum after `SENTIENCE_DIRTY_ENEMIES`:

```c
    SENTIENCE_DIRTY_INVENTORY   = (1 << 8),
    SENTIENCE_DIRTY_EQUIPMENT   = (1 << 9),
    SENTIENCE_DIRTY_ABILITIES   = (1 << 10),
    SENTIENCE_DIRTY_REPUTATIONS = (1 << 11),
    SENTIENCE_DIRTY_CHURCH      = (1 << 12),
    SENTIENCE_DIRTY_RACE        = (1 << 13),
```

Also update the "all dirty" initializer mask — currently `dirty = 0xFF` on first send in `sentience_gmcp_update()`. Change to `dirty = 0x3FFF` (14 bits) or use a named constant:

```c
#define SENTIENCE_DIRTY_ALL  0x3FFF
```

- [ ] **Step 3: Build and verify no regressions**

Run: `cd /sentience/src && ./build tests`
Run: `cd /sentience && ./sent -test:gmcp`
Expected: 34/34 pass (struct changes only, no behavioral change)

- [ ] **Step 4: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c
git commit -m "feat(gmcp): add cache fields and dirty flags for Phase 4 packages

Add inventory_count, equipment_count, abilities_count, reputation_count,
has_church, church_uid, race_uid to sentience_gmcp_cache_t. Add dirty
flag bits for inventory, equipment, abilities, reputations, church, race.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Expanded Preferences Builder & Handler

Expand the existing preferences system to send full preference state with source metadata and handle client-side preference updates.

**Files:**
- Modify: `gmcp_sentience.h` — new input struct for preferences
- Modify: `gmcp_sentience.c` — new builder `sentience_build_preferences_json()`
- Modify: `gmcp_sentience.c` — update `sentience_send_client_preferences()`
- Modify: `protocol.c:~3980` — expand receive handler for set/reset actions
- Reference: `account/preferences.h`, `account/preferences.c`, `const.c` (`pc_set_table[]`)

**Key APIs:**
- `pc_set_table[]` in `const.c` — master list of 43+ preference keys
- `game_settings.pref_defaults` — PREF_ENTRY linked list for runtime overrides
- `pref_get_source(ch, key)` — returns where preference value comes from
- `pref_get_bool/int/string/bitfield()` — read values
- `pref_set_bool/int/string()` — write values
- `pref_find(ch, key)` — find PREF_ENTRY by key
- Categories: toggle/channel/prompt/display/gmcp. Types: bool/int/string/bitfield.

- [ ] **Step 1: Write failing test — preferences builder**

Add test scenario to `tests/data/unit/gmcp_sentience_unit_tests.json`:

```json
{
  "test_type": "gmcp_prefs_builder",
  "description": "Preferences builder produces correct JSON structure",
  "input": {
    "preferences": [
      {"key": "brief", "category": "toggle", "type": "bool", "value_bool": true, "source": "character"},
      {"key": "compact", "category": "toggle", "type": "bool", "value_bool": false, "source": "default"},
      {"key": "prompt_hp", "category": "prompt", "type": "bool", "value_bool": true, "source": "account"}
    ]
  },
  "expected": {
    "_v": 1,
    "preferences_count": 3,
    "has_key_brief": true,
    "brief_source": "character",
    "brief_value": true,
    "compact_source": "default"
  }
}
```

Add scenario runner `run_gmcp_prefs_builder_test()` in `tests/unit/gmcp_sentience_tests.c`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /sentience && ./sent -test:gmcp_prefs`
Expected: FAIL (builder function doesn't exist yet)

- [ ] **Step 3: Implement preferences input struct and builder**

In `gmcp_sentience.h`, add:

```c
#define SENTIENCE_MAX_PREFERENCES 64

typedef struct {
    const char *key;
    const char *category;   /* "toggle", "channel", "prompt", "display", "gmcp" */
    const char *type;       /* "bool", "int", "string", "bitfield" */
    const char *source;     /* "default", "account", "character" */
    const char *label;      /* human-readable display label */
    bool value_bool;
    int value_int;
    const char *value_string;
} sentience_pref_entry_t;

typedef struct {
    int num_prefs;
    sentience_pref_entry_t prefs[SENTIENCE_MAX_PREFERENCES];
} sentience_preferences_input_t;

json_t *sentience_build_preferences_json(const sentience_preferences_input_t *input);
```

In `gmcp_sentience.c`, implement the builder:

```c
json_t *sentience_build_preferences_json(const sentience_preferences_input_t *input)
{
    json_t *obj, *prefs;
    int i;

    if (!input) return NULL;

    obj = json_object();
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    prefs = json_array();
    for (i = 0; i < input->num_prefs; i++) {
        const sentience_pref_entry_t *p = &input->prefs[i];
        json_t *entry = json_object();

        json_object_set_new(entry, "key",      json_string(p->key ? p->key : ""));
        json_object_set_new(entry, "category", json_string(p->category ? p->category : ""));
        json_object_set_new(entry, "type",     json_string(p->type ? p->type : "bool"));
        json_object_set_new(entry, "source",   json_string(p->source ? p->source : "default"));
        json_object_set_new(entry, "label",    json_string(p->label ? p->label : ""));

        if (p->type && !strcmp(p->type, "int"))
            json_object_set_new(entry, "value", json_integer(p->value_int));
        else if (p->type && !strcmp(p->type, "string"))
            json_object_set_new(entry, "value", json_string(p->value_string ? p->value_string : ""));
        else
            json_object_set_new(entry, "value", p->value_bool ? json_true() : json_false());

        json_array_append_new(prefs, entry);
    }
    json_object_set_new(obj, "preferences", prefs);

    return obj;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /sentience && ./sent -test:gmcp_prefs`
Expected: PASS

- [ ] **Step 5: Update `sentience_send_client_preferences()` to use new builder**

Replace the existing preferences sender in `gmcp_sentience.c` to populate the input struct from `pc_set_table[]` and `game_settings.pref_defaults`, using `pref_get_source()` for each key.

- [ ] **Step 6: Expand the receive handler for client preference updates**

In `protocol.c`, expand the `GMCP_SENTIENCE_CLIENT_PREFERENCES` handler (~line 3980) to handle the new action format:

```json
{"action": "set", "key": "brief", "scope": "character", "value": true}
{"action": "reset", "key": "brief", "scope": "character"}
```

Parse `action`, `key`, `scope`, and `value`. Call `pref_set_*()` or `pref_reset()` as appropriate. After mutation, resend full preferences.

- [ ] **Step 7: Build and test**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:gmcp`
Expected: All GMCP tests pass (34 existing + new preferences tests)

- [ ] **Step 8: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c protocol.c tests/
git commit -m "feat(gmcp): expanded preferences builder with source metadata

Bidirectional preferences: full state push with source (default/account/
character) and client-side set/reset via GMCP receive handler.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Inventory Builder

Build `sentience_build_inventory_json()` with embedded actions and keyword disambiguation.

**Files:**
- Modify: `gmcp_sentience.h` — inventory input struct
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/unit/gmcp_sentience_tests.c` — test runner
- Modify: `tests/data/unit/gmcp_sentience_unit_tests.json` — test data
- Reference: `act_info.c` (`format_obj_to_char`, `show_llist_to_char`), `act_obj.c` (action preconditions), `merc.h` (OBJ_DATA)

**Key APIs:**
- `can_see_obj(ch, obj)` — visibility check
- `obj->item_type` → `item_type_info[obj->item_type].name` — type name
- `obj->id[0]`, `obj->id[1]` — unique instance ID
- `obj->short_descr` — display name
- `obj->wear_flags` — what slots it can go to
- `obj->condition` → `object_damage_table[URANGE(0, 9 - (int)(((float)obj->condition)/10), 9)].name`
- `obj->weight`, `obj->level`, `obj->lcontains` — container contents list
- Keyword disambiguation: first keyword of `obj->name`, prefixed with `N.` for duplicates
- LLIST iteration: `ITERATOR it; iterator_start(&it, ch->lcarrying); while ((obj = iterator_nextdata(&it))) {...} iterator_stop(&it);`

- [ ] **Step 1: Write failing test — inventory builder**

Add test scenario for `gmcp_inv_builder` in test JSON. Test:
- Basic item with actions (wearable → wear/drop/examine)
- Keyword disambiguation (two items with same first keyword get `2.keyword`)
- Container item (shows item_count of contents)
- Empty inventory

Add scenario runner in test C file.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /sentience && ./sent -test:gmcp_inv`
Expected: FAIL

- [ ] **Step 3: Implement inventory input struct**

In `gmcp_sentience.h`:

```c
#define SENTIENCE_MAX_INVENTORY    128
#define SENTIENCE_MAX_ITEM_ACTIONS 8
#define SENTIENCE_MAX_ITEM_FLAGS   8

typedef struct {
    const char *label;
    const char *cmd;
} sentience_item_action_t;

typedef struct {
    const char *name;           /* short_descr */
    const char *keywords;       /* full obj->name string for display */
    const char *keyword;        /* first keyword with N. prefix if needed (for action cmds) */
    unsigned long id[2];        /* instance ID — emitted as JSON array [id0, id1] */
    const char *item_type;      /* item_type_info name */
    int condition;              /* integer 0-100 (obj->condition) */
    const char *condition_label;/* damage table name string */
    int level;
    int weight;
    int item_count;             /* container: number of visible items inside, else 0 */
    int num_flags;
    const char *flags[SENTIENCE_MAX_ITEM_FLAGS]; /* "glow", "magic", etc. */
    int num_actions;
    sentience_item_action_t actions[SENTIENCE_MAX_ITEM_ACTIONS];
} sentience_inventory_item_t;

typedef struct {
    int num_items;
    sentience_inventory_item_t items[SENTIENCE_MAX_INVENTORY];
    int capacity_max_weight;    /* carry_weight_max(ch) */
    int capacity_current_weight;/* carry_weight(ch) */
    int capacity_max_items;     /* can_carry_n(ch) */
    int capacity_current_items; /* number of top-level items */
    int capacity_coin_weight;   /* COIN_WEIGHT(ch) */
} sentience_inventory_input_t;

json_t *sentience_build_inventory_json(const sentience_inventory_input_t *input);
```

- [ ] **Step 4: Implement builder function**

In `gmcp_sentience.c`, implement `sentience_build_inventory_json()`:
- Iterate items, build JSON array with all fields per spec §2
- Build `capacity` object
- Each item gets `id` as 2-element JSON array `[id[0], id[1]]`, `actions` array
- `flags` as JSON array of strings from `extra_flags[]` table lookup
- `condition` as integer + `condition_label` as string

- [ ] **Step 5: Run test to verify it passes**

Run: `cd /sentience && ./sent -test:gmcp_inv`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add Char.Inventory builder with actions and disambiguation

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Equipment Builder

Build `sentience_build_equipment_json()` with slot layout and actions.

**Files:**
- Modify: `gmcp_sentience.h` — equipment input struct
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/unit/gmcp_sentience_tests.c`, `tests/data/unit/gmcp_sentience_unit_tests.json`
- Reference: `act_info.c` (`show_equipment`, `wear_view_order[]`, `where_name[]`, `wear_params[][]`), `merc.h` (WEAR_* constants, 51 slots)

**Key APIs:**
- `wear_view_order[]` — display order for slots
- `where_name[]` — slot display names like `"<worn on head>"` 
- `wear_params[slot][0]` — visibility (0 = hidden)
- `wear_params[slot][2]` — removability
- `wear_params[slot][3]` — shifted form availability
- `get_eq_char(ch, slot)` — get item in slot (or NULL)

- [ ] **Step 1: Write failing test — equipment builder**

Test: slot with item (has remove/examine actions), empty slot, concealed slot handling.

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Implement equipment input struct and builder**

In `gmcp_sentience.h`:

```c
#define SENTIENCE_MAX_EQUIPMENT_SLOTS 51

typedef struct {
    int slot_id;
    const char *slot_name;      /* where_name[slot] */
    bool occupied;
    /* Fields below only valid if occupied == true */
    const char *item_name;      /* short_descr */
    const char *keywords;       /* full obj->name string for display */
    const char *keyword;        /* first keyword for action cmds */
    unsigned long id[2];        /* instance ID — emitted as JSON array [id0, id1] */
    const char *item_type;
    int condition;              /* integer 0-100 */
    const char *condition_label;
    int level;
    int num_flags;
    const char *flags[SENTIENCE_MAX_ITEM_FLAGS];
    int num_actions;
    sentience_item_action_t actions[SENTIENCE_MAX_ITEM_ACTIONS];
} sentience_equipment_slot_t;

typedef struct {
    int num_slots;
    sentience_equipment_slot_t slots[SENTIENCE_MAX_EQUIPMENT_SLOTS];
} sentience_equipment_input_t;

json_t *sentience_build_equipment_json(const sentience_equipment_input_t *input);
```

Builder iterates slots in `wear_view_order[]`, skipping non-visible slots. Occupied slots include item details + actions (remove, examine). Empty slots include just slot_name.

- [ ] **Step 4: Run test to verify it passes**

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add Char.Equipment builder with slot layout and actions

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Extended Identity Builder (Classes + Traits + Race Info)

Expand the existing identity builder with full class metadata, traits array, and race info.

**Files:**
- Modify: `gmcp_sentience.h:117-132` — expand `sentience_identity_input_t` struct
- Modify: `gmcp_sentience.c:104-136` — expand `sentience_build_identity_json()`
- Modify: `gmcp_sentience.c:972-1009` — expand identity population in update loop
- Modify: `tests/unit/gmcp_sentience_tests.c`, `tests/data/unit/gmcp_sentience_unit_tests.json`
- Reference: `merc.h` (CLASS_DATA, CLASS_LEVEL, CLASS_TITLE, RACE_DATA), `class_data.h` (CLASS_TYPE_*, class_flags[]), `traits.h` (TRAIT_DEF, TRAIT_VALUE), `act_class.c` (do_classinfo for field reference)

**Key APIs:**
- `cl->clazz->type` → `class_types[].name` — class type string
- `cl->clazz->max_level` — max level
- `cl->clazz->flags` → `class_flags[]` — flag names
- `cl->clazz->primary_stat` — stat index (0-4)
- `cl->clazz->hp_min`, `cl->clazz->hp_max` — HP range
- `cl->clazz->gains_mana` — boolean
- `cl->clazz->description` — description text
- `cl->xp` — current XP
- `cl->active_title` — chosen title keyword
- `cl->clazz->titles` — LLIST of CLASS_TITLE
- `class_display_ch(clazz, ch)` — body-type-aware display name
- `trait_def_list` — global linked list of all TRAIT_DEF
- `ch_has_trait(ch, trait_def)` — check if trait is active
- `ch_get_trait_bool/int/string(ch, trait_id)` — get effective value
- `flag_string(imm_flags, race->imm)` — convert bitfields to name strings
- `flag_string(res_flags, race->res)`, `flag_string(vuln_flags, race->vuln)`
- `flag_string(affect_flags, race->aff[0])` — affect names
- `flag_string(size_flags, race->min_size)` — size name

- [ ] **Step 1: Write failing test — extended identity**

Update existing identity test scenario OR add new `gmcp_identity_ext` scenario testing:
- Class entry has `max_level`, `type`, `flags`, `primary_stat`, `hp_range`, `gains_mana`, `xp`, `active_title`, `available_titles`
- `traits` array present with `id`, `name`, `type`, `value`, `source`
- `race_info` object present with stats, skills, resistances

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Expand identity input struct**

In `gmcp_sentience.h`, replace the anonymous class struct with a richer one:

```c
#define SENTIENCE_MAX_TRAITS 32
#define SENTIENCE_MAX_TITLES 8

typedef struct {
    const char *keyword;
    const char *display;
    bool is_default;
} sentience_class_title_t;

typedef struct {
    const char *id;
    const char *name;
    int level;
    bool is_primary;
    /* New fields */
    int max_level;
    const char *type;           /* class type name */
    const char *flags;          /* space-separated flag names */
    const char *primary_stat;   /* stat name */
    int hp_min, hp_max;
    bool gains_mana;
    const char *description;
    long xp;
    const char *active_title;
    int num_titles;
    sentience_class_title_t titles[SENTIENCE_MAX_TITLES];
    const char *action_label;   /* NULL for primary class */
    const char *action_cmd;     /* NULL for primary class */
} sentience_identity_class_t;

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    const char *category;
    const char *type;           /* "bool", "int", "string" */
    const char *source;         /* "personal", "class", "race" */
    bool value_bool;
    int value_int;
    const char *value_string;
} sentience_trait_t;

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    bool playable;
    bool starting;
    const char *size;
    int stats[5];               /* STR/INT/WIS/DEX/CON */
    int max_stats[5];
    int max_vitals[3];          /* HP/Mana/Move */
    int num_skills;
    const char *skills[16];
    const char *resistances;    /* space-separated from flag_string */
    const char *vulnerabilities;
    const char *immunities;
    const char *affects;
    const char *remort_into;
    int num_traits;
    sentience_trait_t traits[SENTIENCE_MAX_TRAITS]; /* race-specific trait values */
} sentience_race_info_t;

typedef struct {
    const char *name;
    const char *race_wnum;
    const char *race_name;
    const char *body_type;
    int level;
    int tot_level;
    const char *title;
    int num_classes;
    sentience_identity_class_t classes[SENTIENCE_MAX_CLASSES];
    int num_traits;
    sentience_trait_t traits[SENTIENCE_MAX_TRAITS];
    sentience_race_info_t race_info;
} sentience_identity_input_t;
```

- [ ] **Step 4: Expand builder to emit new fields**

Update `sentience_build_identity_json()` in `gmcp_sentience.c` to emit all new class fields, traits array, and race_info object.

- [ ] **Step 5: Expand population in update loop**

Update the identity population block in `sentience_gmcp_update()` (~line 972) to fill in:
- Extended class fields from `cl->clazz` and `cl`
- Traits from `trait_def_list` iteration
- Race info from `ch->race`

- [ ] **Step 6: Run tests to verify all pass**

Run: `cd /sentience && ./sent -test:gmcp`
Expected: All pass

- [ ] **Step 7: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): extend Identity with full class metadata, traits, and race info

Classes: type, flags, primary_stat, hp_range, gains_mana, description,
xp, active_title, available_titles, setclass action.
Traits: active traits with source (personal/class/race).
Race info: stats, skills, resistances, vulnerabilities, affects.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 6: Abilities Builder

Build `sentience_build_abilities_json()` for unified skills/spells/songs.

**Files:**
- Modify: `gmcp_sentience.h` — abilities input struct
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/`
- Reference: `skills.c` (`list_skill_entries`, `skill_entry_is_usable_now`, `skill_entry_rating`, `skill_entry_mana`, `skill_entry_level`), `merc.h` (SKILL_ENTRY, SONG_DATA)

**Key APIs:**
- `ch->pcdata->sorted_skills` — LLIST of SKILL_ENTRY (skills + spells)
- `ch->pcdata->sorted_songs` — LLIST of SKILL_ENTRY (songs)
- `entry->isspell` — true for spells, false for skills
- `entry->song != NULL` — identifies song entries
- `skill_entry_is_usable_now(ch, entry)` — availability check
- `skill_entry_rating(ch, entry)` — proficiency 0–100+
- `skill_entry_mana(ch, entry)` — mana cost
- `skill_entry_level(ch, entry)` — learned level
- `entry->skill->target` — target type (TAR_CHAR_OFFENSIVE, TAR_CHAR_DEFENSIVE, etc.)

- [ ] **Step 1: Write failing test — abilities builder**

Test: spell with Cast action, skill with Use action, passive skill (no action), song with Play action, unavailable ability (empty actions).

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Implement abilities input struct and builder**

In `gmcp_sentience.h`:

```c
#define SENTIENCE_MAX_ABILITIES 256

typedef struct {
    const char *name;
    const char *type;           /* "skill", "spell", "song" */
    bool available;
    int rating;
    int modifier;
    int mana;
    int level;
    const char *target;         /* "offensive", "defensive", "self", "object", "passive", "ignore" */
    bool can_practice;
    int learn_rate;
    int num_actions;
    sentience_item_action_t actions[2];  /* Cast/Play/Use at most */
} sentience_ability_t;

typedef struct {
    int num_abilities;
    sentience_ability_t abilities[SENTIENCE_MAX_ABILITIES];
} sentience_abilities_input_t;

json_t *sentience_build_abilities_json(const sentience_abilities_input_t *input);
```

- [ ] **Step 4: Run test to verify it passes**

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add Char.Abilities builder for skills/spells/songs

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 7: Reputations Builder

Build `sentience_build_reputations_json()` for faction standings.

**Files:**
- Modify: `gmcp_sentience.h` — reputations input struct
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/`
- Reference: `reputation.c`, `merc.h` (REPUTATION_DATA, REPUTATION_INDEX_DATA)

**Key APIs:**
- `ch->pcdata->reputations` — LLIST of REPUTATION_DATA
- `rep->pIndexData->name` — faction name
- `rep->reputation` — current points
- `rep->paragon_level` — paragon tier
- `rep->maximum_rank` — highest achieved
- `get_reputation_rank(rep)` — returns current REPUTATION_RANK
- `REPUTATION_HIDDEN` flag — exclude from display
- `rank->name`, `rank->color` — rank display info

- [ ] **Step 1: Write failing test — reputations builder**

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Implement reputations input struct and builder**

In `gmcp_sentience.h`:

```c
#define SENTIENCE_MAX_REPUTATIONS 32

typedef struct {
    const char *name;
    const char *rank;
    const char *rank_color;
    int points;
    int paragon_level;
    const char *max_rank;
} sentience_reputation_t;

typedef struct {
    int num_reputations;
    sentience_reputation_t reputations[SENTIENCE_MAX_REPUTATIONS];
} sentience_reputations_input_t;

json_t *sentience_build_reputations_json(const sentience_reputations_input_t *input);
```

- [ ] **Step 4: Run test to verify it passes**

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add Char.Reputations builder for faction standings

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 8: Church Builder

Build `sentience_build_church_json()` for church membership info.

**Files:**
- Modify: `gmcp_sentience.h` — church input struct
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/`
- Reference: `church.c`, `merc.h` (CHURCH_DATA, CHURCH_MEMBER)

**Key APIs:**
- `ch->pcdata->church` — CHURCH_DATA pointer (NULL if no church)
- `ch->pcdata->church_member` — CHURCH_MEMBER pointer
- `church->name`, `church->flag` — church identity
- `church->alignment` — alignment enum
- `church->size` — size type
- `church->pk` — PK flag
- `church_member->rank` — rank info
- `has_church_permission(ch, perm)` — permission check
- `church_command_table[]` — available commands

- [ ] **Step 1: Write failing test — church builder (member and non-member)**

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Implement church input struct and builder**

In `gmcp_sentience.h`:

```c
#define SENTIENCE_MAX_CHURCH_ACTIONS 12

typedef struct {
    bool is_member;
    /* Church info (only valid if is_member) */
    const char *church_name;
    const char *church_flag;
    const char *alignment;      /* "good", "evil", "neutral" */
    const char *size;           /* "band", "cult", "order", "church" */
    bool pk;
    /* Rank info */
    const char *rank_name;
    const char *rank_type;      /* "member", "officer", "leader" */
    const char *rank_title;
    /* Actions */
    int num_actions;
    sentience_item_action_t actions[SENTIENCE_MAX_CHURCH_ACTIONS];
} sentience_church_input_t;

json_t *sentience_build_church_json(const sentience_church_input_t *input);
```

- [ ] **Step 4: Run test to verify it passes**

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add Char.Church builder for membership info

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 9: Standalone Race Builder

Build `sentience_build_race_json()` as a dedicated package sent on login and race change.

**Files:**
- Modify: `gmcp_sentience.h` — race input struct (reuse `sentience_race_info_t` from Task 5)
- Modify: `gmcp_sentience.c` — builder function
- Modify: `tests/`
- Reference: `merc.h` (RACE_DATA), `tables.c` (imm_flags, res_flags, vuln_flags, affect_flags, size_flags), `bit.c` (flag_string)

**Key APIs:**
- `ch->race` — RACE_DATA pointer
- `race->trait_values` — indexed array of trait values
- `flag_string(imm_flags, race->imm)` — immunity names
- `flag_string(res_flags, race->res)` — resistance names
- `flag_string(vuln_flags, race->vuln)` — vulnerability names
- `flag_string(affect_flags, race->aff[0])` — affect names
- `flag_string(size_flags, race->min_size)` — size name

- [ ] **Step 1: Write failing test — race builder**

Test: race with skills, resistances, vulnerabilities, traits. Race with no skills/traits (empty arrays).

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Implement race builder**

The `sentience_race_info_t` struct from Task 5 serves as the input. Add:

```c
json_t *sentience_build_race_json(const sentience_race_info_t *input);
```

Builder emits all fields per spec §9: id, name, description, stats, max_stats,
max_vitals, skills array, resistances/vulnerabilities/immunities/affects as
JSON arrays of strings (split flag_string output on spaces), remort_into,
and a traits array for race-specific trait values.

- [ ] **Step 4: Run test to verify it passes**

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c tests/
git commit -m "feat(gmcp): add standalone Char.Race builder

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 10: Channel.Message Enhancement (report_id + actions)

Thread `report_id` through channel message flow and add actions array.

**Files:**
- Modify: `gmcp_sentience.h:232-238` — expand `sentience_channel_message_input_t`
- Modify: `gmcp_sentience.c:202-220` — expand `sentience_build_channel_message()`
- Modify: `channels/channel_gmcp.c:96-187` — add `report_id` parameter to both functions
- Modify: `channels/channel_service.c` — pass `report_id` at call sites (~lines 3389, 3414, 3427, 3551, 3598)
- Modify: `tests/`

**Call flow (from explorer analysis):**
1. `channel_history_append()` generates `report_id` → writes to `out_report_id`
2. GMCP functions called AFTER history append in most paths
3. Exception: publish-success paths skip history; use empty report_id

- [ ] **Step 1: Write failing test — channel message with report_id and actions**

Test: message with report_id + info/report actions. Directed message with reply action.

- [ ] **Step 2: Run test to verify it fails**

- [ ] **Step 3: Expand input struct**

In `gmcp_sentience.h`, add to `sentience_channel_message_input_t`:

```c
typedef struct {
    const char *channel;
    const char *sender;
    const char *text;
    long        timestamp;
    const char *tell_target;
    /* New fields */
    const char *report_id;
    int num_actions;
    sentience_item_action_t actions[4];  /* Info, Report, Reply */
} sentience_channel_message_input_t;
```

- [ ] **Step 4: Expand builder to emit report_id and actions**

Update `sentience_build_channel_message()` to emit `report_id` and `actions` array.

- [ ] **Step 5: Thread report_id through channel_gmcp functions**

Add `const char *report_id` parameter to `channel_gmcp_broadcast()` and `channel_gmcp_send_directed()` in `channels/channel_gmcp.c`. Build action array server-side (Info + Report for all, Reply for directed). Update all call sites in `channel_service.c` to pass `appended_report_id`.

- [ ] **Step 6: Run tests**

Run: `cd /sentience && ./sent -test:gmcp`
Expected: All pass

- [ ] **Step 7: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c channels/channel_gmcp.c channels/channel_service.c tests/
git commit -m "feat(gmcp): add report_id and actions to Channel.Message

Thread report_id from channel history through GMCP send path.
Add Info, Report, and Reply (directed only) action buttons.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 11: Update Loop Integration & Capabilities

Wire all new builders into `sentience_gmcp_update()` and register capabilities.

**Files:**
- Modify: `gmcp_sentience.c:798+` — add dirty detection + send blocks for all new packages
- Modify: `gmcp_sentience.c` — update `sentience_build_client_ready_capabilities_json()` (~line 222)
- Modify: `protocol.h` — add GMCP support enum entries if needed

**Dirty detection patterns:**
- **Inventory**: count visible items in `ch->lcarrying`, compare to `cache->inventory_count`
- **Equipment**: count equipped items in `ch->lworn` (or worn slots), compare to `cache->equipment_count`
- **Abilities**: count `sorted_skills` + `sorted_songs` entries, compare to `cache->abilities_count` (-1 forces rebuild)
- **Reputations**: count `ch->pcdata->reputations` entries, compare to `cache->reputation_count` (-1 forces rebuild)
- **Church**: check `ch->pcdata->church != NULL` vs `cache->has_church`, check `church->uid` vs `cache->church_uid`
- **Race**: check `ch->race->uid` vs `cache->race_uid`

- [ ] **Step 1: Add capabilities registration**

In `sentience_build_client_ready_capabilities_json()`, add:
```c
json_array_append_new(arr, json_string("Sentience.Char.Inventory 1"));
json_array_append_new(arr, json_string("Sentience.Char.Equipment 1"));
json_array_append_new(arr, json_string("Sentience.Char.Abilities 1"));
json_array_append_new(arr, json_string("Sentience.Char.Reputations 1"));
json_array_append_new(arr, json_string("Sentience.Char.Church 1"));
json_array_append_new(arr, json_string("Sentience.Char.Race 1"));
```

Update existing capabilities count test to expect new count (currently 15, will be 21).

- [ ] **Step 2: Add dirty detection for all new packages**

In `sentience_gmcp_update()`, after existing dirty checks, add fingerprint comparisons for inventory, equipment, abilities, reputations, church, and race.

- [ ] **Step 3: Add send blocks for all new packages**

For each new package, add population + send block following the existing pattern (e.g., the Room.Contents fingerprint block). Each block:
1. Populates input struct from character data
2. Calls builder
3. Sends via `sentience_send_package()`
4. Updates cache

- [ ] **Step 4: Add event-triggered invalidation hooks**

In `reputation.c`: after `gain_reputation()`, `set_reputation_rank()`, `group_gain_reputation()` succeed, call a new helper that sets `cache->reputation_count = -1`.

In `do_setclass` (wherever it lives): after successful class swap, set `cache->abilities_count = -1`.

Create helper function `sentience_invalidate_cache(CHAR_DATA *ch, unsigned int flags)` that finds the descriptor and sets appropriate cache field to -1.

- [ ] **Step 5: Build and run full test suite**

Run: `cd /sentience/src && ./build tests && cd /sentience && ./sent -test:gmcp`
Expected: All pass (capabilities count updated)

- [ ] **Step 6: Commit**

```bash
git add gmcp_sentience.h gmcp_sentience.c protocol.h reputation.c tests/
git commit -m "feat(gmcp): wire Phase 4 packages into update loop and capabilities

Dirty-flag detection for inventory, equipment, abilities, reputations.
Event-triggered invalidation for class swap and reputation changes.
Church and race use simple change detection.
Register 6 new capabilities.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 12: Web Client Reference Documentation

Update `docs/GMCP_WEB_CLIENT_REFERENCE.md` with full documentation for all new packages.

**Files:**
- Modify: `docs/GMCP_WEB_CLIENT_REFERENCE.md`

- [ ] **Step 1: Add Preferences section**

Document full JSON structure, source metadata, client→server update format, bidirectional flow.

- [ ] **Step 2: Add Inventory section**

Document JSON structure, action catalog (wear/remove/drop/get/examine/eat/drink/quaff), keyword disambiguation rules, capacity object.

- [ ] **Step 3: Add Equipment section**

Document slot layout, JSON structure, action catalog (remove/examine), slot rendering order.

- [ ] **Step 4: Add Identity extensions section**

Document new class fields, traits array, race_info object, class swap actions.

- [ ] **Step 5: Add Abilities section**

Document unified skills/spells/songs, action types, availability, practice info.

- [ ] **Step 6: Add Reputations section**

Document faction standings, rank info, points, paragon levels.

- [ ] **Step 7: Add Church section**

Document membership info, rank permissions, available actions.

- [ ] **Step 8: Add Channel.Message enhancements section**

Document report_id, actions (Info/Report/Reply), directed channel handling.

- [ ] **Step 9: Commit**

```bash
git add docs/GMCP_WEB_CLIENT_REFERENCE.md
git commit -m "docs: add Phase 4 GMCP packages to web client reference

Preferences, Inventory, Equipment, Identity extensions, Abilities,
Reputations, Church, Channel.Message enhancements.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task Dependency Graph

```
Task 1 (Cache/Flags) ──┬──> Task 2 (Preferences)
                        ├──> Task 3 (Inventory)
                        ├──> Task 4 (Equipment)
                        ├──> Task 5 (Identity)
                        ├──> Task 6 (Abilities)
                        ├──> Task 7 (Reputations)
                        ├──> Task 8 (Church)
                        ├──> Task 9 (Race)
                        ├──> Task 10 (Channel.Message)
                        └──> Task 11 (Update Loop) ──> Task 12 (Docs)

Tasks 2–10 can be done in any order after Task 1.
Task 11 depends on Tasks 2–10 (wires them all together).
Task 12 depends on Task 11.
```
