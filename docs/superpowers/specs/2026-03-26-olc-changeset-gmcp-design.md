# OLC Changeset Framework & GMCP Editor Protocol

**Date:** 2026-03-26
**Status:** Approved Design

## Problem Statement

The game settings editor (`gameedit`) uses a changeset/commit model where changes are
staged, reviewed, and committed atomically with full history. All other OLC editors (31 total)
apply changes immediately to in-memory entities, with no staging, commit semantics, or
rollback capability.

With the web client and rich GMCP infrastructure now in place, we want to:

1. Extend the changeset/commit pattern to core world-building editors (aedit, redit, medit, oedit)
2. Expose editor state over GMCP for web client UIs
3. Enable bidirectional editing — web client can send edits via GMCP
4. Support non-blocking string editing for WebSocket clients

## Scope

**In scope (Phase 1):**
- Generic changeset framework in `editors/common/`
- Field-level overlay with typed JSON values
- Staged mode for Tier 1 editors: aedit, redit, medit, oedit
- `Sentience.Editor.*` GMCP protocol (bidirectional)
- Non-blocking multiline string editor for WebSocket clients
- Per-entity changesets with optional multi-entity grouping
- Per-user pending changes (each builder has their own)
- Committed changeset audit history (view-only, no rollback)
- Draft persistence (save/restore uncommitted work across sessions)

**Out of scope (future phases):**
- Rollback of committed changesets
- Tier 2/3 editor migration
- Conflict resolution for concurrent edits to the same entity
- Real-time collaborative editing (multi-user live cursors, etc.)

## Architecture

### Core Data Structures

#### `olc_pending_change` — A single field modification

```c
typedef enum {
    OLC_FIELD_STRING,       // char* fields (name, description, etc.)
    OLC_FIELD_INT,          // int fields (level, alignment, etc.)
    OLC_FIELD_INT16,        // int16_t fields
    OLC_FIELD_BOOL,         // bool fields
    OLC_FIELD_FLAGS,        // long bitfield (act flags, room flags, etc.)
    OLC_FIELD_WIDEVNUM,     // WNUM/WNUM_LOAD references
    OLC_FIELD_EXIT,         // EXIT_DATA (direction + destination + lock)
    OLC_FIELD_EMBEDDED,     // Embedded structs (dice, location)
    OLC_FIELD_LIST_ADD,     // Add item to a linked list
    OLC_FIELD_LIST_REMOVE,  // Remove item from a linked list
    OLC_FIELD_LIST_UPDATE,  // Update item in a linked list
    OLC_FIELD_MULTILINE,    // Multi-line string (description, etc.)
    OLC_FIELD_TYPE_DATA,    // Polymorphic type-specific data (item types)
} olc_field_type_t;

typedef struct olc_pending_change {
    char *field_path;           // e.g., "name", "exits/north/destination"
    olc_field_type_t field_type;
    json_t *old_value;          // Jansson JSON — can hold any type
    json_t *new_value;          // Jansson JSON — can hold any type

    // Type-specific apply callback: writes new_value to live entity
    bool (*apply_fn)(void *entity, struct olc_pending_change *change);
} olc_pending_change_t;
```

#### `olc_changeset` — Per-entity pending changes for one builder

```c
typedef struct olc_changeset {
    int editor_type;            // ED_ROOM, ED_MOBILE, etc.
    long entity_id;             // vnum or uid
    char *entity_label;         // Human-readable label (e.g., "Room 3001: A Dark Cave")
    char *author;               // Builder character name
    LLIST *changes;             // List of olc_pending_change_t*
    time_t created_at;
    time_t updated_at;
    bool is_dirty;              // Has unsaved changes since last draft
} olc_changeset_t;
```

#### `olc_changeset_group` — Optional multi-entity commit

```c
typedef struct olc_changeset_group {
    int group_id;               // Sequential ID for history
    char *comment;              // Commit message
    char *author;               // Builder who committed
    time_t committed_at;
    LLIST *changesets;          // List of olc_changeset_t* (committed copies)
} olc_changeset_group_t;
```

#### Storage on Descriptor

Each builder's pending changesets are stored on their descriptor:

```c
// In descriptor_data or a new per-character editing state struct:
typedef struct olc_edit_state {
    LLIST *active_changesets;    // List of olc_changeset_t* (one per open entity)
    LLIST *string_edit_sessions; // List of active non-blocking string edits
    int next_string_session_id;
} olc_edit_state_t;
```

### Change Modes (extended enum)

```c
typedef enum olc_change_mode {
    OLC_CHANGE_NONE = 0,
    OLC_CHANGE_IMMEDIATE,
    OLC_CHANGE_AREA_FLAG,
    OLC_CHANGE_EXPLICIT_SAVE,
    OLC_CHANGE_CUSTOM,
    OLC_CHANGE_STAGED,          // NEW: Field-level overlay with commit semantics
} olc_change_mode_t;
```

### `olc_cmd_*` Helper Modifications

When `editor_def->change_mode == OLC_CHANGE_STAGED`, the helpers follow this flow:

1. **Read current value** from the entity's field pointer (for `old_value`)
2. **Validate** the new value (range checks, flag lookups — unchanged)
3. **Store** an `olc_pending_change` in the active changeset instead of writing to `field_ptr`
4. **Call `record_fn`** for audit history (unchanged)
5. **Set `SENTIENCE_DIRTY_EDITOR`** on the descriptor
6. **Show feedback** with staged indicator: `[STAGED] Name set to: A Dark Tavern`

If a pending change already exists for the same `field_path`, it is updated in place
(the `old_value` is preserved from the original, `new_value` is updated). If the new
value equals the original `old_value`, the pending change is removed (no-op detection).

### Preview Helpers

```c
// Returns staged value if pending, otherwise live value
const char *olc_staged_string(olc_changeset_t *cs, const char *field, const char *live);
int         olc_staged_int(olc_changeset_t *cs, const char *field, int live);
long        olc_staged_flags(olc_changeset_t *cs, const char *field, long live);
bool        olc_staged_bool(olc_changeset_t *cs, const char *field, bool live);
json_t     *olc_staged_json(olc_changeset_t *cs, const char *field);

// Check if a field has a pending change
bool olc_is_field_staged(olc_changeset_t *cs, const char *field);
```

Tab show functions use these to render preview state. Fields with pending changes
get a visual marker (e.g., `{Y*{x` prefix or `{Y→{x` arrow).

### Complex Field Handlers

Fields that can't be represented as simple scalars register type-specific handlers:

```c
typedef struct olc_field_handler {
    const char *field_path;         // e.g., "exits/north"
    olc_field_type_t type;

    // Serialize current live value to JSON
    json_t *(*serialize_fn)(void *entity, const char *field_path);

    // Apply a pending change to the live entity
    bool (*apply_fn)(void *entity, olc_pending_change_t *change);

    // Generate display string for preview
    const char *(*display_fn)(json_t *value);
} olc_field_handler_t;
```

Each editor registers its complex field handlers in the `OLC_EDITOR_DEF`:

```c
static const olc_field_handler_t redit_field_handlers[] = {
    { "exits/*",      OLC_FIELD_EXIT,     redit_serialize_exit,  redit_apply_exit,  redit_display_exit },
    { "extra_descr/*", OLC_FIELD_LIST_UPDATE, redit_serialize_ed, redit_apply_ed,   redit_display_ed },
    { "resets/*",     OLC_FIELD_LIST_UPDATE, redit_serialize_reset, redit_apply_reset, redit_display_reset },
    { NULL, 0, NULL, NULL, NULL }
};
```

### Commit Flow

1. Builder runs `commit [comment]` or sends `Sentience.Editor.Commit` via GMCP
2. System validates all pending changes (field handlers can reject)
3. Each `olc_pending_change.apply_fn` writes to the live entity
4. A **committed changeset record** is created and appended to the entity's
   `olc_change_history` (existing audit infrastructure, extended with group_id)
5. Entity is saved to disk (area JSON save or entity-specific save)
6. Pending changeset is cleared
7. `Sentience.Editor.Close` sent via GMCP with reason `"committed"`

### Group Commit Flow

1. Builder has pending changes on room 3001, mob 3005, object 3010
2. `commit group Updated tavern area` or GMCP `Sentience.Editor.Commit` with `group` array
3. All changesets validated first — if any fail, none are committed
4. All applied atomically, each entity saved
5. A `olc_changeset_group` record ties the commits together
6. Each entity's history entry includes the `group_id`

### Revert

- `revert` — Discard all pending changes for the current entity
- `revert <field>` — Discard a single field's pending change
- Both clear the relevant entries from the changeset and send GMCP updates

### Draft Persistence

```
Location: data/drafts/<author>/<editor_type>_<entity_id>.json
```

- `savedraft` serializes the pending changeset to JSON
- On editor open, if a draft exists, prompt: `"You have a saved draft with N changes. Restore? (y/n)"`
- `discardraft` deletes the file
- Drafts are automatically deleted on successful commit
- Draft format is the same JSON used for changeset serialization

## GMCP Protocol: `Sentience.Editor.*`

### Support Negotiation

Clients advertise support via `Core.Supports.Set ["Sentience 1"]` (existing).
Editor messages are part of the Sentience package — no separate negotiation needed.

### Server → Client Messages

#### `Sentience.Editor.Open`

Sent when a builder opens an entity for editing. Includes the full field schema so
the web client can render a dynamic editor form.

```json
{
  "editor_type": "room",
  "entity_id": "room:3001",
  "entity_name": "A Dark Cave",
  "tabs": [
    {
      "name": "General",
      "short": "Gen",
      "fields": [
        {"field": "name", "type": "string", "value": "A Dark Cave", "label": "Name"},
        {"field": "description", "type": "multiline_string", "value": "The cave is...", "label": "Description", "max_length": 4096},
        {"field": "sector", "type": "enum", "value": "cave", "label": "Sector", "options": ["city", "field", "forest", "cave", "water", "air"]},
        {"field": "room_flags", "type": "flags", "value": ["dark", "indoors"], "label": "Room Flags", "options": ["dark", "indoors", "no_mob", "safe", "pet_shop"]},
        {"field": "heal_rate", "type": "int", "value": 100, "label": "Heal Rate", "min": 0, "max": 1000},
        {"field": "mana_rate", "type": "int", "value": 100, "label": "Mana Rate", "min": 0, "max": 1000}
      ]
    },
    {
      "name": "Exits",
      "short": "Exit",
      "fields": [
        {"field": "exits/north", "type": "exit", "value": {"destination": "1#3002", "desc": "...", "lock": {}}, "label": "North"},
        {"field": "exits/south", "type": "exit", "value": null, "label": "South"}
      ]
    }
  ],
  "pending_changes": [],
  "has_draft": false,
  "_v": 1
}
```

#### `Sentience.Editor.State`

Sent when pending changes are updated (field set, reverted, etc.).

```json
{
  "entity_id": "room:3001",
  "pending_count": 2,
  "changes": [
    {"field": "name", "old_value": "A Dark Cave", "new_value": "A Glowing Cavern", "type": "string"},
    {"field": "heal_rate", "old_value": 100, "new_value": 200, "type": "int"}
  ],
  "_v": 1
}
```

#### `Sentience.Editor.Field`

Sent for individual field updates (e.g., after a single `Sentience.Editor.Set`).

```json
{
  "entity_id": "room:3001",
  "field": "name",
  "value": "A Glowing Cavern",
  "type": "string",
  "is_pending": true,
  "_v": 1
}
```

#### `Sentience.Editor.Close`

```json
{
  "entity_id": "room:3001",
  "reason": "committed",
  "_v": 1
}
```

#### `Sentience.Editor.Error`

```json
{
  "entity_id": "room:3001",
  "field": "level",
  "error": "out_of_range",
  "message": "Level must be between 1 and 200.",
  "_v": 1
}
```

### Client → Server Messages

#### `Sentience.Editor.Set`

```json
{"entity_id": "room:3001", "field": "name", "value": "A Glowing Cavern"}
```

Server validates, stores in overlay, responds with `Editor.Field` or `Editor.Error`.

#### `Sentience.Editor.Commit`

Single entity:
```json
{"entity_id": "room:3001", "comment": "Updated room name and description"}
```

Multi-entity group:
```json
{
  "group": ["room:3001", "mob:3005", "obj:3010"],
  "comment": "Rebuilt tavern area"
}
```

#### `Sentience.Editor.Revert`

All pending:
```json
{"entity_id": "room:3001"}
```

Single field:
```json
{"entity_id": "room:3001", "field": "name"}
```

#### `Sentience.Editor.Request`

Request entity data (e.g., on reconnect or tab switch):
```json
{"entity_id": "room:3001", "tab": "General"}
```

Request history:
```json
{"entity_id": "room:3001", "type": "history", "limit": 20}
```

### Incoming Message Handling

Incoming `Sentience.Editor.*` messages are parsed using **Jansson** (`json_loads()`),
not jsmn, due to the complexity of the payloads. This follows the precedent set by
`Sentience.Client.Layout` in `gmcp_sentience.c`.

New handler registration in `GMCPReceiveTable` (protocol.c):

```c
{ GMCP_SENTIENCE_EDITOR,  "Sentience.Editor" }
```

The handler extracts the sub-message from the package name (e.g., `Set`, `Commit`,
`Revert`, `Request`) and dispatches to specific functions:

```c
void sentience_handle_editor(descriptor_t *d, const char *sub_msg, const char *json_str);
```

All incoming editor messages require:
- Character is online and not NPC
- Character has appropriate OLC permissions (IS_BUILDER or editor-specific checks)
- Character has the referenced entity open in an editor

## Non-Blocking String Editor

### Problem

`string_append()` puts the descriptor into modal state (`d->pString != NULL`).
All input is captured by the string editor until `@` is entered. This blocks
all other activity for the player.

### Solution

For WebSocket/GMCP clients, bypass the modal string editor entirely:

#### Detection

In `olc_cmd_string_append()`:

```c
if (is_websocket_connection(ch->desc) && ch->desc->pProtocol->bGMCP) {
    // Send GMCP message, don't enter modal state
    sentience_editor_string_open(ch->desc, entity_id, field, *field_ptr, max_length);
    return true;
}
// else: fall through to existing string_append() for telnet
```

#### GMCP Messages

**`Sentience.Editor.StringEdit.Open`** (server → client):

```json
{
  "entity_id": "room:3001",
  "field": "description",
  "current_value": "The room is dark and musty...",
  "max_length": 4096,
  "session_id": "se_1"
}
```

**`Sentience.Editor.StringEdit.Save`** (client → server):

```json
{
  "session_id": "se_1",
  "value": "The room glows with an ethereal light..."
}
```

**`Sentience.Editor.StringEdit.Cancel`** (client → server):

```json
{
  "session_id": "se_1"
}
```

**`Sentience.Editor.StringEdit.Close`** (server → client):

```json
{
  "session_id": "se_1",
  "status": "saved"
}
```

#### Session Tracking

Each open string edit is tracked in the builder's `olc_edit_state`:

```c
typedef struct olc_string_edit_session {
    int session_id;             // "se_1", "se_2", etc.
    char *entity_id;            // Which entity this belongs to
    char *field_path;           // Which field
    char **field_ptr;           // Direct pointer (for non-staged mode)
    olc_changeset_t *changeset; // For staged mode
} olc_string_edit_session_t;
```

Multiple string edits can be open simultaneously (e.g., room description + extra
description), since each has its own session_id and panel in the web client.

For telnet clients, the existing `string_append()` modal behavior is unchanged.

## New Editor Commands

All editors with `OLC_CHANGE_STAGED` gain these commands automatically via the framework:

| Command | Description |
|---------|-------------|
| `commit [comment]` | Apply all pending changes to live entity and save |
| `commit group [comment]` | Commit all open staged editors together |
| `revert` | Discard all pending changes |
| `revert <field>` | Discard a single field's pending change |
| `pending` | Show table of pending changes (field, old → new) |
| `savedraft` | Persist pending changes to disk |
| `loaddraft` | Restore previously saved draft |
| `discardraft` | Delete saved draft |
| `done` | If pending changes exist, warn; otherwise exit editor |

These are registered in the framework command table and available to all staged editors
without per-editor implementation.

## File Organization

### New Files

```
editors/common/olc_changeset.h      # Changeset types, API declarations
editors/common/olc_changeset.c      # Changeset lifecycle, commit, revert, draft I/O
editors/common/olc_staged.h         # Staged olc_cmd_* helpers, preview getters
editors/common/olc_staged.c         # Staged mode integration with olc_cmd_* flow
editors/common/olc_field_handlers.h # Field handler registration, complex field API
editors/common/olc_field_handlers.c # Generic field handler dispatch
gmcp_editor.h                       # GMCP Editor protocol types
gmcp_editor.c                       # GMCP Editor message handlers (in/out)
```

### Modified Files

```
editors/common/olc_editor.h         # Add OLC_CHANGE_STAGED, staged_config, edit_state
editors/common/olc_editor.c         # Integrate staged mode into editor lifecycle
editors/common/olc_commands.c       # Add staging path to olc_cmd_* helpers
editors/common/olc_display.c        # Add pending-change visual markers
editors/areas/redit.c               # Opt in: change_mode = OLC_CHANGE_STAGED
editors/areas/medit.c               # Opt in: change_mode = OLC_CHANGE_STAGED
editors/areas/oedit.c               # Opt in: change_mode = OLC_CHANGE_STAGED
editors/areas/aedit.c               # Opt in: change_mode = OLC_CHANGE_STAGED
gmcp_sentience.h                    # Add SENTIENCE_DIRTY_EDITOR flag
gmcp_sentience.c                    # Add editor dirty tracking to update cycle
protocol.h                          # Add GMCP_SENTIENCE_EDITOR to receive enum
protocol.c                          # Add dispatch entry for Sentience.Editor.*
merc.h                              # Add olc_edit_state to descriptor/pcdata
CMakeLists.txt                      # Add new source files
Makefile                            # Add new source files (keep synchronized)
```

## Entity ID Format

Entities are identified in GMCP messages using a `type:id` string format:

```
"room:3001"     — Room with vnum 3001
"mob:3005"      — Mobile with vnum 3005
"obj:3010"      — Object with vnum 3010
"area:10"       — Area with uid 10
```

This allows the protocol to be entity-type agnostic while remaining human-readable.

## Security

All incoming GMCP editor messages require:

1. **Authentication**: `d->character != NULL && !IS_NPC(d->character)`
2. **Authorization**: Builder permissions checked via the editor's `OLC_EDITOR_PERM` (same
   as command-line editing — `IS_BUILDER`, area security level, staff rank, etc.)
3. **Context validation**: The referenced entity must be currently open in the builder's editor
4. **Input validation**: Field values validated by the same logic as command-line input
   (range checks, flag table lookups, string length limits)

No new privilege escalation paths are introduced — GMCP editing goes through the same
validation as typed commands.

## Testing Strategy

- Unit tests for changeset CRUD (create, add change, remove change, clear)
- Unit tests for overlay read (staged value vs live value)
- Unit tests for commit flow (apply changes, clear pending)
- Unit tests for draft serialization/deserialization
- Unit tests for GMCP message building (JSON output format)
- Integration tests for full edit→commit cycle via commands
- Integration tests for GMCP round-trip (Set → Field response)
- Integration tests for group commit across multiple entities
- Integration tests for revert (all and single-field)

## Migration Path

1. Build the generic framework (olc_changeset, olc_staged, gmcp_editor)
2. Migrate redit first (simplest Tier 1 editor, well-understood fields)
3. Migrate medit, oedit, aedit
4. Each migration is a single-flag change in the editor def + adding field handlers for complex fields
5. Future: Tier 2/3 editors opt in as needed
