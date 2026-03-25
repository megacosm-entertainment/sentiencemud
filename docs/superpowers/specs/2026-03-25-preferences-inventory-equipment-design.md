# Sentience.Client.Preferences (Expanded), Sentience.Char.Inventory & Sentience.Char.Equipment Design Spec

> Date: 2026-03-25
> Status: Draft
> Scope: Three new/expanded GMCP packages for the Sentience web client

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
  with a new builder that iterates all preferences via the existing API:
  `pref_find()`, `pref_get_source()`, `pref_get_bool()`, etc.
- Labels come from `pc_set_table[].name` for toggles and are synthesized for
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
        {"label": "Quaff", "cmd": "quaff potion"},
        {"label": "Drop", "cmd": "drop potion"},
        {"label": "Examine", "cmd": "examine potion"}
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

### Item Fields

- `id` — 2-element array `[obj->id[0], obj->id[1]]` for unique identification
- `name` — `obj->short_descr` (display name)
- `keywords` — `obj->name` (space-separated keywords for targeting)
- `item_type` — string from `item_type_name()` lookup
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
- Track `inventory_count` + `inventory_fingerprint` in `sentience_gmcp_cache_t`
- Fingerprint: count of visible items in `ch->lcarrying`
- On mismatch → rebuild and send full inventory JSON
- Additionally, trigger immediate send after inventory-modifying commands
  (get, drop, wear, remove, give, sacrifice, etc.)

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
- Track `equipment_count` + `equipment_fingerprint` in `sentience_gmcp_cache_t`
- Fingerprint: count of equipped items in `ch->lworn`
- On mismatch → rebuild and send full equipment JSON

---

## 4. Shared Implementation Notes

### Build System

New builder/sender functions go in `gmcp_sentience.c` / `gmcp_sentience.h`.
No new source files needed. Update both `CMakeLists.txt` and `Makefile` only
if new `.c` files are added.

### Capabilities

Register all three packages in `sentience_build_client_ready_capabilities_json()`:
- `"Sentience.Client.Preferences 1"` (already registered, version stays 1)
- `"Sentience.Char.Inventory 1"` (new)
- `"Sentience.Char.Equipment 1"` (new)

### Receive Table

Add `GMCP_SENTIENCE_CHAR_INVENTORY` to enum and `GMCPReceiveTable` for container
inspect requests. Preferences receive handler already exists.

### Testing

Unit tests for all new builders following existing pattern:
- JSON test data in `tests/data/unit/gmcp_sentience_unit_tests.json`
- Scenario runners in `tests/unit/gmcp_sentience_tests.c`
- Test: builder output structure, field types, action generation logic

### Web Client Reference

Update `docs/GMCP_WEB_CLIENT_REFERENCE.md` with full documentation for all
three packages including JSON examples, action catalogs, and integration notes.

### Future Extensions

- **Compare**: Add `{"label": "Compare", "cmd": "compare sword"}` action for
  wearable items when the character has an item of the same type equipped.
  Deferred — requires compare command implementation first.
- **Container inspection**: Full support for browsing container contents
  via GMCP. Marked as stretch goal.
