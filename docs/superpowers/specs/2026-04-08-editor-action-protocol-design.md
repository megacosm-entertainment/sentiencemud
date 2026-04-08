# Editor Action Protocol — Design Spec

> Unified GMCP action protocol for add/remove operations in all OLC editors,
> with shared staging between GMCP and telnet paths.

## Problem

Web editors cannot add or remove list items (scripts, resets, affects, types,
quests, etc.) because there is no GMCP protocol for these operations. The
telnet add commands exist but many apply changes immediately rather than
staging through the changeset system. The web client would need server-side
data (trigger types, available mobs, valid object types) to build the correct
forms, and sending all possible options in the initial Editor.Open is too
expensive.

## Solution

1. **New GMCP action protocol** — on-demand request/response for action forms
2. **Action handler registry** — per-editor table of form builders + stage functions
3. **Shared staging core** — GMCP and telnet both call the same `stage_fn`
4. **Convert all immediate-apply commands** to stage through changesets
5. **Add reset deletion** (missing from redit)

## GMCP Protocol

### Message Flow

```
Client                              Server
  │                                    │
  │  Editor.Action                     │
  │  {entity_id, action}               │
  ├───────────────────────────────────►│
  │                                    │  Look up action handler
  │                                    │  Call form_fn(entity)
  │  Editor.Action.Form                │
  │  {entity_id, action, session_id,   │
  │   display, title, fields, _v}      │
  │◄───────────────────────────────────┤
  │                                    │
  │  Client renders form (inline/      │
  │  dialog per display hint)          │
  │                                    │
  │  Editor.Action.Submit              │
  │  {entity_id, session_id, values}   │
  ├───────────────────────────────────►│
  │                                    │  Call stage_fn(entity, cs, values)
  │  Editor.Action.Result              │
  │  {entity_id, session_id,           │
  │   status, message?, _v}            │
  │◄───────────────────────────────────┤
  │                                    │
  │  + Editor.Field (is_pending=true)  │
  │  + Editor.Schema.Update (if tabs   │
  │    structurally changed)           │
  │◄───────────────────────────────────┤
```

### Editor.Action (Client → Server)

Request an action form for a specific operation.

```json
{
  "entity_id": "obj:5#3010",
  "action": "addoprog"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `action` | string | ✓ | Action name (matches `add_command` from list schema) |

### Editor.Action.Form (Server → Client)

Self-describing form with live data for the requested action.

```json
{
  "entity_id": "obj:5#3010",
  "action": "addoprog",
  "session_id": "act_1",
  "display": "inline",
  "title": "Add Object Program",
  "fields": [
    {
      "field": "script",
      "label": "Script",
      "type": "widevnum",
      "required": true
    },
    {
      "field": "trigger",
      "label": "Trigger",
      "type": "enum",
      "options": ["ENTER_PROG", "GREET_PROG", "SPELLCAST", "EXIT_PROG", "EXALL_PROG",
                  "FIGHT_PROG", "DEATH_PROG", "HITPRCNT_PROG", "BRIBE_PROG",
                  "SPEECH_PROG", "GIVE_PROG", "ACT_PROG", "RAND_PROG"],
      "required": true
    },
    {
      "field": "phrase",
      "label": "Phrase",
      "type": "string",
      "required": true,
      "default": "*"
    }
  ],
  "_v": 1
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `action` | string | ✓ | Action name (echo) |
| `session_id` | string | ✓ | Unique session for this action instance |
| `display` | string | ✓ | `"inline"` (new row in list) or `"dialog"` (separate form) |
| `title` | string | ✓ | Human-readable action title |
| `fields` | array | ✓ | Field descriptors (same format as Editor.Open fields) |
| `_v` | int | ✓ | Message version (always `1`) |

**Display hints:**
- `"inline"` — client renders as a new row in the existing list section.
  Best for simple, repetitive additions with few fields (scripts, affects,
  catalysts, quests, trades).
- `"dialog"` — client opens a separate form panel. Best for complex
  additions that need search/lookup (mob resets, object resets).

### Editor.Action.Submit (Client → Server)

Submit completed form values for staging.

```json
{
  "entity_id": "obj:5#3010",
  "session_id": "act_1",
  "values": {
    "script": "5#100",
    "trigger": "ENTER_PROG",
    "phrase": "*"
  }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `session_id` | string | ✓ | Must match the Action.Form session_id |
| `values` | object | ✓ | Field values keyed by field name |

### Editor.Action.Result (Server → Client)

Result of the action submission.

```json
{
  "entity_id": "obj:5#3010",
  "session_id": "act_1",
  "status": "success",
  "_v": 1
}
```

On error:
```json
{
  "entity_id": "obj:5#3010",
  "session_id": "act_1",
  "status": "error",
  "message": "Script 5#999 not found",
  "_v": 1
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `entity_id` | string | ✓ | Entity being edited |
| `session_id` | string | ✓ | Echo of the request session_id |
| `status` | string | ✓ | `"success"` or `"error"` |
| `message` | string | error only | Human-readable error message |
| `_v` | int | ✓ | Message version (always `1`) |

On success, the server also sends:
- `Editor.Field` with `is_pending: true` for the staged list change
- `Editor.Schema.Update` if the tab structure changed (e.g., type added)

### Removes

Most removes continue through the existing `Editor.Set` with `/rm` suffix:

```json
{"entity_id": "obj:5#3010", "field": "affects/rm", "value": {"index": 2}}
```

For script program removes (group+trigger addressing):
```json
{"entity_id": "obj:5#3010", "field": "oprogs/rm", "value": {"group": 1, "trigger": 2}}
```

The remove `value` format is list-specific — each list's apply handler knows
how to interpret its remove values.

**Exceptions — removes that use the action flow:** `removetype` and `delreset`
use `Editor.Action` instead of `/rm` because they require server-provided form
data (the current list of types, or the reset index) that the client doesn't
have. These are listed in the action inventory alongside their add counterparts.

## Server Architecture

### Action Handler Registry

```c
typedef struct olc_action_handler {
    const char          *action_name;   // "addoprog", "mreset", etc.
    int                  editor_type;   // ED_OBJECT, ED_MOBILE, ED_ROOM, ED_AREA
    const char          *display_hint;  // "inline" or "dialog"
    const char          *title;         // "Add Object Program"
    const char          *list_name;     // "oprogs", "resets", etc. (for staging)
    olc_action_form_fn   form_fn;       // Build form with live data
    olc_action_stage_fn  stage_fn;      // Validate + stage the change
} olc_action_handler_t;

typedef json_t *(*olc_action_form_fn)(void *entity, CHAR_DATA *ch);
typedef bool (*olc_action_stage_fn)(void *entity, olc_changeset_t *cs,
                                     json_t *values,
                                     char *errbuf, size_t errlen);
```

A static table of handlers is defined per editor module:
```c
static const olc_action_handler_t oedit_actions[] = {
    { "addaffect",  ED_OBJECT, "inline", "Add Affect",
      "affects", oedit_action_affect_form, oedit_action_affect_stage },
    { "addoprog",   ED_OBJECT, "inline", "Add Object Program",
      "oprogs",  oedit_action_oprog_form,  oedit_action_oprog_stage  },
    // ...
    { NULL }
};
```

A central lookup function finds the handler:
```c
const olc_action_handler_t *olc_find_action(int editor_type, const char *action_name);
```

### GMCP Integration

In `gmcp_editor.c`, add two new handler functions:

```c
static void handle_editor_action(descriptor_t *d, json_t *payload);
static void handle_editor_action_submit(descriptor_t *d, json_t *payload);
```

`handle_editor_action`:
1. Validate entity_id, find active changeset
2. Look up action handler by name + editor type
3. Call `form_fn(entity, ch)` to build form JSON
4. Generate unique session_id (counter per descriptor)
5. Store session_id → action_name mapping on descriptor (for submit validation)
6. Send `Editor.Action.Form`

`handle_editor_action_submit`:
1. Validate entity_id, session_id
2. Look up stored action_name from session_id
3. Find action handler
4. Call `stage_fn(entity, cs, values, errbuf, sizeof(errbuf))`
5. Send `Editor.Action.Result`
6. On success: send `Editor.Field` + `Editor.Schema.Update` as needed
7. Clear session_id mapping

### Telnet Integration

Existing telnet add commands are refactored to call the same `stage_fn`:

```
Before:  oedit_addoprog → parse text → create PROG_LIST → apply immediately
After:   oedit_addoprog → parse text → build JSON values → call stage_fn → stage
```

The telnet command function becomes a thin wrapper:
1. Parse text arguments into structured data
2. Pack into `json_t` values object
3. Call `stage_fn(entity, cs, values, errbuf, sizeof(errbuf))`
4. Display success/error message to character

### Apply Handlers

On commit, the changeset apply infrastructure needs handlers for each
list type. These already exist for oedit lists (affects, spells, catalysts,
quests, oprogs, waypoints) but need to be added for:

- medit: mprogs, quests, reputations
- redit: rprogs, resets, conditional descriptions
- aedit: aprogs, trades

Each apply handler processes `LIST_ADD` and `LIST_REMOVE` changes:
- `LIST_ADD`: creates the runtime data structure from JSON values
- `LIST_REMOVE`: finds the item by index/identifier and removes it

## Complete Action Inventory

### oedit (Object Editor)

| Action | Display | List Name | Currently | Form Fields |
|--------|---------|-----------|-----------|-------------|
| addtype | inline | typedata | stages | type (enum: available types) |
| removetype | inline | typedata | stages | type (enum: current types) |
| addaffect | inline | affects | stages | location (enum), modifier (int), random (int) |
| addimmune | inline | affects | stages | class (enum: immune/resist/vuln), bit (flags), random (int) |
| addspell | inline | spells | stages | spell (enum), level (int), random (int) |
| addskill | inline | affects | partial staging exists | skill (enum), modifier (int), random (int) |
| addoprog | inline | oprogs | immediate→convert | script (widevnum), trigger (enum), phrase (string) |
| addquest | inline | quests | stages | quest (widevnum) |
| addcatalyst | inline | catalysts | stages | type (enum), strength (int), charges (int), chance (int), active (bool) |
| addwaypoint | inline | waypoints | stages | wilderness (widevnum), x (int), y (int), name (string) |

All corresponding del* commands stage removals.

### medit (Mobile Editor)

| Action | Display | List Name | Currently | Form Fields |
|--------|---------|-----------|-----------|-------------|
| addmprog | inline | mprogs | immediate→convert | script (widevnum), trigger (enum), phrase (string) |
| addquest | inline | quests | immediate→convert | quest (widevnum) |
| addreputation | inline | reputations | immediate→convert | reputation (widevnum), min_rank (string/none), max_rank (string/none), points (int) |

All corresponding del* commands stage removals.

### redit (Room Editor)

| Action | Display | List Name | Currently | Form Fields |
|--------|---------|-----------|-----------|-------------|
| mreset | dialog | resets | immediate→convert | mob (widevnum), max (int), min (int) |
| oreset | dialog | resets | immediate→convert | object (widevnum), target (enum: room/container/mob), target_name (string, conditional), wear_loc (enum, conditional) |
| delreset | inline | resets | **new** | index (int) |
| addrprog | inline | rprogs | immediate→convert | script (widevnum), trigger (enum), phrase (string) |
| addcdesc | inline | cdescs | immediate→convert | condition (enum), phrase (string) |

All corresponding del* commands stage removals.

### aedit (Area Editor)

| Action | Display | List Name | Currently | Form Fields |
|--------|---------|-----------|-----------|-------------|
| addaprog | inline | aprogs | immediate→convert | script (widevnum), trigger (enum), phrase (string) |
| addtrade | inline | trades | immediate→convert | obj_vnum (widevnum), replenish_time (int), replenish_amount (int), max_qty (int), min_price (int), max_price (int) |

All corresponding del* commands (removetrade, delaprog) stage removals.

## Conditional Form Fields

Some actions have conditional fields that depend on other field values.
The `oreset` command is the primary example:

```json
{
  "fields": [
    {"field": "object", "type": "widevnum", "required": true},
    {"field": "target", "type": "enum", "required": true,
     "options": ["room", "container", "mob"]},
    {"field": "target_name", "type": "string", "required": false,
     "visible_when": {"target": ["container", "mob"]}},
    {"field": "wear_loc", "type": "enum", "required": false,
     "options": ["none", "light", "finger_l", "finger_r", "neck_1", "neck_2",
                 "body", "head", "legs", "feet", "hands", "arms",
                 "shield", "about", "waist", "wrist_l", "wrist_r",
                 "wield", "hold", "float", "secondary"],
     "visible_when": {"target": ["mob"]}}
  ]
}
```

The `visible_when` property tells the client to show/hide fields based
on other field values. The client hides fields that don't match and omits
them from the submit payload.

## Action Session Management

The server tracks active action sessions per descriptor to:
- Prevent submitting to an expired or invalid session
- Auto-expire sessions on editor close or entity change
- Limit concurrent action sessions (max 1 per descriptor)

```c
typedef struct olc_action_session {
    char    session_id[32];     // "act_1", "act_2", etc.
    char    action_name[64];    // "addoprog"
    int     editor_type;        // ED_OBJECT
    time_t  created_at;
} olc_action_session_t;
```

Stored on `descriptor_t` (or `protocol_t`). Cleared when:
- Action is submitted (success or error)
- Editor is closed
- New action is requested (replaces previous)

## Schema Updates

After a successful action stage, the server sends `Editor.Schema.Update`
for any tabs whose structure changed. This reuses the existing Phase 4
infrastructure.

Specifically (sent after staging, not after commit):
- After addtype/removetype staging: refresh Type tab
- After addoprog/deloprog staging: refresh Scripts tab
- After addaffect/delaffect staging: refresh Affects tab
- After mreset/oreset/delreset staging: refresh Resets tab
- Similar for all list operations

## GMCP Reference Doc Updates

The GMCP_WEB_CLIENT_REFERENCE.md needs:
- New section for Editor.Action, Editor.Action.Form, Editor.Action.Submit, Editor.Action.Result
- TOC entries for all four messages
- Package summary table entries
- Updated lifecycle section mentioning action flow
- Conditional field documentation (visible_when)
- Updated Editor Client Implementation Guide with action form rendering guidance

## Testing

Each action handler needs:
1. **Form test** — verify form_fn produces valid schema with expected fields and options
2. **Stage test** — verify stage_fn correctly stages the change
3. **Round-trip test** — verify stage → commit → apply creates correct runtime data
4. **Validation test** — verify stage_fn rejects invalid inputs with proper error messages
5. **GMCP integration test** — verify the full message flow works end-to-end

Use the existing `olccs_` test prefix. Test data in `tests/data/unit/`.

## Error Handling

- Unknown action name → `Editor.Action.Result` with `status: "error"`, message: "Unknown action"
- Invalid/expired session_id → `Editor.Action.Result` with error
- Validation failure → `Editor.Action.Result` with specific message (e.g., "Script 5#999 not found")
- Changeset limit reached → `Editor.Action.Result` with `"limit_reached"` error
- Form generation failure → `Editor.Action.Result` with error (defensive)

## File Organization

New files:
- `editors/common/olc_actions.h` — action handler types, registry API, session management
- `editors/common/olc_actions.c` — registry lookup, session management, GMCP dispatch helpers

Action handlers per editor (in existing files):
- `editors/objects/oedit_actions.c` (new) — oedit action form/stage functions + table
- `editors/mobiles/medit_actions.c` (new) — medit action form/stage functions + table
- `editors/rooms/redit_actions.c` (new) — redit action form/stage functions + table
- `editors/areas/aedit_actions.c` (new) — aedit action form/stage functions + table

Modified files:
- `gmcp_editor.c` — add handle_editor_action + handle_editor_action_submit
- `gmcp_editor.h` — declare new message types
- Each editor's existing add/del commands — refactor to call stage_fn
- `CMakeLists.txt` + `Makefile` — add new .c files
- `docs/GMCP_WEB_CLIENT_REFERENCE.md` — document new messages

Test files:
- `tests/unit/olc_action_tests.c` (new) — action form/stage/validation tests
- `tests/data/unit/olc_action_tests.json` (new) — test case definitions
