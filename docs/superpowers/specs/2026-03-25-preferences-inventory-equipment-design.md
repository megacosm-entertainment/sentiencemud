# GMCP Character Data Packages Design Spec

> Date: 2026-03-25
> Status: Draft
> Scope: Seven new/expanded GMCP packages for the Sentience web client

---

## 1. Sentience.Client.Preferences (Expanded)

### Purpose

Expand the existing Sentience.Client.Preferences package to expose the full
preference system over GMCP. Currently only 3 GMCP-category booleans are sent.
The expanded version sends all preferences across all categories with source
metadata, enabling a rich settings UI that shows inheritance (default → account
→ character) and allows bidirectional editing at any scope.

### Direction

Bidirectional. Server pushes full state on login and after any change. Client
sends partial updates specifying key, value, and scope.

### Server → Client (Full State)

Package: `Sentience.Client.Preferences`

```json
{
  "_v": 1,
  "preferences": [
    {
      "key": "autoloot",
      "category": "toggle",
      "type": "bool",
      "value": true,
      "source": "character",
      "label": "Auto Loot"
    },
    {
      "key": "prompt",
      "category": "prompt",
      "type": "string",
      "value": "{B<{x%h{Bhp {x%m{Bm {x%v{Bmv>{x",
      "source": "character",
      "label": "Prompt"
    },
    {
      "key": "gmcp_suppress_channels",
      "category": "gmcp",
      "type": "bool",
      "value": false,
      "source": "account",
      "label": "Suppress Inline Channels"
    }
  ]
}
```

Fields per preference:
- `key` — preference key (matches internal `PREF_ENTRY.key`)
- `category` — one of: `"toggle"`, `"channel"`, `"prompt"`, `"display"`, `"gmcp"`
- `type` — one of: `"bool"`, `"int"`, `"string"`, `"bitfield"`
- `value` — effective value (after inheritance resolution)
- `source` — where the effective value comes from: `"default"`, `"account"`, `"character"`
- `label` — human-readable display label

### Client → Server (Partial Update)

The client sends a partial update targeting a single preference:

```json
{"action": "set", "key": "autoloot", "value": true, "scope": "character"}
```

- `action` — `"set"` or `"reset"` (reset removes the override at the given scope)
- `key` — preference key (must exist in the system)
- `value` — new value (type must match the preference type)
- `scope` — `"account"` or `"character"`

Server validates the key exists, the type matches, and the scope is valid.
On success, persists the change and echoes the full preference state back.
On error, sends:

```json
{"error": "invalid_key", "message": "Unknown preference key: foo"}
```

Error codes: `"invalid_key"`, `"invalid_value"`, `"invalid_scope"`, `"invalid_action"`.

### Caching

No dirty-flag needed — preferences are event-driven. The full state is sent:
1. On login (after entering the game)
2. After any GMCP preference update from the client
3. After any in-game `config` / `toggle` command that changes a preference

### Implementation Notes

- The existing `sentience_send_client_preferences()` function will be replaced
  with a new builder that iterates all preference keys.
- The master key list is built by walking `pc_set_table[]` (43+ toggle keys,
  terminated by NULL) and `game_settings.pref_defaults` (PREF_ENTRY linked list
  for runtime overrides and non-toggle preferences).
- For each key, call `pref_get_source()` to determine origin, then the
  appropriate `pref_get_bool()` / `pref_get_int()` / `pref_get_string()` /
  `pref_get_bitfield()` to get the effective value.
- Labels come from `pc_set_table[].name` for toggles and `PREF_ENTRY.key` for
  other categories.
- The existing receive handler in protocol.c will be expanded to support the
  new action format while remaining backward-compatible with the current
  `{"key": value}` format.

---

## 2. Sentience.Char.Inventory

### Purpose

Server → Client push of the character's carried inventory, with per-item
metadata and embedded action lists. Enables a web client inventory panel with
context menus (drop, examine, wear, eat, drink, etc.) without parsing text.

### Direction

Primarily Server → Client (push). Client → Server for container inspection.

### Server → Client (Push)

Package: `Sentience.Char.Inventory`

```json
{
  "_v": 1,
  "items": [
    {
      "id": [196228330, 1],
      "name": "a sharp sword",
      "keywords": "sharp sword",
      "item_type": "weapon",
      "level": 15,
      "weight": 5,
      "condition": 85,
      "condition_label": "Used",
      "flags": ["glow", "magic"],
      "actions": [
        {"label": "Wear", "cmd": "wear sword"},
        {"label": "Drop", "cmd": "drop sword"},
        {"label": "Examine", "cmd": "examine sword"},
        {"label": "Sacrifice", "cmd": "sacrifice sword"},
        {"label": "Keep", "cmd": "keep sword"}
      ]
    },
    {
      "id": [196228330, 5],
      "name": "a healing potion",
      "keywords": "healing potion",
      "item_type": "potion",
      "level": 10,
      "weight": 1,
      "condition": 100,
      "condition_label": "",
      "flags": ["magic"],
      "actions": [
        {"label": "Quaff", "cmd": "quaff 2.potion"},
        {"label": "Drop", "cmd": "drop 2.potion"},
        {"label": "Examine", "cmd": "examine 2.potion"}
      ]
    }
  ],
  "capacity": {
    "items": 12,
    "max_items": 30,
    "weight": 45,
    "max_weight": 200,
    "coin_weight": 3
  }
}
```

### Keyword Disambiguation

When multiple items share the same first keyword, action commands must use the
MUD's `N.keyword` syntax to target the correct item. The builder tracks keyword
occurrence counts and emits `"drop 2.potion"` for the second potion, etc.
The first occurrence uses the plain keyword (no prefix).

### Client → Server (Container Inspection) — Stretch Goal

```json
{"action": "inspect", "id": [196228330, 5]}
```

Server responds with:

```json
{
  "_v": 1,
  "container_id": [196228330, 5],
  "container_name": "a leather bag",
  "items": [ /* same item format as above */ ]
}
```

Package for response: `Sentience.Char.Inventory.Container`

The receive table entry for container inspect is deferred until the stretch
goal is implemented.

### Item Fields

- `id` — 2-element array `[obj->id[0], obj->id[1]]` for unique identification
- `name` — `obj->short_descr` (display name)
- `keywords` — `obj->name` (space-separated keywords for targeting)
- `item_type` — string from `item_type_info[obj->item_type].name`
- `level` — `obj->level`
- `weight` — `obj->weight`
- `condition` — `obj->condition` (0–100)
- `condition_label` — human-readable from `object_damage_table[]`
- `flags` — array of lowercase flag names for display-relevant flags only:
  `"glow"`, `"hum"`, `"invis"`, `"magic"`, `"evil"`, `"bless"`, `"kept"`,
  `"activated"`, `"planted"`, `"buried"`
- `actions` — server-determined list of valid actions (see Action Determination)

### Action Determination

The server determines valid actions per item based on item type, flags, and
character state. Actions are generated at build time, not cached.

**Universal actions** (all carried items):
- Drop (unless ITEM_NODROP)
- Examine
- Sacrifice (unless ITEM_NO_SAC)

**Type-specific actions:**
- Wearable items (has wear flags beyond ITEM_TAKE): Wear
- ITEM_FOOD: Eat
- ITEM_POTION/ITEM_PILL: Quaff
- ITEM_SCROLL: Recite
- ITEM_WAND: Zap
- ITEM_STAFF: Brandish
- ITEM_DRINK_CON/ITEM_FOUNTAIN: Drink
- ITEM_CONTAINER: Open/Close (if CONT_CLOSEABLE), Look In
- ITEM_WEAPON_CONTAINER: similar to container
- ITEM_INSTRUMENT: Play

**Conditional actions:**
- Keep/Unkeep (toggle based on ITEM_KEPT flag)
- If character is not blind and not shifted (for wear)

### Caching / Update Frequency

Dirty-flag fingerprint on the game pulse loop (matching Room.Contents pattern):
- Track `inventory_count` in `sentience_gmcp_cache_t`
- Fingerprint: count of visible items in `ch->lcarrying`
- On mismatch → rebuild and send full inventory JSON

---

## 3. Sentience.Char.Equipment

### Purpose

Server → Client push of the character's equipped items by wear slot, with
embedded actions. Enables a web client equipment panel (paper-doll or slot
list) with context menus.

### Direction

Server → Client (push only).

### Server → Client (Push)

Package: `Sentience.Char.Equipment`

```json
{
  "_v": 1,
  "slots": [
    {
      "slot": "light",
      "slot_name": "<used as light>",
      "item": {
        "id": [196228330, 1],
        "name": "a glowing lantern",
        "keywords": "glowing lantern",
        "item_type": "light",
        "level": 5,
        "condition": 100,
        "condition_label": "",
        "flags": ["glow"],
        "actions": [
          {"label": "Remove", "cmd": "remove lantern"},
          {"label": "Examine", "cmd": "examine lantern"}
        ]
      }
    },
    {
      "slot": "body",
      "slot_name": "<worn on body>",
      "item": null
    },
    {
      "slot": "wield",
      "slot_name": "<wielded>",
      "item": {
        "id": [196228330, 3],
        "name": "Vengeance",
        "keywords": "sword vengeance",
        "item_type": "weapon",
        "level": 40,
        "condition": 95,
        "condition_label": "",
        "flags": ["hum"],
        "actions": [
          {"label": "Remove", "cmd": "remove vengeance"},
          {"label": "Examine", "cmd": "examine vengeance"}
        ]
      }
    }
  ]
}
```

### Slot Rendering

- Slots are sent in `wear_view_order[]` order (same as in-game `equipment` command)
- Empty slots included with `"item": null` so the client can render the full layout
- Slot filtering respects character state:
  - Shifted-form slots hidden when `wear_params[iWear][3]` is false
  - Secondary/shield/hold hidden when empty and both hands full
- `slot` — lowercase identifier derived from wear location constant name
  (e.g., `WEAR_WIELD` → `"wield"`, `WEAR_FINGER_L` → `"finger_l"`)
- `slot_name` — display string from `where_name[]` with MUD color codes stripped

### Equipment Actions

**Standard equipped item actions:**
- Remove (unless ITEM_NOREMOVE and not WEAR_ALWAYSREMOVE, or tattoo)
- Examine

**Additional based on type:**
- Weapons: no additional (remove + examine covers it)
- Container-type equipment: Look In

### Caching / Update Frequency

Same dirty-flag fingerprint pattern:
- Track `equipment_count` in `sentience_gmcp_cache_t`
- Fingerprint: count of equipped items in `ch->lworn`
- On mismatch → rebuild and send full equipment JSON

---

## 4. Sentience.Char.Identity (Extended Classes)

### Purpose

Extend the existing `Sentience.Char.Identity` package to include the full list
of unlocked classes with enough detail for a client-side class swap UI. The
current implementation already sends class name/level/is_primary — this adds
class type, max level, and a `setclass` action command.

### Changes to Existing JSON

The `classes` array gains new fields, and a new `traits` array is added:

```json
{
  "_v": 1,
  "name": "Tieryo",
  "race": "Human",
  "body_type": "male",
  "level": 40,
  "tot_level": 120,
  "title": "the Archmage",
  "classes": [
    {
      "id": "mage",
      "name": "Mage",
      "level": 40,
      "max_level": 60,
      "type": "mage",
      "is_primary": true,
      "action": null
    },
    {
      "id": "warrior",
      "name": "Warrior",
      "level": 35,
      "max_level": 60,
      "type": "warrior",
      "is_primary": false,
      "action": {"label": "Switch to Warrior", "cmd": "setclass warrior"}
    },
    {
      "id": "herbalist",
      "name": "Herbalist",
      "level": 10,
      "max_level": 30,
      "type": "gathering",
      "is_primary": false,
      "action": {"label": "Switch to Herbalist", "cmd": "setclass herbalist"}
    }
  ],
  "traits": [
    {
      "id": "cosmic_projection",
      "name": "Cosmic Projection",
      "description": "Can project consciousness across planes.",
      "category": "ability",
      "type": "bool",
      "value": true,
      "source": "race"
    },
    {
      "id": "divine_healer",
      "name": "Divine Healer",
      "description": "Enhanced healing spell effectiveness.",
      "category": "ability",
      "type": "bool",
      "value": true,
      "source": "class"
    },
    {
      "id": "breath_damage_bonus",
      "name": "Breath Damage Bonus",
      "description": "Percentage bonus to breath-type spell damage.",
      "category": "combat",
      "type": "int",
      "value": 25,
      "source": "personal"
    }
  ]
}
```

New fields per class:
- `max_level` — maximum achievable level in this class (`clazz->max_level`)
- `type` — class category string: `"mage"`, `"cleric"`, `"thief"`, `"warrior"`,
  `"crafting"`, `"gathering"`, `"explorer"`
- `action` — `null` for the active class, otherwise `{"label": "Switch to X", "cmd": "setclass X"}`

### Traits Array

All active traits (those with a non-default value from any source) are included.
The trait resolution walks all defined traits and includes any where at least
one layer (personal, class, or race) has set a value.

Fields per trait:
- `id` — trait identifier (matches `TRAIT_DEF.id`)
- `name` — display name (`TRAIT_DEF.name`)
- `description` — tooltip/description (`TRAIT_DEF.description`)
- `category` — grouping: `"combat"`, `"ability"`, `"resource"`, `"survival"`, etc.
- `type` — `"bool"`, `"int"`, or `"string"`
- `value` — effective value after three-layer resolution
- `source` — where the effective value comes from: `"personal"`, `"class"`, or `"race"`

Source determination:
- If `pcdata->trait_values[idx].set` → `"personal"`
- Else if current class has it set → `"class"`
- Else if race has it set → `"race"`
- For boolean traits with OR semantics, source is the highest-priority layer
  that contributes `true`.

### Implementation Notes

- Expand `sentience_identity_input_t` struct to include `max_level`, `type`, and
  `action_cmd` fields per class entry, plus a traits array.
- Expand `sentience_build_identity_json()` to emit the new class fields and
  traits array.
- The `SENTIENCE_MAX_CLASSES` cap (8) remains unchanged.
- Hidden classes (`CLASS_HIDDEN` flag) are excluded from the GMCP list.
- Trait iteration: walk `trait_def_list` (global linked list of all TRAIT_DEF),
  check `ch_has_trait()` for each, include if active.
- Cap traits at a reasonable limit (e.g., 32) to bound JSON size.
- Update existing identity tests to verify new fields.

---

## 5. Sentience.Char.Abilities

### Purpose

Server → Client push of the character's known skills, spells, and songs in a
unified package. Groups by availability (current class can use vs. locked by
class), includes proficiency, mana cost, and practice metadata. Enables a
web client abilities panel with filtering, sorting, and cast/use actions.

### Direction

Server → Client (push only).

### Server → Client (Push)

Package: `Sentience.Char.Abilities`

```json
{
  "_v": 1,
  "abilities": [
    {
      "name": "acid blast",
      "type": "spell",
      "available": true,
      "rating": 85,
      "modifier": 5,
      "mana": 20,
      "level": 16,
      "target": "offensive",
      "can_practice": true,
      "learn_rate": 8,
      "actions": [
        {"label": "Cast", "cmd": "cast 'acid blast'"}
      ]
    },
    {
      "name": "sword",
      "type": "skill",
      "available": true,
      "rating": 100,
      "modifier": 0,
      "mana": 0,
      "level": 1,
      "target": "passive",
      "can_practice": false,
      "learn_rate": 0,
      "actions": []
    },
    {
      "name": "fireball",
      "type": "spell",
      "available": false,
      "rating": 60,
      "modifier": 0,
      "mana": 30,
      "level": 25,
      "target": "offensive",
      "can_practice": false,
      "learn_rate": 0,
      "actions": []
    },
    {
      "name": "Song of Healing",
      "type": "song",
      "available": true,
      "rating": 0,
      "modifier": 0,
      "mana": 40,
      "level": 10,
      "target": "defensive",
      "can_practice": false,
      "learn_rate": 0,
      "actions": [
        {"label": "Play", "cmd": "play 'song of healing'"}
      ]
    }
  ]
}
```

### Fields

- `name` — skill/spell/song display name
- `type` — `"skill"`, `"spell"`, or `"song"`
- `available` — `true` if usable with current class/state, `false` if locked
  (e.g., belongs to another class the character has but isn't active in)
- `rating` — proficiency percentage (0–100+)
- `modifier` — temporary rating modifier (from equipment/buffs)
- `mana` — mana cost (0 for non-spell skills)
- `level` — level at which this ability was/will be learned
- `target` — target type string: `"offensive"`, `"defensive"`, `"self"`,
  `"object"`, `"passive"`, `"ignore"`
- `can_practice` — whether the ability can be practiced at a trainer
- `learn_rate` — percentage gain per practice session (0 if not practicable)
- `actions` — context actions:
  - Spells: `{"label": "Cast", "cmd": "cast 'spell name'"}`
  - Songs: `{"label": "Play", "cmd": "play 'song name'"}`
  - Active skills (non-passive): `{"label": "Use", "cmd": "skill name"}`
  - Passive skills: empty array
  - Unavailable abilities: empty array

### Data Sources

Skills and spells come from `ch->sorted_skills` (SKILL_ENTRY linked list).
Songs come from `ch->sorted_songs` (separate SKILL_ENTRY linked list).
Both use the same `SKILL_ENTRY` struct with `entry->isspell` distinguishing
spells from skills, and `entry->song != NULL` identifying songs.

Availability is determined by `skill_entry_is_usable_now(ch, entry)` for skills
and by checking `songs_learned[uid]` + current class for songs.

### Caching / Update Frequency

Dirty-flag fingerprint on the game pulse loop:
- Track `abilities_count` in `sentience_gmcp_cache_t`
- Fingerprint: count of entries in `ch->sorted_skills` + count of entries in
  `ch->sorted_songs`
- On mismatch → rebuild and send full abilities JSON
- This catches all change sources: class swap, level up, token grants, scripting

---

## 6. Sentience.Char.Reputations

### Purpose

Server → Client push of the character's reputation standings across all
factions. Enables a web client reputation panel showing faction name, current
rank, progress, and rank color.

### Direction

Server → Client (push only).

### Server → Client (Push)

Package: `Sentience.Char.Reputations`

```json
{
  "_v": 1,
  "reputations": [
    {
      "name": "City Guards",
      "rank": "Honored",
      "rank_color": "Y",
      "points": 12500,
      "paragon_level": 0,
      "max_rank": "Honored"
    },
    {
      "name": "Dark Brotherhood",
      "rank": "Hated",
      "rank_color": "R",
      "points": -8000,
      "paragon_level": 0,
      "max_rank": "Neutral"
    }
  ]
}
```

### Fields

- `name` — reputation/faction name (`rep->pIndexData->name`)
- `rank` — current rank name (`get_reputation_rank()` → `rank->name`)
- `rank_color` — display color character (`rank->color`)
- `points` — current reputation points (`rep->reputation`)
- `paragon_level` — paragon progression level (`rep->paragon_level`)
- `max_rank` — highest rank ever achieved (name of rank at `rep->maximum_rank`)

Hidden reputations (`REPUTATION_HIDDEN` flag) are excluded.

### Caching / Update Frequency

Dirty-flag fingerprint:
- Track `reputation_count` in `sentience_gmcp_cache_t`
- Fingerprint: count of entries in `ch->reputations` LLIST
- On mismatch → rebuild and send full reputations JSON

---

## 7. Sentience.Char.Church

### Purpose

Server → Client push of the character's church membership information. Enables
a web client panel showing church details, the character's rank, and available
church actions.

### Direction

Server → Client (push only).

### Server → Client (Push)

Package: `Sentience.Char.Church`

If the character has no church membership, sends:

```json
{
  "_v": 1,
  "member": false
}
```

If the character is a church member:

```json
{
  "_v": 1,
  "member": true,
  "church": {
    "name": "Knights of Valor",
    "flag": "KoV",
    "alignment": "good",
    "size": "order",
    "pk": false
  },
  "rank": {
    "name": "Templar",
    "type": "officer",
    "title": "Sir Tieryo"
  },
  "actions": [
    {"label": "Church Talk", "cmd": "church talk"},
    {"label": "Go to Hall", "cmd": "church gohall"},
    {"label": "Members", "cmd": "church list"},
    {"label": "MOTD", "cmd": "church motd"},
    {"label": "Donate", "cmd": "church donate"}
  ]
}
```

### Fields

**Church info:**
- `name` — church name
- `flag` — short church code/flag
- `alignment` — `"good"`, `"evil"`, or `"neutral"`
- `size` — `"band"`, `"cult"`, `"order"`, or `"church"`
- `pk` — whether this is a PK church

**Rank info:**
- `name` — rank display name
- `type` — `"member"`, `"officer"`, or `"leader"`
- `title` — gender-appropriate title for this rank

**Actions:**
Server-determined based on the character's rank permissions. Only actions the
character has permission to perform are included. Derived from
`church_command_table[]` filtered by `has_church_permission()`.

### Caching / Update Frequency

Event-driven (not pulse-based). Church data changes rarely:
- Sent on login
- Resent when church membership changes (join/leave/rank change)
- Use a simple `has_church` boolean + `church_uid` in cache to detect changes

---

## 8. Shared Implementation Notes

### Build System

New builder/sender functions go in `gmcp_sentience.c` / `gmcp_sentience.h`.
No new source files needed. Update both `CMakeLists.txt` and `Makefile` only
if new `.c` files are added.

### Capabilities

Register all packages in `sentience_build_client_ready_capabilities_json()`:
- `"Sentience.Client.Preferences 1"` (already registered, version stays 1)
- `"Sentience.Char.Inventory 1"` (new)
- `"Sentience.Char.Equipment 1"` (new)
- `"Sentience.Char.Abilities 1"` (new)
- `"Sentience.Char.Reputations 1"` (new)
- `"Sentience.Char.Church 1"` (new)

`Sentience.Char.Identity` version stays at 1 (backward-compatible additions).

### Receive Table

No new receive entries needed for V1 (all new packages are server→client push).
The existing `GMCP_SENTIENCE_CLIENT_PREFERENCES` handler is expanded in-place.
Container inspect (stretch goal) deferred.

### Cache Additions

New fields in `sentience_gmcp_cache_t`:
- `int inventory_count` — visible items in `ch->lcarrying`
- `int equipment_count` — equipped items in `ch->lworn`
- `int abilities_count` — entries in `sorted_skills` + `sorted_songs`
- `int reputation_count` — entries in `ch->reputations`
- `bool has_church` — whether character has church membership
- `long church_uid` — UID of current church (for change detection)

### Testing

Unit tests for all new builders following existing pattern:
- JSON test data in `tests/data/unit/gmcp_sentience_unit_tests.json`
- Scenario runners in `tests/unit/gmcp_sentience_tests.c`
- Test: builder output structure, field types, action generation logic

### Web Client Reference

Update `docs/GMCP_WEB_CLIENT_REFERENCE.md` with full documentation for all
packages including JSON examples, action catalogs, and integration notes.

### Future Extensions

- **Compare**: Add `{"label": "Compare", "cmd": "compare sword"}` action for
  wearable items when the character has an item of the same type equipped.
  Deferred — requires compare command implementation first.
- **Container inspection**: Full support for browsing container contents
  via GMCP. Marked as stretch goal.
- **Skill favorites**: Toggle favorite flag from client, filter by favorites.
