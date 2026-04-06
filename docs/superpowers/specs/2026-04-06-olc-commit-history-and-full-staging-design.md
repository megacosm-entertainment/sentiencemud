# OLC Commit History & Full Field Staging

## Problem

The OLC staged changeset system can stage field changes and commit them, but:
1. **No commit history** — once committed, changes are gone. No way to review what was changed or revert mistakes.
2. **Partial staging coverage** — only ~30% of data-modifying commands in the 4 staged editors (oedit, redit, medit, aedit) use the staging system. The rest directly mutate entities, bypassing changesets entirely.
3. **All editors are migrating to staged mode** to support GMCP editing, so the infrastructure must be generic and complete.

Gameedit has its own independent history/rollback system. The goal is to bring the unified OLC framework to feature parity so gameedit can eventually migrate in.

## Approach

Category-based phasing: build infrastructure by capability (history → scalars → list ops → type-specific → exits), where each phase delivers working, testable value before the next begins.

## Design

### 1. Commit History Data Model

#### Core Structures (in `editors/common/olc_commit_history.h`)

```c
typedef struct olc_committed_change {
    char            *field_path;      /* Field identifier (e.g., "Condition", "armor/pierce") */
    olc_field_type_t field_type;
    json_t          *old_value;       /* JSON — enables programmatic rollback */
    json_t          *new_value;
} olc_committed_change_t;

typedef struct olc_commit_record {
    int              id;              /* Sequential per-entity */
    int              group_id;        /* Shared across entities in group commit; 0 if solo */
    char            *author;
    char            *comment;         /* Optional commit message */
    time_t           timestamp;
    LLIST           *changes;         /* List of olc_committed_change_t */
} olc_commit_record_t;

typedef struct olc_commit_history {
    int              editor_type;     /* ED_* constant */
    WNUM_LOAD        entity_wnum;     /* Entity identifier */
    LLIST           *records;         /* List of olc_commit_record_t, newest first */
    int              next_id;
    int              max_records;     /* Configurable via game settings, default 20 */
    bool             is_dirty;        /* Needs disk save */
} olc_commit_history_t;
```

Key decisions:
- **JSON old/new values** (not display strings) so rollback can apply values directly via field handlers.
- **Per-entity history** capped at a configurable limit (game setting, default 20). Oldest records evicted on overflow.
- **group_id** links records across entities committed together. A global counter provides unique group IDs.
- **Replaces** the existing `OLC_CHANGE_HISTORY` / `OLC_CHANGE_ENTRY` field-level system across all `OLC_EDITOR_DEF` editors.

### 2. History Commands

Registered in the staged command dispatch (`olc_editor.c`), available in any `OLC_CHANGE_STAGED` editor:

| Command | Description |
|---------|-------------|
| `history` | Table of past commits: ID, author, # changes, date, comment |
| `history <id>` | Detailed view: field-by-field old→new for a specific commit |
| `history revert <id>` | Preview what reverting would change + require confirmation |
| `history revert <id> confirm` | Apply old values from the commit via field handlers. Creates a new forward commit record documenting the rollback. |
| `history revert <id> <field>` | Revert a single field from a commit |

Design principles:
- **Forward-only history** — reverts create new commit records, never delete history entries. Always auditable.
- **Confirmation required** — rollback shows a preview first, requires explicit `confirm` argument (same pattern as gameedit).
- **Rollback via field handlers** — revert swaps old/new values in each change, then applies through `olc_changeset_commit()`. This reuses all existing apply logic.

### 3. History Persistence

**Source of truth:** Disk files at `{DATA_DIR}history/{editor_type}/{auid}_{vnum}.json`
- Example: `data/history/object/5_100.json` for object vnum 100 in area UID 5.

**Cache layer:** Redis, keyed as `olc:history:{editor_type}:{auid}:{vnum}`.
- On commit: write to Redis (async thread), mark dirty for disk save.
- On history read: check Redis first; on miss, load from disk, populate cache.
- Uses existing Redis read/write caching infrastructure.

**Save triggers:** Area save (`AREA_CHANGED`), shutdown, copyover.

**Load strategy:** Lazy — loaded on first `history` command or first commit for that entity. Not loaded at boot.

**No entity struct changes** — history is fetched on demand through the cache layer, not stored as a member on entity structs.

**JSON format:** Array of commit records using the same serialization conventions as changeset drafts (Jansson library).

### 4. Staging Infrastructure Additions

#### 4a. List Operations

New helpers in `olc_commands.c`:

```c
bool olc_cmd_list_add(CHAR_DATA *ch, const char *label,
                      const char *item_key, json_t *item_json,
                      olc_changeset_t *cs);

bool olc_cmd_list_remove(CHAR_DATA *ch, const char *label,
                         const char *item_key, json_t *item_json,
                         olc_changeset_t *cs);
```

- Stage `OLC_FIELD_LIST_ADD` with `new_value` = item JSON, `old_value` = null.
- Stage `OLC_FIELD_LIST_REMOVE` with `old_value` = item JSON, `new_value` = null.
- Field path uses `"Collection/identifier"` format: `"Affects/+3 hitroll"`, `"Spells/fireball"`.
- Apply handlers are **custom per list type** — each editor registers handlers that know how to deserialize the JSON and add/remove from the actual data structures.
- Each list type needs a serializer (affect → JSON, spell assignment → JSON, etc.) implemented alongside its apply handler.

Covers: addaffect/delaffect, addspell/delspell, addquest/delquest, addcatalyst/delcatalyst, addimmune/delimmune, addoprog/deloprog, addmprog/delmprog, addskill/delskill, addtype/removetype, addreputation/delreputation, addtrade/removetrade, addaprog/delaprog, addcdesc/delcdesc, etc.

#### 4b. Type-Specific Data Staging

For oedit's ~30 type subcommands (armor, weapon, drink, food, etc.), each modifying fields within type-specific structs (ARMOR_DATA, WEAPON_DATA, etc.):

- Stage individual type fields with hierarchical field paths: `"armor/pierce"`, `"weapon/dice_number"`, `"food/hunger"`.
- New helper: `olc_cmd_type_field()` taking type name prefix and field details.
- Apply handlers use existing type data pointers (e.g., `pObj->_armor->pierce`).
- `addtype`/`removetype` staged as list operations on the type bitset.

This keeps type field changes granular (visible in pending, revertible individually) rather than staging entire type structs as opaque blobs.

#### 4c. Exit Handling

For redit's 10 direction handlers:

- New helper: `olc_cmd_exit()` capturing before/after exit state as JSON.
- Exit JSON includes: direction, to_room wnum, key vnum, exit flags, keyword, description.
- Uses existing `OLC_FIELD_EXIT` enum value.
- Apply handler reconstructs exit from JSON.
- Bidirectional linking handled in the apply function (same logic as current `change_exit()`).

### 5. Migration from OLC_CHANGE_HISTORY

The existing `OLC_CHANGE_HISTORY` / `OLC_CHANGE_ENTRY` system (used by clsedit, racedit, skedit, soedit, gredit, traitedit, rsgedit) is replaced:

- The `get_history_fn` callback in `OLC_EDITOR_DEF` is repurposed to return the entity's commit history (fetched from cache/disk).
- Existing `olc_history_record()`, `olc_history_show()`, `olc_history_view()` are replaced by the new commit-level equivalents.
- Non-staged editors that currently use `OLC_CHANGE_HISTORY` will be migrated as they move to `OLC_CHANGE_STAGED` mode.
- The old structs and functions are removed once all editors have migrated.

### 6. Commands That Are NOT Staged

These command categories don't modify entity data and are excluded from staging:

- **Navigation:** `prev`, `next`, `show`, `commands`, `?`
- **Creation:** `create` (creates new entities — not a field change)
- **Resets:** `mreset`, `oreset` (modify area reset lists, not entity fields)
- **Variables:** `varset`, `varclear` (script variable operations — separate system)

## Implementation Phases

### Phase 1: Commit History + Remaining Scalars
- `olc_commit_history.h/.c` — data model, create/archive/lookup/serialize/deserialize
- History commands: `history`, `history <id>`, `history revert`
- Redis cache integration for history reads/writes
- Disk persistence in `{DATA_DIR}history/`
- Hook into `olc_staged_cmd_commit()` and `olc_staged_cmd_commit_group()` to archive on commit
- Convert ~20 remaining simple scalar commands to `olc_cmd_*`
- Add field handlers for all newly converted scalars
- Game setting for max history entries per entity
- Tests for history create/archive/serialize/revert

### Phase 2: List Operations
- `olc_cmd_list_add()` / `olc_cmd_list_remove()` helpers
- Per-list-type JSON serializers (affect, spell, quest, catalyst, immune, oprog, mprog, etc.)
- Per-list-type apply handlers in each editor's field handler table
- Convert ~30 add/del commands across all 4 editors
- Tests for list staging, commit, and revert

### Phase 3: Type-Specific Subcommands
- `olc_cmd_type_field()` helper with hierarchical paths
- Convert ~30 oedit type subcommands (armor, weapon, drink, food, furniture, etc.)
- Stage addtype/removetype as list operations
- Apply handlers for each type's fields
- Tests for type-specific staging and revert

### Phase 4: Exit Handling + Complex Operations
- `olc_cmd_exit()` helper with full exit state capture
- Convert 10 redit direction handlers
- Handle remaining complex commands (shop, trainer, coordinates, etc.)
- Tests for exit staging and bidirectional link handling

## Files Affected

### New Files
- `editors/common/olc_commit_history.h` — Commit history data model and API
- `editors/common/olc_commit_history.c` — Implementation
- `tests/unit/olc_commit_history_tests.c` — History unit tests
- `tests/data/unit/olc_commit_history_tests.json` — History test definitions

### Modified Files (per phase)
- `editors/common/olc_commands.h/.c` — New staging helpers (list, type, exit)
- `editors/common/olc_field_handlers.h/.c` — New apply handler macros/generics
- `editors/common/olc_staged.c` — History archival on commit, history commands
- `editors/common/olc_editor.h/.c` — Updated get_history_fn, remove old history
- `editors/objects/oedit.c` — All oedit command conversions + handlers
- `editors/rooms/redit.c` — All redit command conversions + handlers
- `editors/mobiles/medit.c` — All medit command conversions + handlers
- `editors/areas/aedit.c` — All aedit command conversions + handlers
- `CMakeLists.txt` + `Makefile` — New source files
- `tests/framework/test_dispatcher.c` + `test_modules.h` — New test registration

## Testing Strategy

Each phase includes:
- Unit tests for new infrastructure (history model, list staging helpers, etc.)
- Integration tests verifying commit→history→revert roundtrips
- Regression tests ensuring existing 31 olccs_ tests still pass
- Full test suite verification (595 tests baseline)

## Revert Semantics by Field Type

| Field Type | Revert Action |
|-----------|---------------|
| STRING, INT, INT16, BOOL, FLAGS, MULTIFLAGS, MULTILINE | Apply `old_value` via field handler (swap old/new, commit) |
| LIST_ADD | Remove the added item (apply handler must support removal by JSON match) |
| LIST_REMOVE | Re-add the removed item (apply handler must support addition from JSON) |
| TYPE_DATA | Apply `old_value` for the type field (same as scalar within type struct) |
| EXIT | Restore previous exit state from `old_value` JSON (including bidirectional unlink/relink) |

List and exit revert handlers are implemented in their respective phases (2 and 4). Phase 1 history revert only supports scalar field types; attempting to revert a commit containing non-scalar changes in Phase 1 will warn the user and skip those fields.

## Open Questions / Future Work

- **Cross-editor group commit** — Currently deferred. `commit group` only affects the current editor type. Enabling cross-editor commits requires entity lookup functions (given wnum, find live entity pointer). Designed for but not implemented in this work.
- **Gameedit migration** — Once this framework reaches parity, gameedit can migrate from its custom system to `OLC_EDITOR_DEF` + `OLC_CHANGE_STAGED`.
- **GMCP history integration** — History data could be exposed via GMCP for web client display.
