# OLC Editor Framework Cleanup & Hardening

## Context

The editor framework has been largely migrated -- 17 of 18 active editors use `OLC_EDITOR_DEF`. However, the display layer has critical gaps: tables don't enforce column widths, long strings overflow without truncation or wrapping, and `ctx->value_width` is computed but never used. There's ~30 lines of identical boilerplate repeated across 6-13 editors for history helpers and `get_area` callbacks. `gameedit.c` remains the sole unmigrated editor. The framework lacks documentation for future editor authors.

**Future consideration**: A web-based client will eventually need richer editing interfaces. This plan keeps current changes scoped to the text renderer, but notes where the architecture should be kept decoupled so a future display backend (e.g., JSON/structured output for web) can be introduced without rewriting editor logic. The key principle: editors describe *what* to display (fields, tables, sections), and the display layer decides *how* to render it for the current client.

---

## Phase 1: Table & Display Hardening

**Goal**: Tables and field displays enforce column widths, truncate/wrap long strings, and remain readable regardless of content length.

### 1A. Color-aware string utilities

Add to `editors/common/olc_display.c` (declare in `olc_display.h`):

**`olc_trunc(dest, dest_size, src, max_visible)`** - Truncate color-coded string to `max_visible` visible chars, appending `"..."` if needed. Pads with spaces to full width for column alignment. Uses existing `strlen_no_colours()`.

```c
/**
 * olc_trunc - Truncate a color-code-aware string to a visible width
 *
 * @param dest         Output buffer
 * @param dest_size    Size of output buffer in bytes
 * @param src          Source string (may contain {X color codes)
 * @param max_visible  Maximum visible character width
 * @return             Number of visible characters written (excluding padding)
 */
int olc_trunc(char *dest, size_t dest_size, const char *src, int max_visible);
```

Implementation: Walk source byte-by-byte, skip `{X` color codes (2 bytes each), copy verbatim. When visible count reaches `max_visible - 3`, append `"..."` and `"{x"`. If string fits, copy as-is and pad with spaces to `max_visible`.

**`olc_wrap(lines, max_lines, src, max_visible, indent)`** - Word-wrap color-coded string into multiple lines at word boundaries. Carries color state across lines. Indents continuation lines.

```c
/**
 * olc_wrap - Word-wrap a color-code-aware string into multiple lines
 *
 * @param lines        Array of output line buffers (each MSL bytes)
 * @param max_lines    Maximum number of lines
 * @param src          Source string (may contain {X color codes)
 * @param max_visible  Maximum visible width per line
 * @param indent       Number of spaces to indent continuation lines
 * @return             Number of lines produced
 */
int olc_wrap(char lines[][MSL], int max_lines, const char *src,
             int max_visible, int indent);
```

### 1B. Store table column context in OLC_LAYOUT_CTX

Add two fields to `OLC_LAYOUT_CTX` in `editors/common.h`:

```c
const OLC_TABLE_COL *table_cols;   /* Active column defs, set by table_begin */
int                  table_ncols;  /* Number of active columns */
```

Forward-declare `typedef struct olc_table_col OLC_TABLE_COL;` in `common.h` (full struct stays in `olc_display.h`). Initialize both to NULL/0 in `olc_layout_new()`.

### 1C. Fix table rendering pipeline

**`olc_display_table_begin()`** - After rendering headers, store `cols`/`num_cols` into `ctx->table_cols`/`ctx->table_ncols`.

**`olc_display_table_row()`** - Read widths from `ctx->table_cols`. For each cell: call `olc_trunc()` to enforce column width, pad/align per `right_align` flag. Falls back to 15-char default if no column context (backward compat for any callers that skip `table_begin`).

**`olc_display_table_end()`** - Render bottom separator, then clear `ctx->table_cols = NULL; ctx->table_ncols = 0;`.

### 1D. Harden field renderers

- **`olc_display_string()`** - If `strlen_no_colours(value) > ctx->value_width`, truncate via `olc_trunc()`.
- **`olc_display_pair()`** - Truncate each value to `half_width - label_width - 2` using `olc_trunc()`.
- **`olc_display_infof()`** - Replace unsafe `strcat(buf, "\n\r")` with bounds-checked append.

### Files modified
- `editors/common.h` - Add table_cols/table_ncols to OLC_LAYOUT_CTX, forward-declare OLC_TABLE_COL
- `editors/common.c` - Initialize new fields in `olc_layout_new()`
- `editors/common/olc_display.h` - Declare `olc_trunc()`, `olc_wrap()`
- `editors/common/olc_display.c` - Implement utilities; fix `table_begin`/`table_row`/`table_end`; harden `string`/`pair`/`infof`

---

## Phase 2: Reduce Boilerplate

### 2A. History helper macro

Add `OLC_HISTORY_HELPERS` macro to `editors/common/olc_editor.h`. Generates the standard four-function pattern: `PREFIX_get_history()`, `PREFIX_ensure_history()`, `PREFIX_record()`, `PREFIX_record_cb()`.

```c
/**
 * OLC_HISTORY_HELPERS - Generate standard history helper functions
 *
 * Produces four static functions for change history management.
 * Editors with custom key logic (e.g., rsgedit) should not use this macro.
 *
 * @param PREFIX      Function prefix (e.g., racedit)
 * @param ENTITY_T    Entity struct type (e.g., RACE_DATA)
 * @param HIST_TYPE   OLC_HIST_* constant (e.g., OLC_HIST_RACE)
 * @param HIST_FIELD  Member name for history pointer (e.g., olc_history)
 * @param ID_EXPR     Expression for history key (e.g., e->id or e->name)
 */
#define OLC_HISTORY_HELPERS(PREFIX, ENTITY_T, HIST_TYPE, HIST_FIELD, ID_EXPR)
```

Replace in 6 editors (~30 lines each -> 1 line):
| Editor | Entity | Hist Type | ID Expression |
|--------|--------|-----------|---------------|
| `traitedit.c` | TRAIT_DEF | OLC_HIST_TRAIT | `e->id` |
| `racedit.c` | RACE_DATA | OLC_HIST_RACE | `e->id` |
| `clsedit.c` | CLASS_DATA | OLC_HIST_CLASS | `e->name` |
| `skedit.c` | SKILL_DATA | OLC_HIST_SKILL | `e->name` |
| `soedit.c` | SONG_DATA | OLC_HIST_SONG | `e->name` |
| `gredit.c` | SKILL_GROUP | OLC_HIST_GROUP | `e->name` |

**`rsgedit.c` stays manual** - uses custom `rsg_history_key()` function.

### 2B. get_area callback macro

Add `OLC_GET_AREA_FN` macro to `editors/common/olc_editor.h`:

```c
/**
 * OLC_GET_AREA_FN - Generate a standard get_area callback
 *
 * @param PREFIX      Function prefix (e.g., redit)
 * @param ENTITY_T    Entity struct type (e.g., ROOM_INDEX_DATA)
 * @param AREA_FIELD  Member name for area pointer (e.g., area or pArea)
 */
#define OLC_GET_AREA_FN(PREFIX, ENTITY_T, AREA_FIELD)  \
static AREA_DATA *PREFIX##_get_area(void *pEdit) {      \
    ENTITY_T *e = (ENTITY_T *)pEdit;                     \
    return e ? e->AREA_FIELD : NULL;                      \
}
```

Replace in 12 editors (~5 lines each -> 1 line):
| Editor | Entity | Area Field |
|--------|--------|------------|
| `redit.c` | ROOM_INDEX_DATA | area |
| `medit.c` | MOB_INDEX_DATA | area |
| `oedit.c` | OBJ_INDEX_DATA | area |
| `tedit.c` | TOKEN_INDEX_DATA | area |
| `qedit.c` | QUEST_INDEX_V2_DATA | area |
| `dngedit.c` | DUNGEON_INDEX_DATA | area |
| `bpedit.c` | BLUEPRINT | area |
| `bsedit.c` | BLUEPRINT_SECTION | area |
| `evtedit.c` | EVTEDIT_DATA | area |
| `wedit.c` | WILDS_DATA | pArea |
| `olc_mpcode.c` | SCRIPT_DATA | area |

**`aedit.c` stays manual** - casts `pEdit` directly to `AREA_DATA*`.

### 2C. Legacy cleanup in common.h / common.c

1. Migrate `qedit.c`'s 4 calls from `olc_render_section()` to `olc_display_section()`
2. Remove dead declarations from `common.h`: `olc_render_table_header`, `olc_render_field_row`, `olc_render_text_block_row`, `olc_render_table_footer`, `format_to_width`, `olc_select_tab`, `olc_render_tab_bar`
3. Mark remaining legacy `olc_render_*` functions as deprecated with comments (don't remove yet - some are used internally by the new framework)

### Files modified
- `editors/common/olc_editor.h` - Add OLC_HISTORY_HELPERS and OLC_GET_AREA_FN macros
- 6 editors - Replace history quartet with macro
- 12 editors - Replace get_area with macro (11 files; olc_mpcode is 12th)
- `editors/quests/qedit.c` - Migrate olc_render_section calls
- `editors/common.h` - Remove dead declarations
- `editors/common.c` - Remove dead stubs, add deprecation comments

---

## Phase 3: Migrate gameedit.c

The sole unmigrated editor. Uses manual if/else dispatch. Has its own changeset/rollback system.

### Approach: Command table dispatch, keep standalone pattern

gameedit is a standalone command (`do_gameedit set X Y`), not a modal editor. We add a command table for clean dispatch but keep it as a standalone command rather than forcing it into the modal editor lifecycle. The changeset/pending/rollback logic is preserved unchanged.

### Steps
1. Define `gameedit_table[]` command table (show, set, confirm, revert, rollback, history, pending, view, comment)
2. Replace the if/else chain in `do_gameedit()` with a table-driven dispatch loop
3. Migrate `gameedit_show()` to use `olc_display_*` renderers with `olc_theme_system`:
   - Category listing -> `olc_display_table_begin/row/end`
   - Single-setting detail -> `olc_display_header/section/string/bool/number/text/footer`
4. Preserve changeset/pending/rollback logic unchanged

### Files modified
- `editors/game_settings/gameedit.c` - Restructure dispatch, migrate display

---

## Phase 4: Framework Documentation

Create `src/docs/OLC_FRAMEWORK.md` with:

1. **Architecture Overview** - How editors plug into the game loop (`comm.c` -> `run_olc_editor` -> `olc_editor_interp`)
2. **Creating a New Editor (Step-by-Step)** - Define OLC_EDITOR_DEF, command table, show function, entry point; update CMakeLists.txt, Makefile, and interp.c
3. **OLC_EDITOR_DEF Reference** - Every field documented with valid values
4. **Display API** - All `olc_display_*()` functions with usage examples
5. **Command Helpers** - All `olc_cmd_*()` functions with usage examples
6. **Table Rendering** - OLC_TABLE_COL, truncation/wrapping behavior, column width enforcement
7. **Themes** - Available themes (world, entity, data, scripting, system, building) and when to use each
8. **Permissions** - OLC_PERM flags, per-command permissions
9. **Change Tracking** - AREA_FLAG vs EXPLICIT_SAVE vs CUSTOM vs NONE
10. **History/Audit** - OLC_HISTORY_HELPERS macro, manual pattern for custom cases
11. **Boilerplate Macros** - OLC_GET_AREA_FN, OLC_HISTORY_HELPERS with examples
12. **Design Notes: Future Display Backends** - Brief section noting the separation of editor logic from display, and how a future web client renderer would implement the same `olc_display_*` API against structured output instead of text buffers

### Files created
- `src/docs/OLC_FRAMEWORK.md`

---

## Implementation Order

```
Phase 1 (display hardening)  ──┐
                                ├──> Phase 3 (gameedit)  ──> Phase 4 (docs)
Phase 2 (boilerplate macros) ──┘
```

Phases 1 and 2 are independent. Phase 3 benefits from both. Phase 4 documents final state.

## Verification

After each phase:
```bash
cd /sentience/src && ./build && cd /sentience
```

Manual verification: Enter editors in-game, test tables with long values, flag displays, string fields near screen width boundaries.

## Design Note: Future Web Client Display

The current `olc_display_*` functions write directly to a `BUFFER` with `{X` color codes for telnet/terminal clients. When a web client is introduced, the cleanest path would be:

1. The `olc_display_*` API remains the editor-facing interface (editors don't change)
2. `OLC_LAYOUT_CTX` gains a `render_mode` or `backend` field indicating terminal vs web
3. Each `olc_display_*` function checks the backend and either:
   - Writes color-coded text to the buffer (current behavior), or
   - Emits structured data (JSON objects describing fields/tables/sections) for the web client to render with HTML/CSS
4. This means the current work should avoid baking terminal-specific assumptions into the *calling* code in editors -- editors should call `olc_display_string(ctx, theme, "Name", "name", value)` and let the display layer handle formatting. This is already the pattern for all migrated editors.

No action needed now, but this is why we avoid adding raw `snprintf`/color-code logic in editors and always go through the `olc_display_*` layer.
