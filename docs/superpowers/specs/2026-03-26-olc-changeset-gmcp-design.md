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
} olc_pending_change_t;
```

**Apply dispatch:** The `olc_pending_change` struct is data-only. Application is
always dispatched through `olc_field_handler_t` lookups. Simple scalar fields
(string, int, bool, flags) use generic framework-provided handlers registered
automatically for each `olc_field_type_t`. Complex fields (exits, lists, embedded
structs) use editor-specific handlers registered in the `OLC_EDITOR_DEF`.

#### `olc_changeset` — Per-entity pending changes for one builder

```c
typedef struct olc_changeset {
    int editor_type;            // ED_ROOM, ED_MOBILE, etc.
    WNUM_LOAD entity_wnum;      // Area UID + vnum (persistent identifier)
    char *entity_label;         // Human-readable label (e.g., "Room 5#3001: A Dark Cave")
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

The existing enum in `olc_editor.h` is extended by appending the new value.
Existing values and their implicit integer assignments are preserved.

```c
typedef enum {
    OLC_CHANGE_AREA_FLAG,       // 0 — existing
    OLC_CHANGE_EXPLICIT_SAVE,   // 1 — existing
    OLC_CHANGE_CUSTOM,          // 2 — existing
    OLC_CHANGE_NONE,            // 3 — existing
    OLC_CHANGE_STAGED,          // 4 — NEW: Field-level overlay with commit semantics
} olc_change_mode_t;
```

### `olc_cmd_*` Helper Modifications

When `editor_def->change_mode == OLC_CHANGE_STAGED`, the helpers follow this flow:

1. **Read current value** from the entity's field pointer (for `old_value`)
2. **Validate** the new value (range checks, flag lookups — unchanged)
3. **Store** an `olc_pending_change` in the active changeset instead of writing to `field_ptr`
4. **Call `record_fn`** for audit history (unchanged)
5. **Send GMCP response immediately** (not via dirty-flag polling — editor updates
   are event-driven, sent directly from the command handler for zero latency)
6. **Show feedback** with staged indicator: `[STAGED] Name set to: A Dark Tavern`

**Note on GMCP delivery:** Unlike `Sentience.Char.*` packages which use dirty-flag
polling in `sentience_gmcp_update()`, editor messages are sent **immediately** from
within the command handler or GMCP message handler. This avoids unnecessary latency
from waiting for the next update tick. A `SENTIENCE_DIRTY_EDITOR` flag is still used
for the prompt indicator (showing that pending changes exist) but does not drive
GMCP message dispatch.

If a pending change already exists for the same `field_path`, it is updated in place
(the `old_value` is preserved from the original, `new_value` is updated). If the new
value equals the original `old_value`, the pending change is removed (no-op detection).

### List Operation Semantics

List fields (extra descriptions, resets, affects) require special overlay handling:

**Field path keying:** List items use keyword-based paths when items have unique
identifiers (e.g., `extra_descr/statue`, `extra_descr/fountain`). For lists without
unique keys (resets, affects), use index-based paths (e.g., `resets/0`, `resets/1`).
Index-based paths reference the item's position in the **original** live list at the
time editing began — not the position after applying other pending changes.

**Operation collapsing rules:**
- `LIST_ADD` then `LIST_REMOVE` for the same item → collapse to no-op, purge both
- `LIST_ADD` then `LIST_UPDATE` for the same item → collapse to `LIST_ADD` with updated value
- `LIST_REMOVE` then `LIST_ADD` for the same key → collapse to `LIST_UPDATE`
- `LIST_UPDATE` then `LIST_UPDATE` → keep original `old_value`, update `new_value`
- `LIST_UPDATE` then `LIST_REMOVE` → collapse to `LIST_REMOVE` with original's `old_value`

**old_value for list operations:**
- `LIST_ADD`: `old_value` is `null` (item didn't exist)
- `LIST_REMOVE`: `old_value` is the complete serialized item being removed
- `LIST_UPDATE`: `old_value` is the item's state before any pending changes

**Commit ordering:** List operations are applied in insertion order (the order they
were added to the changeset). This matters for remove-then-add-back scenarios where
order determines the final state.

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
3. Each pending change is applied via its `olc_field_handler_t.apply_fn` (looked up by `field_path`)
4. A **committed changeset record** is created and appended to the entity's
   `olc_change_history` (existing audit infrastructure, extended with group_id)
5. Entity is saved to disk (area JSON save or entity-specific save)
6. Pending changeset is cleared
7. `Sentience.Editor.CommitResult` sent via GMCP:
   ```json
   {"entity_id": "room:5#3001", "status": "success", "changes_applied": 2, "_v": 1}
   ```
   Note: Commit does **not** close the editor — the builder can continue editing.
   `Sentience.Editor.Close` is only sent when the builder exits via `done`.

### Group Commit Flow

1. Builder has pending changes on room 3001, mob 3005, object 3010
2. `commit group Updated tavern area` or GMCP `Sentience.Editor.Commit` with `group` array
3. All changesets validated first — if any fail validation, none are committed
4. All in-memory changes applied first (all entities updated in memory)
5. Then all entities saved to disk sequentially
6. **Partial failure handling:** If entity N's disk save fails after entities 1..N-1
   succeeded, the error is logged, the builder is told which entities succeeded and
   which failed, and the failed entity's changes remain as "pending" so the builder
   can retry. In-memory state is consistent (all changes applied) even if disk
   persistence is partial.
7. A `olc_changeset_group` record ties the commits together
8. Each entity's history entry includes the `group_id`
9. `Sentience.Editor.CommitResult` sent with per-entity results:
   ```json
   {
     "group_id": 42,
     "results": [
       {"entity_id": "room:5#3001", "status": "success", "changes_applied": 3},
       {"entity_id": "mob:5#3005", "status": "success", "changes_applied": 1},
       {"entity_id": "obj:5#3010", "status": "save_failed", "changes_applied": 2,
        "error": "Disk write failed for area 5"}
     ],
     "comment": "Rebuilt tavern area",
     "_v": 1
   }
   ```

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
- **Auto-draft on disconnect:** When a descriptor is freed with non-empty pending
  changesets, all are automatically saved as drafts. This prevents data loss from
  link-dead disconnects, crashes, or accidental quit. The existing draft restore
  prompt on editor-open handles the reconnect case seamlessly.
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
  "entity_id": "room:5#3001",
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

Sent for bulk state changes (not individual field edits). Triggered by:
- `revert` (all fields cleared)
- `loaddraft` / draft auto-restore (bulk restore of pending changes)
- `Sentience.Editor.Request` response (client requests full state)
- After commit (pending list is now empty)

```json
{
  "entity_id": "room:5#3001",
  "pending_count": 2,
  "changes": [
    {"field": "name", "old_value": "A Dark Cave", "new_value": "A Glowing Cavern", "type": "string"},
    {"field": "heal_rate", "old_value": 100, "new_value": 200, "type": "int"}
  ],
  "draft_restored": false,
  "_v": 1
}
```

#### `Sentience.Editor.Field`

Sent for individual field updates. Triggered by:
- A single `Sentience.Editor.Set` from the client
- A MUD command that modifies one field (e.g., `redit name A Glowing Cavern`)
- A single-field `revert <field>`

```json
{
  "entity_id": "room:5#3001",
  "field": "name",
  "value": "A Glowing Cavern",
  "type": "string",
  "is_pending": true,
  "_v": 1
}
```

#### `Sentience.Editor.Close`

Sent only when the editor is actually exited (not on commit).

Valid `reason` values: `"done"` (builder exited normally), `"forced"` (admin override
or entity deleted), `"disconnect"` (session lost, after auto-draft saved).

```json
{
  "entity_id": "room:5#3001",
  "reason": "done",
  "_v": 1
}
```

#### `Sentience.Editor.Error`

```json
{
  "entity_id": "room:5#3001",
  "field": "level",
  "error": "out_of_range",
  "message": "Level must be between 1 and 200.",
  "_v": 1
}
```

### Client → Server Messages

#### `Sentience.Editor.Set`

```json
{"entity_id": "room:5#3001", "field": "name", "value": "A Glowing Cavern"}
```

Server validates, stores in overlay, responds with `Editor.Field` or `Editor.Error`.

#### `Sentience.Editor.Commit`

Single entity:
```json
{"entity_id": "room:5#3001", "comment": "Updated room name and description"}
```

Multi-entity group:
```json
{
  "group": ["room:5#3001", "mob:5#3005", "obj:5#3010"],
  "comment": "Rebuilt tavern area"
}
```

#### `Sentience.Editor.Revert`

All pending:
```json
{"entity_id": "room:5#3001"}
```

Single field:
```json
{"entity_id": "room:5#3001", "field": "name"}
```

#### `Sentience.Editor.Request`

Request entity data (e.g., on reconnect or tab switch):
```json
{"entity_id": "room:5#3001", "tab": "General"}
```

Request history:
```json
{"entity_id": "room:5#3001", "type": "history", "limit": 20}
```

### Incoming Message Handling

Incoming `Sentience.Editor.*` messages are parsed using **Jansson** (`json_loads()`),
not jsmn, due to the complexity of the payloads. This follows the precedent set by
`Sentience.Client.Layout` in `gmcp_sentience.c`.

New handler registration in `GMCPReceiveTable` (protocol.c) — one entry per
sub-message, matching the existing exact-match dispatch pattern:

```c
{ GMCP_SENTIENCE_EDITOR_SET,     "Sentience.Editor.Set"     }
{ GMCP_SENTIENCE_EDITOR_COMMIT,  "Sentience.Editor.Commit"  }
{ GMCP_SENTIENCE_EDITOR_REVERT,  "Sentience.Editor.Revert"  }
{ GMCP_SENTIENCE_EDITOR_REQUEST, "Sentience.Editor.Request"  }
{ GMCP_SENTIENCE_EDITOR_STRING_SAVE,   "Sentience.Editor.StringEdit.Save"   }
{ GMCP_SENTIENCE_EDITOR_STRING_CANCEL, "Sentience.Editor.StringEdit.Cancel" }
{ GMCP_SENTIENCE_EDITOR_DRAFT_SAVE,    "Sentience.Editor.Draft.Save"        }
{ GMCP_SENTIENCE_EDITOR_DRAFT_LOAD,    "Sentience.Editor.Draft.Load"        }
```

Each `case` in `ParseGMCP()` extracts the raw JSON substring from the jsmn token
and passes it to a Jansson-based handler function (following the `Sentience.Client.Layout`
precedent where `json_loads()` parses the full payload):

```c
case GMCP_SENTIENCE_EDITOR_SET:
case GMCP_SENTIENCE_EDITOR_COMMIT:
case GMCP_SENTIENCE_EDITOR_REVERT:
case GMCP_SENTIENCE_EDITOR_REQUEST:
case GMCP_SENTIENCE_EDITOR_STRING_SAVE:
case GMCP_SENTIENCE_EDITOR_STRING_CANCEL:
case GMCP_SENTIENCE_EDITOR_DRAFT_SAVE:
case GMCP_SENTIENCE_EDITOR_DRAFT_LOAD:
    sentience_handle_editor(apDescriptor, GMCPReceiveTable[i].module,
                            string + t[1].start);
    break;
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
  "entity_id": "room:5#3001",
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
    int session_id;             // Integer internally, formatted as "se_N" for GMCP
    char *entity_id;            // Which entity this belongs to
    char *field_path;           // Which field
    char **field_ptr;           // Direct pointer (for non-staged mode)
    olc_changeset_t *changeset; // For staged mode
} olc_string_edit_session_t;
```

The `session_id` is stored as an integer and formatted to string `"se_N"` at
GMCP message build time.

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
gmcp_sentience.h                    # Add SENTIENCE_DIRTY_EDITOR flag (for prompt indicator)
gmcp_sentience.c                    # Add editor prompt indicator to update cycle
protocol.h                          # Add GMCP_SENTIENCE_EDITOR to receive enum
protocol.c                          # Add dispatch entry for Sentience.Editor.*
merc.h                              # Add olc_edit_state to descriptor/pcdata
mem.c                               # Hook auto-draft into free_descriptor() cleanup
CMakeLists.txt                      # Add new source files
Makefile                            # Add new source files (keep synchronized)
```

## Entity ID Format

Entities are identified in GMCP messages using a `type:area_uid#vnum` string format,
matching the existing WNUM_LOAD serialization used throughout the codebase:

```
"room:5#3001"   — Room with vnum 3001 in area uid 5
"mob:5#3005"    — Mobile with vnum 3005 in area uid 5
"obj:5#3010"    — Object with vnum 3010 in area uid 5
"area:10"       — Area with uid 10 (no vnum component)
```

**Parsing:** Split on `:` to get entity type, then parse the remainder as a wnum
string (split on `#` for `area_uid` and `vnum`). Areas use uid-only format since
they are identified by a single value. This prevents ambiguity when multiple areas
contain entities with the same vnum, and aligns with existing `Sentience.Room.Info`
GMCP messages which already use wnum strings.

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
- Unit tests for list operation collapsing (add+remove→noop, add+update→add, etc.)
- Unit tests for draft serialization/deserialization
- Unit tests for draft restore of corrupt/incompatible JSON (graceful failure)
- Unit tests for GMCP message building (JSON output format)
- Integration tests for full edit→commit cycle via commands
- Integration tests for GMCP round-trip (Set → Field response)
- Integration tests for GMCP Set with invalid field / unauthorized entity
- Integration tests for group commit across multiple entities
- Integration tests for group commit with partial validation failure
- Integration tests for revert (all and single-field)
- Integration tests for commit with no pending changes (should be rejected)
- Integration tests for string edit session with disconnected entity
- Integration tests for auto-draft on descriptor free

## Safety Limits

- Maximum 100 pending changes per entity
- Maximum 500 total pending changes across all open entities per builder
- Exceeding limits returns `Editor.Error` with `"error": "too_many_pending"`
- Draft files have a maximum size of 64KB

## Concurrent Editing

Concurrent editing conflict resolution is out of scope for Phase 1, but the system
includes a **stale-commit warning**: when committing, if the entity's
`olc_change_history` shows a commit by another builder since this changeset was
created (`changeset.created_at < latest_history_entry.timestamp`), the builder is
warned: `"Warning: {author} committed changes to this entity since you started editing. Proceed? (y/n)"`.
For GMCP commits, the response includes `"warning": "stale_changeset"` and the
client must send a confirmation.

## Future Enhancements (Out of Scope)

- `Sentience.Editor.OpenRequest` — Client-initiated editor open (for "click room to
  edit" flows in the web client)
- Rollback of committed changesets
- Tier 2/3 editor migration
- Real-time collaborative editing
- Conflict resolution for concurrent edits

## Migration Path

1. Build the generic framework (olc_changeset, olc_staged, gmcp_editor)
2. Migrate redit first (simplest Tier 1 editor, well-understood fields)
3. Migrate medit, oedit, aedit
4. Each migration is a single-flag change in the editor def + adding field handlers for complex fields
5. Future: Tier 2/3 editors opt in as needed
