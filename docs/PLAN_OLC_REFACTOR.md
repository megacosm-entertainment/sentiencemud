# OLC Editor Framework Refactoring Plan

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Architecture of the New Framework](#architecture-of-the-new-framework)
4. [OLC Change History (Audit Trail)](#olc-change-history-audit-trail)
5. [MXP Integration Strategy](#mxp-integration-strategy)
6. [Data / Display Separation (Future)](#data--display-separation-future)
7. [Migration Plan](#migration-plan)
8. [Editors to Add (Backport Detail)](#editors-to-add-backport-detail)
9. [Player Housing Considerations](#player-housing-considerations)
10. [File Inventory](#file-inventory)

---

## Executive Summary

The codebase has **28 OLC editors** spread across ~38,000 lines of editor code plus ~8,100 lines
in the core `olc.c`/`olc_act.c` files. These editors have evolved over many years through three
distinct implementation generations, resulting in significant code duplication, inconsistent
permissions, no audit trail, and no visual consistency.

This plan covers:
- **Unifying all editors** under a common framework (`editors/common/`)
- **Eliminating ~500+ lines of duplicated interpreter boilerplate**
- **Standardizing permissions, change tracking, and audit logging**
- **Adding a tab-based UI system** with per-editor color themes
- **Creating common display renderers** to replace thousands of hand-crafted `sprintf` calls
- **Change history / audit trail** — gameedit-style `history` and `view` commands in every
  editor so staff can see who changed what, when (modeled on gameedit's changeset viewer,
  but lightweight — no rollback)
- **MXP clickable link support** throughout all editors via `mxp_links.h` helpers and `bprintf`
- **Preparing for player-accessible editors** (housing)
- **Backporting editors** from `src_20_dev` (liqedit, matedit, sectoredit, corpsedit, repedit) and adding new ones (evtedit)
- **Future: data/display separation** — structuring the framework so display logic can be
  decoupled from data access, enabling alternative rendering targets (web client)

---

## Current State Analysis

### 2.1 Editor Inventory

| Editor    | Type       | ED_ Constant   | Location                              | Lines  | Gen   | Tabs | Change Model         |
|-----------|------------|----------------|---------------------------------------|--------|-------|------|----------------------|
| AEDIT     | Areas      | ED_AREA        | `editors/areas/aedit.c`               | 1,302  | 1     | No   | Area flag            |
| REDIT     | Rooms      | ED_ROOM        | `editors/rooms/redit.c`               | 1,971  | 1     | No   | Area flag            |
| OEDIT     | Objects    | ED_OBJECT      | `editors/objects/oedit.c`             | 3,516  | 1     | No   | Area flag            |
|           |            |                | `editors/objects/oedit_types.c`       | 1,955  |       |      |                      |
| MEDIT     | Mobiles    | ED_MOBILE      | `editors/mobiles/medit.c`             | 3,874  | 1     | No*  | Area flag            |
| TEDIT     | Tokens     | ED_TOKEN       | `editors/tokens/tedit.c`             | 1,060  | 3     | Yes  | Area flag (callback) |
| HEDIT     | Help       | ED_HELP        | `editors/help/hedit.c`               | 1,106  | 1     | No   | Area flag            |
| SHEDIT    | Ships      | ED_SHIP        | `editors/ships/shedit.c`             | 903    | 1     | No   | Area flag            |
| PEDIT     | Projects   | ED_PROJECT     | `editors/projects/pedit.c`           | 445    | 1     | No   | Global bool          |
| WEDIT     | Wilderness | ED_WILDS       | `editors/wilderness/wedit.c`         | ~650   | 1     | No   | Area flag            |
| VLEDIT    | Vlinks     | ED_VLINK       | `editors/wilderness/vledit.c`        | ~250   | 1     | No   | Area flag            |
| BSEDIT    | BP Sections| ED_BPSECT      | `editors/blueprints/bsedit.c`        | ~1,500 | 1     | No   | Area flag            |
| BPEDIT    | Blueprints | ED_BLUEPRINT   | `editors/blueprints/bpedit.c`        | ~1,450 | 1     | No   | Area flag            |
| DNGEDIT   | Dungeons   | ED_DUNGEON     | `editors/dungeons/dngedit.c`         | 5,825  | 1     | No   | Area flag            |
| CMDEDIT   | Commands   | ED_CMDEDIT     | `editors/commands/cmdedit.c`         | 968    | 1     | No   | Immediate save       |
| SOCEDIT   | Socials    | ED_SOCIAL      | `editors/socials/socialedit.c`       | 526    | 1     | No   | Explicit save        |
| RACEDIT   | Races      | ED_RACE        | `editors/races/racedit.c`            | ~1,340 | 2→FW | No   | Explicit save        |
| TRAITEDIT | Traits     | ED_TRAIT       | `editors/traits/traitedit.c`         | ~600   | 2→FW | No   | Explicit save        |
| SKEDIT    | Skills     | ED_SKILL       | `editors/skills/skedit.c`            | ~740   | 2→FW | No   | Explicit save        |
| GREDIT    | Groups     | ED_GROUP       | `editors/skills/gredit.c`            | ~380   | 2→FW | No   | Explicit save        |
| SOEDIT    | Songs      | ED_SONG        | `editors/skills/soedit.c`            | ~420   | 2→FW | No   | Explicit save        |
| CLSEDIT   | Classes    | ED_CLASS       | `editors/classes/clsedit.c`          | ~1,385 | 2→FW | No   | Explicit save        |
| RSGEDIT   | Rand Str   | ED_RSG         | `editors/random_strings/rsgedit.c`   | 42     | 1     | No   | Area flag            |
| MPEDIT    | MobScript  | ED_MPCODE      | `editors/scripting/olc_mpcode.c`     | 1,808  | 1     | No   | Area flag            |
| OPEDIT    | ObjScript  | ED_OPCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Area flag            |
| RPEDIT    | RoomScript | ED_RPCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Area flag            |
| TPEDIT    | TokScript  | ED_TPCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Area flag            |
| IPEDIT    | InstScript | ED_IPCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Custom               |
| APEDIT    | AreaScript | ED_APCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Area flag            |
| DPEDIT    | DngScript  | ED_DPCODE      | `editors/scripting/olc_mpcode.c`     |(shared)| 1     | No   | Custom               |
| GAMEEDIT  | Settings   | ED_GAMESETTING | `editors/game_settings/gameedit.c`   | 1,921  | Special | No | Changeset+Confirm  |
|           |            |                |                                       |        |       |      |                      |
| *Planned — backport from `src_20_dev`:* | | | | | | | |
| LIQEDIT   | Liquids    | ED_LIQEDIT     | `editors/liquids/liqedit.c`          | —      | —     | No   | Immediate save       |
| MATEDIT   | Materials  | ED_MATEDIT     | `editors/materials/matedit.c`        | —      | —     | No   | Immediate save       |
| SECTEDIT  | Sectors    | ED_SECTOREDIT  | `editors/sectors/sectoredit.c`       | —      | —     | No   | Immediate save       |
| CORPSEDIT | Corpse Types| ED_CORPSEDIT  | `editors/corpses/corpsedit.c`        | —      | —     | No   | Immediate save       |
| REPEDIT   | Reputation | ED_REPEDIT     | `editors/reputation/repedit.c`       | —      | —     | No   | Area flag            |
|           |            |                |                                       |        |       |      |                      |
| *Planned — new design / major rework:* | | | | | | | |
| RSGEDIT   | Rand Strings| ED_RSG        | `editors/random_strings/rsgedit.c`   | 42     | Stub  | No   | Area flag            |
| QEDIT     | Quests     | ED_QUEST       | `editors/quests/qedit.c`             | —      | —     | No   | Area flag            |
| MSNEDIT   | Missions   | ED_MISSION     | `editors/missions/msnedit.c`         | —      | —     | No   | Explicit save        |
| EVTEDIT   | Events     | ED_EVENT       | `editors/events/evtedit.c`           | —      | —     | No   | Explicit save        |

**Gen column key**: 1 = Gen 1 (manual dispatch), 2→FW = Gen 2 migrated to framework,
3 = Gen 3 (prototype framework), Special = non-modal

**\* medit has an extern declaration for `medit_tab_names` but no actual tab implementation**

**Non-modal editors**: GAMEEDIT operates as a direct command (`gameedit set ...`) rather than
entering a modal editor state. It uses a changeset/confirm workflow for safety.

### 2.2 Three Generations of Editor Code

#### Generation 1: Manual Dispatch (Oldest — lives partly in `olc.c`)

Used by: aedit, redit, oedit, medit, hedit, shedit, pedit, wedit, vledit, bsedit, bpedit,
dngedit, cmdedit, socialedit, rsgedit, and all script editors.

**Pattern** (repeated ~20 times with minor variations):
```c
void aedit(CHAR_DATA *ch, char *argument) {
    AREA_DATA *pArea;
    // Permission check (varies per editor)
    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);
    if (!str_cmp(command, "done")) { edit_done(ch); return; }
    ch->pcdata->immortal->last_olc_command = current_time;
    if (command[0] == '\0') { aedit_show(ch, argument); return; }
    for (cmd = 0; aedit_table[cmd].name; cmd++) {
        if (!str_prefix(command, aedit_table[cmd].name)) {
            if ((*aedit_table[cmd].olc_fun)(ch, argument))
                SET_BIT(pArea->area_flags, AREA_CHANGED);  // inline change marking
            return;
        }
    }
    interpret(ch, arg);
}
```

**Problems**:
- ~25 lines of identical boilerplate per editor
- Command tables live in `olc.c` (4,633 lines), not in their editor files
- Change marking is inline and varies (`SET_BIT`, `projects_changed = true`, `save_commands()`)
- Permission checks are inconsistent (6 different patterns found)
- No audit trail
- Show functions use raw `sprintf` + `add_buf` (~200+ lines per show function)

#### Generation 2: Self-contained (Newer data editors)

Used by: clsedit, racedit, traitedit, skedit, gredit, soedit.

Same boilerplate pattern, but:
- Command tables are in the editor's own `.c` file (self-contained)
- Uses `olc_set_editor()` helper
- **Return value from commands is ignored** — no change marking at all
- Uses explicit `save` command instead of area dirty flagging
- Entry point `do_*edit()` also in the same file

**Problems**: Still duplicated boilerplate, still manual show functions, no tabs, no MXP.

#### Generation 3: Framework-based (Only tedit)

Uses `olc_process_command_tabbed()` from `editors/common.c`:
```c
void tedit(CHAR_DATA *ch, char *argument) {
    // Permission check
    olc_process_command_tabbed(ch, argument, tedit_table, &tedit_tabs,
                               tedit_show, tedit_mark_changed);
}
```

**Benefits**: ~5 lines per editor, all boilerplate handled centrally, tab support, MXP support,
consistent color scheme via `olc_render_*()` functions, change marking via callback.

**Problems**: Only one editor uses it. The framework was a prototype that proved the concept but
wasn't generalized enough for full adoption.

### 2.3 Show Function Patterns

| Style | Used By | Characteristics | Lines per Show |
|-------|---------|-----------------|----------------|
| Raw `sprintf` | aedit, medit, redit, oedit, shedit, etc. | Manual formatting, no MXP, inconsistent colors | 100-629 |
| `formatf` | clsedit, racedit, skedit, traitedit | Slightly cleaner, still manual, no MXP | 50-135 |
| `olc_render_*` | tedit only | Framework renderers, MXP, NAWS-aware, consistent | 30-50 |
| `olc_buffer_show_*` | Available but unused | Helper functions in common.c, no current callers | N/A |

The `medit_show` function alone is **629 lines** of `sprintf` + `add_buf` calls.

### 2.4 Permission Checking Patterns (6 Variants Found)

| Pattern | Used By |
|---------|---------|
| `get_staff_rank(ch) < STAFF_CREATOR` | aedit |
| `IS_BUILDER(ch, pArea)` | redit, oedit, medit |
| `ch->tot_level < LEVEL_IMMORTAL` | tedit |
| `get_staff_rank(ch) < STAFF_IMPLEMENTOR` | pedit |
| `ch->pcdata->security < 9` | socialedit |
| None in interpreter (only in `do_*` entry point) | clsedit, racedit, skedit, traitedit, gredit, soedit |

### 2.5 Change Tracking Patterns (5 Variants Found)

| Pattern | Used By | Mechanism |
|---------|---------|-----------|
| `SET_BIT(area->area_flags, AREA_CHANGED)` | aedit, redit, oedit, medit, shedit, tedit | Area dirty flag, auto-saved on area save |
| `projects_changed = true` | pedit | Global bool |
| `save_commands()` | cmdedit | Immediate save on every change |
| Nothing (return value ignored) | clsedit, racedit, skedit, gredit, soedit, traitedit | Explicit `save` command |
| Changeset + confirm | gameedit | Pending changes list, requires confirmation |

### 2.6 Core OLC Infrastructure

**Descriptor state** (`merc.h`, `struct descriptor_data`):
```c
void *              pEdit;          // Pointer to entity being edited
int                 nEditTab;       // Current tab index
int                 nMaxEditTabs;   // Max tabs for this editor
HELP_CATEGORY       *hCat;         // Help editor category pointer
char **             pString;       // String editor pointer
int                 editor;        // ED_* constant
void                *editor_ptr;   // General-use pointer
```

**Key functions**:
- `olc_set_editor(ch, editor_type, pEdit)` — sets `pEdit`, `editor`, resets tab to 0
- `edit_done(ch)` — clears all editor state, timestamps last OLC command
- `process_olc_command()` — generic interpreter (Gen 3 framework)
- `olc_process_command_tabbed()` — tabbed variant of above

**`olc.c` contents** (4,633 lines):
- `editor_name_table[]` — maps ED_* constants to display names
- `editor_max_tabs_table[]` — maps ED_* constants to tab counts (all 0 currently)
- `editor_table[]` — maps editor names to `do_*` entry point functions
- All Gen 1 command tables (aedit_table, redit_table, oedit_table, etc.)
- All Gen 1 interpreter functions (aedit, redit, oedit, etc.)
- Shared helper functions (edit_done, show_commands, show_help, etc.)

---

## Architecture of the New Framework

### 3.1 File Structure

```
editors/
├── common.h              # Legacy display helpers (keep, gradually deprecate)
├── common.c              # Legacy display + process_olc_command (keep, shared)
├── common/
│   ├── olc_editor.h      # Editor definition, lifecycle, perms, audit    [NEW]
│   ├── olc_editor.c      # Editor framework implementation               [NEW]
│   ├── olc_display.h     # Theme-aware display renderers                  [NEW]
│   ├── olc_display.c     # Display renderer implementation                [NEW]
│   ├── olc_commands.h    # Common command helpers (string/number/flag/bool) [NEW]
│   └── olc_commands.c    # Command helper implementation                  [NEW]
├── areas/
│   └── aedit.c           # Command table + show + do_aedit (migrate)
├── mobiles/
│   ├── medit.c           # Command table + show + do_medit (migrate)
│   └── medit.h
├── objects/
│   ├── oedit.c
│   └── oedit_types.c
├── rooms/
│   └── redit.c
├── classes/
│   └── clsedit.c
├── races/
│   └── racedit.c
├── skills/
│   ├── skedit.c
│   ├── gredit.c
│   └── soedit.c
├── traits/
│   └── traitedit.c
├── tokens/
│   └── tedit.c
├── scripting/
│   └── olc_mpcode.c
├── commands/
│   └── cmdedit.c
├── socials/
│   └── socialedit.c
├── ships/
│   └── shedit.c
├── blueprints/
│   ├── bpedit.c
│   └── bsedit.c
├── dungeons/
│   └── dngedit.c
├── game_settings/
│   └── gameedit.c        # Non-modal; may remain special-cased
├── help/
│   └── hedit.c
├── projects/
│   └── pedit.c
├── wilderness/
│   ├── wedit.c
│   └── vledit.c
├── random_strings/
│   └── rsgedit.c
└── reserved_vnums/
    ├── reserved.c
    └── reserved.h
```

### 3.2 Core Framework: `OLC_EDITOR_DEF`

Each editor defines a single static struct that declares all its metadata:

```c
static const OLC_EDITOR_DEF clsedit_def = {
    .name           = "ClsEdit",
    .editor_type    = ED_CLASS,
    .cmd_table      = clsedit_table,
    .show_fn        = clsedit_show,
    .tabs           = {
        .count = 3,
        .tabs = {
            { "General",  "Gen",  clsedit_show_general  },
            { "Titles",   "Ttl",  clsedit_show_titles   },
            { "Rewards",  "Rwd",  clsedit_show_rewards  },
        }
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
};
```

The editor's interpreter function then becomes a single call:

```c
void clsedit(CHAR_DATA *ch, char *argument) {
    olc_editor_interp(ch, argument, &clsedit_def);
}
```

### 3.3 `olc_editor_interp()` Flow

```
Input arrives → olc_editor_interp(ch, argument, &def)
  ├── Permission check (def.perm)
  │     └── Fail → "Insufficient permissions" + edit_done()
  ├── smash_tilde + extract command word
  ├── "done" → edit_done()
  ├── Update last_olc_command timestamp
  ├── Tab switch? (if def.tabs.count > 0)
  │     ├── Numeric: "1", "2", "3" → switch tab, redisplay
  │     └── "tab <name>" → switch tab, redisplay
  ├── Empty input → def.show_fn(ch, "")
  ├── Command table lookup (def.cmd_table)
  │     ├── Found → execute, if returns true:
  │     │     ├── olc_mark_changed(ch, &def)
  │     │     └── olc_audit_log() if def.audit_changes
  │     └── Not found → interpret(ch, original_argument)
  └── Done
```

### 3.4 Permission System

```c
struct olc_editor_perm {
    int   flags;            // Bitmask of OLC_PERM_* flags
    int   min_staff_rank;   // STAFF_* constant (if PERM_STAFF_RANK)
    int   min_security;     // Security level (if PERM_SECURITY_LEVEL)
    bool  (*check_fn)(...); // Custom callback (if PERM_CUSTOM)
};
```

Permission flags (combinable):
| Flag | Meaning |
|------|---------|
| `OLC_PERM_NONE` | Immortal-only (basic check) |
| `OLC_PERM_STAFF_RANK` | `get_staff_rank(ch) >= min_staff_rank` |
| `OLC_PERM_AREA_SECURITY` | `IS_BUILDER(ch, area)` via `get_area_fn` |
| `OLC_PERM_SECURITY_LEVEL` | `ch->pcdata->security >= min_security` |
| `OLC_PERM_CUSTOM` | Custom callback function |
| `OLC_PERM_PLAYER` | Non-immortal players may use this editor |

For **player housing**, the `OLC_PERM_PLAYER | OLC_PERM_CUSTOM` combination allows
a custom callback to verify the player owns the room/object they're trying to edit.

#### 3.4.1 Per-Command Permission

Individual commands in an editor's `olc_cmd_type` table can require a minimum staff
rank via the `min_staff_rank` field. When 0 (the default), the command inherits the
editor-level permission. When set, `olc_editor_interp()` checks it before executing.

```c
// Example: restrict "scripts" command to Creators
{ "scripts",    skedit_scripts,  STAFF_CREATOR },
{ "name",       skedit_name                    },  // inherits editor perm (min_staff_rank=0)
```

#### 3.4.2 Per-Argument / In-Command Permission Utilities

Two utility functions allow fine-grained permission checks within command handlers:

- **`olc_check_command_perm(ch, min_staff_rank, cmd_name)`** — Simple staff-rank
  gate. Returns `false` and sends a denial message if the character lacks the rank.
  Called automatically by the dispatcher for per-command checks, but can also be
  called manually inside a command handler for argument-level gating.

- **`olc_check_perm(ch, perm, pEdit, label, silent)`** — Full `OLC_EDITOR_PERM`
  check supporting all permission modes (staff rank, security level, custom callback,
  player mode). Build a stack-local `OLC_EDITOR_PERM` and call this for complex
  argument-level validation.

```c
// Example: inside a command handler, require STAFF_CREATOR for "script" arg
bool medit_set(CHAR_DATA *ch, char *argument) {
    if (!str_cmp(arg, "script")) {
        if (!olc_check_command_perm(ch, STAFF_CREATOR, "script"))
            return false;
        // ... set the script ...
    }
}
```

### 3.5 Change Tracking Modes

| Mode | Behavior |
|------|----------|
| `OLC_CHANGE_AREA_FLAG` | `SET_BIT(area->area_flags, AREA_CHANGED)` via `get_area_fn` callback |
| `OLC_CHANGE_EXPLICIT_SAVE` | No auto-marking; editor has a `save` command |
| `OLC_CHANGE_CUSTOM` | Custom callback `mark_changed_fn(ch, pEdit)` |
| `OLC_CHANGE_NONE` | No tracking (read-only, or handled elsewhere) |

### 3.6 Color Theme System

Six pre-defined themes for visual editor identification:

| Theme | Label | Value | Section | Used For |
|-------|-------|-------|---------|----------|
| `olc_theme_default` | `{Y` | `{C` | `{G` | Fallback |
| `olc_theme_world` | `{Y` | `{G` | `{g` | Areas, rooms, wilderness |
| `olc_theme_entity` | `{Y` | `{C` | `{B` | Mobs, objects, tokens, ships |
| `olc_theme_data` | `{c` | `{W` | `{C` | Skills, races, classes, traits |
| `olc_theme_scripting` | `{M` | `{W` | `{m` | All script editors |
| `olc_theme_system` | `{R` | `{W` | `{r` | Commands, settings, socials |
| `olc_theme_building` | `{Y` | `{W` | `{y` | Blueprints, dungeons |

Themes can be defined per-editor or use a category default. The theme colors flow through
all `olc_display_*()` renderers automatically.

### 3.7 Display Renderer System

The `olc_display_*()` functions provide theme-aware rendering:

| Function | Purpose |
|----------|---------|
| `olc_display_new()` | Create themed layout context |
| `olc_display_header()` | Editor header with name, entity, tabs |
| `olc_display_footer()` | Bottom separator |
| `olc_display_string()` | Label + string value |
| `olc_display_number()` | Label + numeric value |
| `olc_display_dice()` | Label + XdY+Z dice |
| `olc_display_bool()` | Label + Yes/No |
| `olc_display_vnum()` | Label + vnum with entity name |
| `olc_display_percent()` | Label + percentage |
| `olc_display_flags()` | Flags with MXP toggles |
| `olc_display_type()` | Type selector from stat table |
| `olc_display_text()` | Multi-line text block |
| `olc_display_pair()` | Two fields side-by-side |
| `olc_display_section()` | Titled section divider |
| `olc_display_hr()` | Horizontal rule |
| `olc_display_blank()` | Empty line |
| `olc_display_table_begin/row/end()` | Inline data tables |
| `olc_display_list()` | Numbered lists with MXP delete |
| `olc_display_infof()` | Formatted info line |
| `olc_display_scripts()` | Grouped script display |
| `olc_display_vars()` | Variable display |
| `olc_display_entity_list()` | Paginated entity list |

All renderers accept a theme parameter and automatically use NAWS-detected screen width
for responsive column layout.

### 3.8 Audit Logging (Server-Side)

When `audit_changes = true` in an `OLC_EDITOR_DEF`, every command that returns `true`
(indicating a change was made) is automatically logged to the **server log**:

```
OLC AUDIT: [ClsEdit] Nibelung command 'name' with args 'Warrior'
OLC AUDIT: [MEdit] Builder changed name on entity 3001: 'old name' -> 'new name'
```

For finer-grained auditing, individual command functions can call:
```c
olc_audit_log(ch, &def, "name", old_name, new_name, mob->vnum);
```

This server-side logging is distinct from the **change history** system (Section 4)
which provides an in-game, player-visible audit trail stored alongside the entity data.

### 3.9 Relationship to Existing Code

The new framework is **additive** — it does not remove any existing functions. During migration:

- `editors/common.h` + `editors/common.c` remain as-is (legacy renderers, `process_olc_command`)
- `olc.h` ED_* constants, EDIT_* macros remain unchanged
- `olc.c` command tables gradually move into their editor files
- `olc.c` interpreter functions gradually become one-line `olc_editor_interp()` calls
- Both old and new display functions can be used in the same show function

---

## OLC Change History (Audit Trail)

### 4.1 Overview

Every editor supports `history` and `view` commands that let staff see who changed
what, when. This is modeled on gameedit's changeset viewer — but simpler, without rollback
or the pending/confirm workflow.

**Goal**: Any immortal editing an entity can type `history` to see a table of recent changes,
and `view <id>` to drill into a specific change entry showing old vs new values. This gives
accountability ("who messed up this mob?") without adding complexity.

**Status**: Fully implemented for all Gen 2 editors (skedit, gredit, soedit, racedit,
clsedit, traitedit). Framework support is in place for all future editors.

### 4.2 Design: Per-Entity Change Log

Unlike gameedit's singleton changeset array (which tracks a single global settings object),
OLC entities are numerous — there are thousands of rooms, mobs, objects, etc. The change
history system needs to be lightweight and per-entity.

#### Data Structures (`editors/common/olc_editor.h`)

```c
/**
 * A single field change within an OLC entity's history.
 */
struct olc_change_entry {
    char            *author;        /**< Character name who made the change */
    time_t          timestamp;      /**< When the change was made */
    int             id;             /**< Sequential ID within this entity's history */
    char            *field;         /**< Field name (e.g., "name", "level", "act_flags") */
    char            *old_value;     /**< Previous value as display string */
    char            *new_value;     /**< New value as display string */
};

/**
 * Change history for a single OLC entity.
 * Stored as a bounded list — oldest entries are evicted when full.
 */
struct olc_change_history {
    LLIST           *entries;       /**< List of OLC_CHANGE_ENTRY* (newest first) */
    int             next_id;        /**< Next sequential ID to assign */
    int             max_entries;    /**< Maximum entries to keep (default: 50) */
};
```

**Key differences from gameedit's model**:

| Feature | gameedit | OLC change history |
|---------|----------|--------------------|
| Scope | Global (one settings object) | Per-entity (each mob/obj/room/etc.) |
| Grouping | Changesets (multi-field batches) | Individual field changes |
| Rollback | Yes (`gameedit rollback <id>`) | No — view-only |
| Pending/confirm | Yes (staging area) | No — changes are immediate |
| Storage | `LLIST *changes` per changeset | `LLIST *entries` per entity |
| Max entries | `MAX_CHANGESETS = 20` | Per-entity cap (50) |
| Persistence | `json_changesets.c` | Per-type history files |

#### Constants

```c
#define OLC_MAX_HISTORY_ENTRIES     50   /**< Max history entries per entity */
#define OLC_HISTORY_FIELD_LEN       64   /**< Max field name length */
#define OLC_HISTORY_VALUE_LEN       256  /**< Max old/new value display length */
```

### 4.3 Recording Changes

Changes are recorded through helper functions called by command handlers (NOT automatically
by the framework — command handlers know the field name, old value, and new value).

#### Core Recording Function (`olc_editor.c`)

```c
void olc_history_record(OLC_CHANGE_HISTORY *history, CHAR_DATA *ch,
                        const char *field, const char *old_val,
                        const char *new_val);
```

#### Per-Editor Convenience Pattern

Each editor defines a static convenience function that combines recording with dirty
tracking (marking the entity for deferred persistence):

```c
static void racedit_record(RACE_DATA *race, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(racedit_ensure_history(race), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_RACE, race->id, race->olc_history);
}
```

The `*_ensure_history()` helper lazily initializes the history, loading persisted
history from disk on first access:

```c
static OLC_CHANGE_HISTORY *racedit_ensure_history(RACE_DATA *race)
{
    if (!race) return NULL;
    if (!race->olc_history) {
        race->olc_history = olc_history_load(OLC_HIST_RACE, race->id);
        if (!race->olc_history)
            race->olc_history = olc_history_new();
    }
    return (OLC_CHANGE_HISTORY *)race->olc_history;
}
```

#### Usage in Command Handlers

Record **before** overwriting the value so old_val captures the previous state:

```c
RACEDIT(racedit_name) {
    RACE_DATA *race;
    EDIT_RACE(ch, race);
    if (argument[0] == '\0') { /* show syntax */ return false; }

    racedit_record(race, ch, "name", race->name, argument);
    free_string(race->name);
    race->name = str_dup(argument);
    return true;
}
```

For flag toggles, record the flag name and whether it was added or removed:

```c
olc_history_record(hist, ch, "form",
    flag_name, IS_SET(race->form, value) ? "removed" : "added");
```

For bulk values (flag bitfields), record the full string:

```c
olc_history_record(hist, ch, "flags",
    flag_string(skill_flags, old_flags),
    flag_string(skill_flags, skill->flags));
```

### 4.4 The `history` Command

The `history` command is built into the framework as a recognized command in
`olc_editor_interp()` (like `done`). Any editor whose `OLC_EDITOR_DEF` has a
`get_history_fn` callback will automatically support `history` and `view`.

```c
typedef OLC_CHANGE_HISTORY *(*olc_get_history_fn)(void *pEdit);
```

In the `OLC_EDITOR_DEF`:
```c
static OLC_CHANGE_HISTORY *racedit_get_history(void *pEdit) {
    RACE_DATA *race = (RACE_DATA *)pEdit;
    return race ? (OLC_CHANGE_HISTORY *)race->olc_history : NULL;
}

static const OLC_EDITOR_DEF racedit_def = {
    ...
    .get_history_fn = racedit_get_history,
    ...
};
```

#### Display Format (similar to gameedit_history):

```
+------+--------------------+--------------------+-------------------------------+
| ID   | Author             | Field              | Date & Time                   |
+------+--------------------+--------------------+-------------------------------+
|   12 | Nibelung           | name               | 2025-01-15 14:32:07           |
|   11 | Nibelung           | level              | 2025-01-15 14:31:55           |
|   10 | Builder            | act_flags           | 2025-01-14 09:12:30           |
|    9 | Builder            | short_descr         | 2025-01-14 09:12:15           |
+------+--------------------+--------------------+-------------------------------+

Use 'view <id>' to see details of a specific change.
```

MXP-enabled clients get clickable IDs that send `view <id>`.

#### `history` accepts arguments:
- `history` — show all (up to max)
- `history 10` — show last 10
- `history name` — show changes matching a field name

### 4.5 The `view` Command

The `view <id>` command shows details of a single change entry, similar to
gameedit's `gameedit view <id>`:

```
+----------------------------------------------------------------------------+
| Change #12                                                                  |
+----------------------------------------------------------------------------+
| Author:  Nibelung                                                           |
| Date:    2025-01-15 14:32:07                                                |
+------------------------+---------------------------------------------------+
| Field                  | name                                              |
| Old Value              | a large ogre                                      |
| New Value              | a hulking ogre warrior                            |
+------------------------+---------------------------------------------------+
```

### 4.6 Persistence

**Status**: Fully implemented in `io/json/json_olc.c` / `io/json/json_olc.h`.

Change history is stored in **per-type files** under `data/history/`, not embedded in
each entity's own JSON data. This avoids extra I/O on every field change and allows
history to be searched/managed independently.

#### File Layout

| History Type | Constant | File Path |
|-------------|----------|-----------|
| Skills | `OLC_HIST_SKILL` (0) | `data/history/skills.json` |
| Groups | `OLC_HIST_GROUP` (1) | `data/history/groups.json` |
| Songs | `OLC_HIST_SONG` (2) | `data/history/songs.json` |
| Races | `OLC_HIST_RACE` (3) | `data/history/races.json` |
| Classes | `OLC_HIST_CLASS` (4) | `data/history/classes.json` |
| Traits | `OLC_HIST_TRAIT` (5) | `data/history/traits.json` |

New types are added by incrementing `OLC_HIST_MAX` and adding entries to the
`hist_files[]` and `hist_type_names[]` arrays in `json_olc.c`.

#### File Format

Each type file is a JSON object keyed by entity identifier (name for skills/groups/
songs/classes, id for races/traits):

```json
{
    "_format": "olc_history",
    "_version": 1,
    "entries": {
        "fireball": [
            {
                "id": 3,
                "author": "Nibelung",
                "timestamp": "2026-02-16T14:32:07",
                "field": "mana",
                "old_value": "25",
                "new_value": "30"
            },
            {
                "id": 2,
                "author": "Builder",
                "timestamp": "2026-02-15T09:12:30",
                "field": "name",
                "old_value": "fire ball",
                "new_value": "fireball"
            }
        ],
        "magic missile": [ ... ]
    }
}
```

#### Async / Batched Write Strategy

History changes are accumulated in memory and flushed to disk at strategic points,
avoiding a disk write on every single field change.

**Dirty tracking**: A linked list (`OLC_HISTORY_DIRTY`) tracks which entities have
unsaved history changes. Each dirty entry stores the history type, entity ID, and
a pointer to the in-memory history.

**Write triggers** (three levels of safety):

| Trigger | When | Function | Location |
|---------|------|----------|----------|
| Entity save | User types `save` in editor | `olc_history_flush()` | Each editor's save command |
| Periodic tick | Every 60 seconds (PULSE_AREA) | `olc_history_flush_all()` | `update_handler()` in `update.c` |
| Shutdown | Server shutdown/reboot | `olc_history_flush_all()` | `do_shutdown()` in `act_wiz.c` |

**Single-entity flush** (`olc_history_flush`): Loads the type file, replaces the
entity's entry array, writes back, removes from dirty list. Used by save commands
for immediate persistence.

**Batch flush** (`olc_history_flush_all`): Groups dirty entries by type, loads each
type file once, merges all dirty entries for that type, writes once per file, clears
dirty list. Used by periodic tick and shutdown for efficient bulk writes.

#### API Summary (`io/json/json_olc.h`)

| Function | Purpose |
|----------|---------|
| `olc_history_to_json(history)` | Serialize history to JSON array |
| `olc_history_from_json(arr)` | Deserialize JSON array to history |
| `olc_history_load(type, id)` | Load one entity's history from its type file |
| `olc_history_mark_dirty(type, id, history)` | Mark for deferred flush |
| `olc_history_flush(type, id, history)` | Immediately write one entity |
| `olc_history_flush_all()` | Flush all dirty entries (grouped by type) |
| `olc_history_remove(type, id)` | Remove entity from type file (on delete) |

For **area-format entities** (legacy `.are` files), history will not be persisted until
the entity is migrated to JSON storage. This is acceptable since `.are` is read-only going
forward — history will accumulate once worlds are saved as JSON.

### 4.7 Opt-in Per Struct

Not every struct needs change history immediately. The migration path:

1. **Phase 1** (DONE): `void *olc_history` added to `SKILL_DATA` (`merc.h`),
   `SKILL_GROUP` (`merc.h`), `SONG_DATA` (`merc.h`), `RACE_DATA` (`merc.h`),
   `CLASS_DATA` (`merc.h`), `TRAIT_DEF` (`traits.h`). All six editors wired
   with recording + persistence.
2. **Phase 2**: Add to `MOB_INDEX_DATA`, `OBJ_INDEX_DATA`, `ROOM_INDEX_DATA`,
   `AREA_DATA` — new `OLC_HIST_*` constants, new type files
3. **Phase 3**: Add to blueprint/dungeon/ship structs
4. **Phase 4**: Add to script program structs (optional — scripts have version control
   via code history already)

### 4.8 What NOT to Do

- **No rollback** — this is for auditing, not undo. If something needs reverting,
  staff can see what the old value was and manually set it back.
- **No changeset grouping** — gameedit groups multiple field changes into a single
  changeset with a confirmation step. OLC editors record individual field changes
  immediately. Grouping adds complexity without proportional benefit for entity editors.
- **No pending/confirm workflow** — changes are applied immediately as they always
  have been. The history is purely retroactive.

---

## MXP Integration Strategy

### 5.1 Overview

MXP (MUD eXtension Protocol) enables clickable links in the terminal client. The game
already has a comprehensive MXP helper system in `mxp_links.h`/`mxp_links.c`, plus
the `bprintf` safe buffer formatting function. The OLC framework should leverage these
throughout.

### 5.2 MXP Helpers Available

From `mxp_links.h` (all gracefully degrade to plain text when MXP is not enabled):

| Helper | Purpose | Example Use in OLC |
|--------|---------|--------------------|
| `mxp_command_link(d, cmd, text)` | Generic command link | Tab switching, save/done |
| `mxp_mob_link(d, mob)` | Link to a mob by vnum | Mob lists, vnum fields |
| `mxp_obj_link(d, obj)` | Link to an object by vnum | Object lists, vnum fields |
| `mxp_room_link(d, room)` | Link to a room by vnum | Exit targets, room lists |
| `mxp_help_link(d, topic)` | Link to a help topic | Flag/type help |
| `mxp_player_link(d, name)` | Link to a player | History author names |
| `mxp_link(d, cmd, text, hint)` | Low-level link with tooltip | Custom links |

### 5.3 Where MXP Should Be Used in OLC

| Context | MXP Action | Implementation |
|---------|-----------|----------------|
| **Tab bar** | Click tab name → switch tab | `mxp_command_link(d, "2", "Combat")` |
| **Flag fields** | Click flag name → toggle | Already in `olc_display_flags()` |
| **Type selectors** | Click type → cycle | `mxp_command_link(d, "sector forest", "forest")` |
| **Vnum fields** | Click vnum → jump to entity | `mxp_mob_link`/`mxp_obj_link`/`mxp_room_link` |
| **String fields** | Click label → edit command | `mxp_command_link(d, "name", "Name:")` |
| **List items** | Click `[X]` → delete | Already in `olc_display_list()` |
| **Scripts** | Click script → open script editor | `mxp_command_link(d, "edit mprog 3001", "3001")` |
| **History entries** | Click ID → view details | `mxp_command_link(d, "view 12", "#12")` |
| **History authors** | Click name → player info | `mxp_player_link(d, author)` |
| **`[Done]` button** | Click → exit editor | `mxp_command_link(d, "done", "[Done]")` |
| **`[Save]` button** | Click → save | `mxp_command_link(d, "save", "[Save]")` |

### 5.4 Implementation in Display Renderers

The `olc_display_*()` functions already accept a `CHAR_DATA *ch` parameter through
the layout context, and already check `display_use_mxp(ch)`. The approach:

1. **Labels become clickable**: `olc_display_string()` makes the label a clickable link
   that types the command for editing that field (e.g., clicking "Name:" sends `name `)
2. **Vnums become clickable**: `olc_display_vnum()` wraps vnums with domain-specific
   MXP links (`mxp_mob_link`, `mxp_obj_link`, etc.)
3. **Tabs become clickable**: `olc_display_header()` renders each tab name as a
   clickable link using `mxp_command_link()`
4. **Lists get action links**: `olc_display_list()` already supports `[X]` delete links;
   extend with `[+]` add and `[E]` edit links

### 5.5 `bprintf` Usage

The `bprintf` function (`recycle.h`) provides safe formatted output to a `BUFFER *`:

```c
bprintf(buffer, "%sName: %s%s{x\n\r", theme->label, theme->value, pMob->player_name);
```

This replaces the error-prone pattern:
```c
sprintf(buf, "%sName: %s%s{x\n\r", theme->label, theme->value, pMob->player_name);
add_buf(buffer, buf);
```

All new display renderers should use `bprintf`. During migration, existing `sprintf`+`add_buf`
patterns should be converted to `bprintf`.

---

## Data / Display Separation (Future)

### 6.1 Motivation

The game currently supports Telnet, TLS, and WebSocket connections. A future web client
would need a different rendering format (JSON/HTML) instead of the color-coded text used
by Telnet/MXP clients. The framework should be designed so that display logic can eventually
be replaced without rewriting data access.

### 6.2 Current Architecture

Today, show functions interleave data access and display:
```c
void medit_show(CHAR_DATA *ch, char *argument) {
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    // ... hundreds of sprintf() calls mixing data access and formatting
}
```

### 6.3 Design Principle: Renderer Abstraction

The `olc_display_*()` functions are already an implicit abstraction layer. Instead of
show functions directly building strings, they call renderers that know the data type
and the display format. This is the right direction.

**Future possibility**: A renderer could be swapped to produce JSON instead of
color-coded text, without changing the show function:

```c
// Current (telnet renderer):
olc_display_string(ctx, "Name", pMob->player_name, "name");
// → outputs: "{YName: {Ca hulking ogre warrior{x\n\r"

// Future (JSON renderer, same call):
olc_display_string(ctx, "Name", pMob->player_name, "name");
// → outputs: {"field":"Name","value":"a hulking ogre warrior","command":"name"}
```

### 6.4 Guidelines for Now

No immediate changes are needed, but the following principles should guide all
framework development:

1. **Show functions should use renderers, not raw `sprintf`**: Every field displayed
   should go through an `olc_display_*()` call. This ensures the rendering can be
   intercepted later.

2. **Renderers should receive semantic data, not pre-formatted strings**: Pass the
   raw value and the field type, not a pre-colored string. Let the renderer decide
   how to present it.

3. **Keep the `OLC_LAYOUT_CTX` extensible**: The layout context could later include
   a "renderer backend" flag that switches between text and JSON output.

4. **Command names as field identifiers**: The `command` parameter in renderers
   (used for MXP links today) doubles as a machine-readable field identifier that a
   web client could use as a JSON key.

5. **Don't embed color codes in data**: Keep color codes only in the display layer.
   Data structs should store plain values; colorization happens at render time.

### 6.5 Long-Term Vision

```
Show Function (semantic)
    │
    ├─ olc_display_string(ctx, "Name", value, "name")
    ├─ olc_display_number(ctx, "Level", value, "level")
    ├─ olc_display_flags(ctx, "Act", table, flags, "act")
    └─ ...
        │
        ├─ Text Renderer (current) → color codes + MXP links → Telnet client
        └─ JSON Renderer (future)  → structured JSON          → Web client
```

This is a **future consideration only** — no code changes needed now. The main takeaway
is: **always go through the display renderers**, never bypass them with raw `sprintf` in
new code.

---

## Migration Plan

### 7.1 Phase Overview

| Phase | Focus | Editors | Est. Effort |
|-------|-------|---------|-------------|
| **0** | Framework foundation | — | Done |
| **1** | Gen 2 quick wins | clsedit, racedit, traitedit, gredit, soedit, skedit | Small |
| **2** | Tabbed pioneers | medit, oedit | Medium |
| **3** | World editors | aedit, redit, wedit, vledit | Medium |
| **4** | Blueprint/dungeon | bsedit, bpedit, dngedit | Medium |
| **5** | Remaining editors | shedit, hedit, pedit, cmdedit, socialedit | Small-Med |
| **6** | Script editors | mpedit, opedit, rpedit, tpedit, ipedit, dpedit, apedit | Medium |
| **7** | Command table migration | Move tables from olc.c to editor files | Mechanical |
| **8** | Backport + new editors | liqedit, matedit, sectoredit, corpsedit, repedit, qedit, msnedit, rsgedit, evtedit | Med–Large |
| **9** | Player housing | Restricted REDIT/OEDIT subset | Large |

### 7.2 Phase 0: Framework Foundation (DONE)

**Status**: Complete.

Files created:
- `editors/common/olc_editor.h` — Editor definition, lifecycle, permissions, audit API
- `editors/common/olc_editor.c` — Framework implementation (~900 lines)
- `editors/common/olc_display.h` — Theme-aware display renderer API
- `editors/common/olc_display.c` — Display renderer implementation (~700 lines)
- `io/json/json_olc.h` — Per-type history file persistence API (~105 lines)
- `io/json/json_olc.c` — Dirty tracking, serialization, per-type file management (~515 lines)

All files added to both `CMakeLists.txt` and `Makefile`. Build verified.

### 7.3 Phase 1: Generation 2 Quick Wins

These editors are the easiest to migrate because they:
- Already have self-contained command tables in their own files
- Already use `olc_set_editor()`
- Don't mark changes inline (they use explicit `save`)
- Have relatively small, simple show functions

#### Phase 1 Status

| Editor | OLC_EDITOR_DEF | olc_editor_interp | olc_display_* show | history wired | history persistence | cmd helpers |
|--------|:-:|:-:|:-:|:-:|:-:|:-:|
| **traitedit** | Done | Done | Done | Done (5 commands) | Done | 3 of 10 |
| **gredit** | Done | Done | Done | Done (3 commands) | Done | 1 of 8 |
| **soedit** | Done | Done | Done | Done (8 recording points) | Done | 5 of 9 |
| **skedit** | Done | Done | Done | Done (23 commands) | Done | 15 of 19 |
| **racedit** | Done | Done | Done | Done (~25 commands) | Done | 14 of 25 |
| **clsedit** | Done | Done | Done | Done (16 commands) | Done | 8 of 20 |

All 6 Gen 2 editors are fully migrated to the new framework with:
- `OLC_EDITOR_DEF` static definition (name, type, table, show, theme, perms, change mode)
- Single-line `olc_editor_interp()` interpreter replacing manual dispatch boilerplate
- `olc_editor_enter()` replacing manual `olc_set_editor()` calls
- Show functions converted to themed `olc_display_*()` renderers
- `history` / `view` commands working via `get_history_fn`
- Per-field change recording in all data-modifying commands
- Async dirty tracking with flush on save / periodic tick / shutdown
- 46 command functions converted to `olc_cmd_*()` common helpers (1-3 lines each)

**Phase 1 is complete.** No remaining TODO items.

#### Phase 1 Completed: Common Command Helpers

To eliminate the repetitive boilerplate across the Gen 2 editors — where each
data-modifying command manually parsed arguments, validated input, called
`free_string`/`str_dup`, recorded history, and sent feedback — a set of common
command helper functions was created in `editors/common/olc_commands.h` and
`olc_commands.c`.

**Before** (repeated 30+ times across 6 editors):
```c
RACEDIT(racedit_name) {
    RACE_DATA *race;
    EDIT_RACE(ch, race);
    if (argument[0] == '\0') {
        send_to_char("Syntax: name <name>\n\r", ch);
        return false;
    }
    racedit_record(race, ch, "name", race->name, argument);
    free_string(race->name);
    race->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
}
```

**After** (1-3 lines per command):
```c
RACEDIT(racedit_name) {
    RACE_DATA *race;
    EDIT_RACE(ch, race);
    return olc_cmd_string(ch, argument, "Name", NULL, &race->name,
        OLC_STR_DEFAULT, race, racedit_record_cb);
}
```

##### Helper API

All helpers share a consistent signature pattern:
`(ch, argument, label, syntax, field_ptr, [metadata], ctx, record_fn)`

- `label` — used for feedback messages and history field names
- `syntax` — optional custom syntax help (NULL = auto-generated from label)
- `record_fn` — generic `olc_cmd_record_fn` callback for history recording

| Helper | Purpose |
|--------|---------|
| `olc_cmd_string()` | Set a `char *` field (with optional clear support via `OLC_STR_*` flags) |
| `olc_cmd_string_append()` | Open the multi-line string editor |
| `olc_cmd_number()` / `olc_cmd_number_i16()` | Set integer field with range validation |
| `olc_cmd_flag_toggle()` | Toggle bitwise flags via XOR |
| `olc_cmd_type_set()` / `olc_cmd_type_set_i16()` | Set a single value from a flag/stat table |
| `olc_cmd_bool()` | Toggle or explicitly set a boolean field |

The `_i16` variants exist because several structs (`SKILL_DATA`, `SONG_DATA`,
`CLASS_DATA`) use `int16_t` for fields like target, position, mana, beats, type,
and max_level.

##### Migration Results

46 command functions across all 6 Gen 2 editors were converted:

| Editor | Converted | Manual | Notes |
|--------|-----------|--------|-------|
| skedit | 15 | 4 | spellfun (entity lookup), save/list/show |
| soedit | 5 | 4 | spell (slot + entity lookup), save/list/show |
| gredit | 1 | 7 | add/remove (list manipulation), create/delete/save/list/show |
| racedit | 14 | 11 | act/affects (multi-bank flags), size/stats/maxstats/maxvitals (multi-arg), skills/prerequisite/remortinto (entity lookups), trait (polymorphic), save/list/show |
| clsedit | 8 | 12 | name (duplicate check), display/who (body-type sub-args), primary (custom stat lookup), reward/title/trait (complex sub-commands), create/save/list/show |
| traitedit | 3 | 7 | type (resets defaults), default (polymorphic by type), create/delete/save/list/show |

Commands remain manual when they have: custom validation (duplicate checks,
entity lookups), multi-argument sub-commands (`size min|max`, `stats <stat> <val>`),
multi-bank flag logic (`act_flags` + `act2_flags`), or polymorphic behavior
(`trait` commands that vary by type). These patterns will be evaluated for
additional helper coverage in Phase 2+ as needed.

#### Per-editor migration steps (for reference):

1. **Define `OLC_EDITOR_DEF`** at the top of the editor file
2. **Replace interpreter function** with `olc_editor_interp()` call
3. **Update `do_*edit()` entry point** to use `olc_editor_enter()`
4. **Add tab definitions** (split show into logical tabs)
5. **Migrate show function** to use `olc_display_*()` renderers
6. **Wire history** — add `olc_history` field to struct, recording in commands, persistence
7. **Test**: Verify all commands work, tabs switch, permissions check

### 7.4 Phase 2: Tabbed Pioneers (medit, oedit)

These are the largest, most complex editors and will benefit most from tabs:

**medit** (629-line show function → split into tabs):
| Tab | Contents |
|-----|----------|
| General | Name, area, vnum, level, alignment, sex, size, race, act flags |
| Combat | HP/mana/move dice, damage, hitroll, AC, attacks, offensive |
| Defense | Immunities, resistances, vulnerabilities, form, parts |
| Economy | Gold, shop data, loot |
| Scripts | Script programs |
| Special | Questor, trainer, boss, crew, persist |

**oedit** (split across oedit.c and oedit_types.c):
| Tab | Contents |
|-----|----------|
| General | Name, short, long, area, vnum, level, type, material |
| Properties | Weight, cost, condition, fragility, extra flags, wear flags |
| Values | Type-specific value fields (v0-v7) |
| Affects | Affects, applies, immunities, catalysts |
| Scripts | Script programs |
| Type | Type-specific subeditor (weapon, armor, container, etc.) |

Migration requires:
- Moving command tables from `olc.c` to the editor files
- Moving interpreter functions from `olc.c` to the editor files
- Splitting monolithic show functions into per-tab functions
- Keeping all existing commands working

### 7.5 Phase 3: World Editors (aedit, redit, wedit, vledit)

These editors are interrelated (areas contain rooms, rooms may be in wilderness).

Special considerations:
- **redit** uses `EDIT_ROOM` macro that checks for virtual rooms (not `pEdit`)
- **redit** will be the foundation for player housing (Phase 9)
- **aedit** needs area-level permission checks (`IS_BUILDER`)

Suggested redit tabs:
| Tab | Contents |
|-----|----------|
| General | Name, description, sector, room flags, heal rate, mana rate |
| Exits | North, south, east, west, up, down + NE/NW/SE/SW |
| Resets | Mob resets, object resets |
| Extra | Extra descriptions, conditional descriptions |
| Scripts | Room programs |

### 7.6 Phase 4: Blueprint / Dungeon Editors

These are large editors (dngedit is 5,825 lines) with complex data models.

Suggested dngedit tabs:
| Tab | Contents |
|-----|----------|
| General | Name, description, area-who, levels, flags |
| Structure | Floors, entry/exit points, zone/portal/mount outs |
| Groups | Min/max group, max players, idle timeout |
| Scripts | Dungeon programs |

### 7.7 Phase 5: Remaining Editors

- **shedit** — Ship editor, relatively simple
- **hedit** — Help editor, has category navigation (may need special tab handling)
- **pedit** — Project editor, small, uses global dirty flag
- **cmdedit** — Command editor, uses immediate save
- **socialedit** — Social editor, simple

### 7.8 Phase 6: Script Editors

All script editors share the same infrastructure via `scriptedit_*` functions.
They already have a common command table pattern. Migration involves:
- Defining one `OLC_EDITOR_DEF` per script type (MP, OP, RP, TP, IP, DP)
- Using `olc_theme_scripting` for all
- Script-specific `create` and `list` commands while sharing `show`, `code`, `compile`, etc.

### 7.9 Phase 7: Command Table Migration

Move remaining command tables from `olc.c` to their respective editor files. This is
mechanical refactoring:
1. Cut the table from `olc.c`
2. Paste into the editor's `.c` file
3. Remove the `extern` declaration from `olc.h`
4. Update the `OLC_EDITOR_DEF` to reference it
5. Repeat for each editor

Goal: `olc.c` should shrink from 4,633 lines to ~500 lines (just the shared infrastructure).

### 7.10 Phase 8: New Editors (Backport from `src_20_dev`)

Five editors exist in `src_20_dev` with full implementations that need to be
backported to `src`. These are **not** new designs — they have working code,
command tables, interpreters, and persistence already written.

| Editor | Cmds | Data Struct | src_20_dev Location | Current src Status | Effort |
|--------|------|-------------|--------------------|--------------------|--------|
| liqedit | 14 | `LIQUID` | `olc.c` + `olc_act.c` | Hardcoded `liq_type` in `const.c` | Medium |
| matedit | 10 | `MATERIAL` | `olc.c` + `olc_act.c` | Hardcoded `material_type` in `const.c` | Medium |
| sectoredit | 15 | `SECTOR_DATA` | `olc.c` + `sectors.c` | Hardcoded `SECT_*` #defines | Large |
| corpsedit | 18 | `CORPSE_TYPE` | `act_wiz.c` | Just a `CORPSE_TYPE(obj)` macro | Large |
| repedit | 9 | `REPUTATION_INDEX_DATA` | `olc.c` + `reputation.c` | Nothing (has separate plan doc) | Large |

Additional editors to design from scratch or complete:

| Editor | ED_ Constant | Purpose | Current Status | Effort |
|--------|-------------|---------|---------------|--------|
| EVTEDIT | ED_EVENT | Events (replaces gq system) | Nothing | Large |
| QEDIT | ED_QUEST | Builder quest chains | Struct exists, no editor | Large |
| MSNEDIT | ED_MISSION | Repeatable auto-missions | `missions.c` in src_20_dev | Large |
| RSGEDIT | ED_RSG | Random string generation | Stub editor, struct exists, no persistence | Medium |

The backported editors should be rebuilt using the new framework rather than
porting the Gen 1 boilerplate verbatim. The `src_20_dev` code serves as a
reference for command behavior and data model.

### 7.11 Phase 9: Player Housing

Player housing requires a **restricted subset** of REDIT and OEDIT available to
non-immortal players. Key design considerations:

**Permission model**:
```c
static bool housing_room_perm(CHAR_DATA *ch, void *pEdit) {
    ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *)pEdit;
    return room && is_room_owner(ch, room);  // Custom ownership check
}

static const OLC_EDITOR_DEF housing_redit_def = {
    .name       = "HouseEdit",
    .editor_type = ED_ROOM,
    .cmd_table  = housing_redit_table,     // Restricted command table!
    .show_fn    = housing_redit_show,
    .theme      = &olc_theme_world,
    .perm       = {
        .flags    = OLC_PERM_PLAYER | OLC_PERM_CUSTOM,
        .check_fn = housing_room_perm,
    },
    .change_mode = OLC_CHANGE_CUSTOM,
    .audit_changes = true,                  // Important for player actions
};
```

**Restricted command tables**: Player housing editors would have a **subset** of the
full editor commands. For example, players could change room names, descriptions,
and some flags, but not sector types, exits to non-owned rooms, or resets.

**Audit trail**: All player housing changes should be audited for abuse prevention.

---

## Editors to Add (Backport Detail)

The following editors have full working implementations in `src_20_dev`. Each
subsection documents the existing commands, data structures, persistence format,
and what needs to change in `src` before the editor can be ported.

### 8.1 Liquid Editor (liqedit)

**src_20_dev location**: `olc.c` (table, entry, interpreter), `olc_act.c` (all commands)

**Commands** (14): color, create, delete, flammable, fuel, full, hunger, list,
maxmana, name, proof, show, thirst

**Data struct** (`LIQUID` in `src_20_dev/merc.h`):
- name, uid, color (string)
- flammable (bool)
- proof, full, thirst, hunger (int — hours/affect values)
- fuel_unit, fuel_duration (fuel source properties)
- max_mana (mana recovery from drinking)
- Stored as a dynamic linked list (`liquid_list`), persisted to `liquids.dat`

**Current src**: Only has a simple hardcoded `liq_type` struct in `const.c`
(name, colour, affects array). No dynamic loading, no editor.

**Prerequisites to port**:
1. Backport full `LIQUID` struct to `merc.h`
2. Add `liquid_list` (LLIST), `liquid_lookup()`, `liquid_water`/`liquid_blood`/`liquid_potion` globals
3. Create `io/legacy/liquids.c` or `io/json/json_liquids.c` for persistence
4. Update all `liq_table[]` references to use dynamic `liquid_lookup()`
5. Build editor as `editors/liquids/liqedit.c` using framework

**Effort**: Medium — small editor, but underlying data model needs replacing.

### 8.2 Material Editor (matedit)

**src_20_dev location**: `olc.c` (table, entry, interpreter), `olc_act.c` (all commands)

**Commands** (10): class, corrodibility, create, flags, flammable, gm, name,
strength, value

**Data struct** (`MATERIAL` in `src_20_dev/merc.h`):
- name, gm (global material pointer)
- material_class (liquid, organic, metal, etc.)
- flags (bitfield)
- flammable (percentage), burned → `MATERIAL *` (what it becomes when burned)
- corrodibility (percentage), corroded → `MATERIAL *` (what it becomes when corroded)
- strength, value (1-10 scale)
- Stored as dynamic linked list (`material_list`), persisted to `materials.dat`

**Current src**: Only has a simple hardcoded `material_type` in `const.c`
(name, strength, value). No dynamic loading, no editor.

**Prerequisites to port**:
1. Backport full `MATERIAL` struct to `merc.h`
2. Add `material_list` (LLIST), `material_lookup()` and global pointers
3. Create persistence (legacy `.dat` or JSON)
4. Update all `material_table[]` references to use dynamic `material_lookup()`
5. Build editor as `editors/materials/matedit.c` using framework

**Effort**: Medium — same pattern as liqedit. Small, simple editor.

### 8.3 Sector Editor (sectoredit)

**src_20_dev location**: `olc.c` (table, entry, interpreter), `sectors.c`
(load/save, command implementations)

**Commands** (15): affinity, class, comments, create, description, flags, gsct,
health, hidemsgs, mana, move, movecost, name, show, soil

**Data struct** (`SECTOR_DATA` in `src_20_dev/merc.h`):
- name, description, comments
- gsct (global sector constant pointer — e.g., `gsct_inside`)
- sector_class, flags
- move_cost (movement point cost)
- health_rate, mana_rate (regen modifiers)
- soil properties
- hide messages (custom messages when hiding in this terrain)
- element affinity
- Stored as dynamic linked list (`sectors_list`), persisted to `sectors.dat`
- Rooms reference sectors via `SECTOR_DATA *` pointer instead of `int`

**Current src**: Sectors are hardcoded `#define SECT_*` constants (0–27 in `merc.h`).
Rooms use `int sector_type`. No `SECTOR_DATA` struct, no `sectors.c`, no editor.

**Prerequisites to port**:
1. Backport full `SECTOR_DATA` struct to `merc.h`
2. Create `sectors.c` with load/save and `sectors_list`
3. **Refactor every room**: change `int sector_type` → `SECTOR_DATA *sector`
4. Update all `SECT_*` comparisons to use sector pointer/lookup
5. Bootstrap default sectors from current `SECT_*` defines
6. Build editor as `editors/sectors/sectoredit.c` using framework

**Effort**: Large — the editor itself is medium complexity, but the underlying
room system refactor (int→pointer) touches many files.

### 8.4 Corpse Editor (corpsedit)

**src_20_dev location**: `act_wiz.c` (table, entry, interpreter, load/save,
all command implementations). IMPLEMENTOR-only access.

**Commands** (18): animate, chance, comments, create, damage, decay, description,
gcrp, headless, keywords, lost, long, message, name, ownerloot, short, show, skull

**Data struct** (`CORPSE_TYPE` in `src_20_dev/merc.h`):
- Appearance: name, keywords, short_descr, long_descr, description
- Headless variant: separate short/long/description for headless version
- Animated variant: name, long_descr, description, headless flag
- Skull mechanics: success/fail messages (self + others)
- Decay: type (what it decays into), rate, timer, spill settings
- Damage sub-entries: linked list of `CORPSE_DAMAGE` structs
- Owner loot control, resurrection/animation/skulling chances
- gcrp (global corpse type pointer — e.g., `gcrp_humanoid`)
- Stored as dynamic linked list (`corpse_list`), persisted to `corpse.dat`

**Current src**: Only has `CORPSE_TYPE(obj)` as a macro for `obj->value[0]`.
No `CORPSE_TYPE` struct, no editor.

**Prerequisites to port**:
1. Backport full `CORPSE_TYPE` and `CORPSE_DAMAGE` structs to `merc.h`
2. Create persistence (legacy `.dat` or JSON)
3. Integrate into mob death / `make_corpse()` — assign corpse types to mobiles
4. Add `corpse_type` field to `MOB_INDEX_DATA`
5. Build editor as `editors/corpses/corpsedit.c` using framework

**Effort**: Large — medium editor, but integration into death/corpse generation
and mobile data is substantial.

### 8.5 Reputation Editor (repedit)

**src_20_dev location**: `olc.c` (table, entry, interpreter), `reputation.c`
(load/save, command implementations)

**Commands** (9): comments, create, description, flags, initial, name, rank,
show, token

**Data struct** (`REPUTATION_INDEX_DATA` in `src_20_dev/merc.h`):
- name, description, comments, flags, uid
- Ranks: linked list of `REPUTATION_INDEX_RANK_DATA` (name, uid, min/max values)
- Token reference (associated token for script integration)
- Area-bound: saved in `.are` files via `olc_save.c`, uses WNUM for indexing
- Areas have `factions` (LLIST of `REPUTATION_INDEX_DATA *`)

**Current src**: No reputation system at all — no struct, no editor, no
`reputation.c`. A separate plan exists: `docs/PLAN_backport_reputation_system.md`

**Prerequisites to port**:
1. Follow `PLAN_backport_reputation_system.md` for the full subsystem
2. Backport `REPUTATION_INDEX_DATA`, `REPUTATION_INDEX_RANK_DATA` structs
3. Create `reputation.c` with load/save
4. Integrate into area save/load pipeline
5. Add player reputation tracking (`REPUTATION_DATA` on characters)
6. Build editor as `editors/reputation/repedit.c` using framework

**Effort**: Large — entire reputation subsystem must be ported first.

### 8.6 Event Editor (EVTEDIT) — New Design

The global quest (gq) system in `gq.c` would be replaced by a more general event
system. The event editor would manage event definitions:

- **Event types**: Invasion, collection quest, boss spawn, world event, timed challenge
- **Scheduling**: Recurring, one-time, triggered, random
- **Rewards**: Experience, gold, tokens, items, reputation
- **Requirements**: Level range, class, race, quest prerequisites

This editor has no `src_20_dev` predecessor — it would be designed from scratch
using the new framework.

### 8.7 Quest Editor (QEDIT) — Complete Existing Design

**Current src**: `QUEST_INDEX_DATA` struct exists in `merc.h` with a rich data model
(stages, requirements, rewards), and `quest.c` (457 lines) has basic runtime
functions (`get_quest_index`, load/save via `.are` files). `qedit` interpreter
is declared in `olc.h` but **never implemented** — no command table, no functions.

**Data struct** (`QUEST_INDEX_DATA` in `merc.h`):
- name, description, flags (QUEST_REPEATABLE, QUEST_DAILY)
- Multi-stage: LLIST of stages (QUEST_INDEX_STAGE_HEADER), with sub-types:
  - Fetch stages (collect N items from mobs/regions)
  - Slay stages (kill N mobs)
  - Escort stages (planned)
- Prerequisites: LLIST of WNUM references to other quests
- Requirements: class, class level, race, race level, reputation rank,
  skills (QUEST_INDEX_SKILL_REQUIREMENT), songs, stat ranges
- Rewards: exp, gold, silver, pneuma, deity points, mission points,
  practices, trains, paragon levels, reputation rank/points,
  item rewards (QUEST_INDEX_ITEM_REWARD), skill/song rewards,
  reward script (token-based scripted reward)
- Area-bound, uses WNUM, saved in `.are` files

**Prerequisites to build**:
1. Create `editors/quests/qedit.c` using framework
2. Implement command table covering all `QUEST_INDEX_DATA` fields
3. Stage sub-editor for managing multi-step quest flows
4. Prerequisites linking (select from other quests in same/other areas)
5. Reward configuration commands
6. Wire into area save/load (partially done — quest load exists in `quest.c`)

**Effort**: Large — the data model is already there and well-structured but the
editor needs to handle complex nested structures (stages, requirements, rewards).

### 8.8 Mission Editor (MSNEDIT) — Repeatable Auto-Quests

Inspired by FFXIV's Levequest system: small, repeatable, procedurally-generated
quests given out by designated "missionary" NPCs. This is the
enhancement/replacement for the existing quest system's repeatable side.

**src_20_dev**: `missions.c` (1,952 lines) has a working mission runtime:
- Player commands: `mission info|points|time|request|cancel|complete`
- `generate_mission()` creates procedural multi-part missions
- `generate_mission_part()` delegates to mob scripts (TRIG_MISSION_PART)
- Mission parts: retrieve obj, slay mob, rescue mob, travel to room, custom tasks
- Mission scrolls: physical objects given to players describing their mission
- Missionaries configured via `medit missionary` sub-command on mob indexes
- Timer-based: missions expire, daily allowance system
- Game settings: `max_mission_allowance`, `inc_missions`, `max_missions`

**Data structs** (`src_20_dev/merc.h`):
- `MISSIONARY_DATA`: scroll template, appearance (keywords, short/long, header/footer/prefix/suffix, line width)
- `MISSION_DATA`: giver/receiver (mob/obj/room + WNUM), timer, class restrictions, parts list, scripted flag
- `MISSION_PART_DATA`: obj/mob/room targets, description, minutes expected, completion tracking

**Current src**: No `missions.c`, no `MISSION_DATA` struct, no missionary system.
The `QUEST_INDEX_DATA` covers builder-designed one-off quests, not repeatable
procedural missions.

**Design goals for the editor**:
- **MSNEDIT** would edit mission *templates* (not runtime instances)
- Define mission categories, difficulty tiers, reward scales
- Configure which task types are available per missionary
- Set up missionary NPCs (currently a medit sub-command; could become standalone)
- Define mission part templates with objective types and parameters
- Configure reward formulas (scaling by level, class type)

**Prerequisites to build**:
1. Backport `MISSION_DATA`, `MISSION_PART_DATA`, `MISSIONARY_DATA` structs
2. Backport `missions.c` runtime (player commands, generation, update loop)
3. Backport missionary medit sub-command or create standalone editor
4. Design mission template struct (new — src_20_dev generates missions procedurally via scripts)
5. Create `editors/missions/msnedit.c` using framework
6. Create persistence (JSON preferred for new systems)

**Effort**: Large — requires backporting the entire mission runtime first,
then designing the template/editing layer on top.

### 8.9 Random String Generator Editor (RSGEDIT) — Complete Stub

The RSG system generates random strings (primarily names) from weighted patterns
and character classes. It is consumed by other systems (mob name generation,
item naming, etc.).

**Current src**: The editor shell exists but is nearly empty:
- `olc_edit_rsg.c` (112 lines): interpreter, 3 command tables (main, pattern, class)
  with 21 function references — but only `rsgedit_create` has a body (and it's a stub
  that returns false)
- `editors/random_strings/rsgedit.c` (42 lines): just includes and a stub `rsgedit_create`
- `RANDOM_STRING` struct exists in `merc.h` with: uid, name, description,
  patterns (linked list of `RANDOM_PATTERN` with weight, name, class references),
  classes (linked list of `RANDOM_CLASS` with uid, name)
- `RANDOM_STRING_ENTRY` struct: weight + string (for entries within a class)
- `ED_RSG` (13) constant defined in `olc.h`
- **No persistence** — no load/save functions exist in either codebase
- **No implementations** — `src_20_dev` is identical (same stub)

**Command tables already defined** (need implementations):
- Main: list, create, show, pattern, class, generate
- Pattern sub-editor: list, create, show, delete
- Class sub-editor: list, create, show, delete, add, edit, remove

**Design considerations**:
- Patterns define the structure of generated strings (e.g., `CVC` for consonant-vowel-consonant)
- Classes define character sets (e.g., "vowels" = {a, e, i, o, u} with weights)
- Support for dictionary-based generation (word lists, syllable banks)
- Support for rules/constraints (no triple consonants, capitalization, etc.)
- Output should be consumable by other editors (medit for mob names, oedit for item names)
- Persistence: JSON in `data/system/rsg/` or per-area in `.are` files

**Prerequisites to build**:
1. Design persistence format (JSON recommended)
2. Implement load/save functions
3. Implement all 21 stub functions across the 3 command tables
4. Add dictionary support (word lists loadable from files)
5. Add rule/constraint system
6. Consolidate the two source files (`olc_edit_rsg.c` → `editors/random_strings/rsgedit.c`)
7. Migrate to framework (currently Gen 1 boilerplate in `olc_edit_rsg.c`)

**Effort**: Medium — struct exists, tables exist, sub-editor pattern already
designed. Needs implementations and persistence, but no external system
dependencies.

---

## Player Housing Considerations

### 9.1 Which Editors Need Player Variants

| Editor | Player Need | Restriction Level |
|--------|-------------|-------------------|
| REDIT  | Room editing (name, desc, flags) | Heavy — no exits/resets/sector |
| OEDIT  | Furniture/decoration placement | Heavy — cosmetic only |
| TEDIT  | Token customization | Medium — predefined templates |

### 9.2 Permission Architecture

The `OLC_PERM_PLAYER` flag combined with `OLC_PERM_CUSTOM` callbacks means:
- No need to duplicate editor code
- Same framework, different command tables
- Custom permission callbacks verify ownership per-action
- Full audit trail for all player modifications

### 9.3 Safety Measures

- **Restricted command tables**: Player editors use a whitelist of safe commands
- **Value validation**: All player inputs validated against allowed ranges
- **Rate limiting**: Prevent spamming editor changes
- **Rollback**: Audit trail enables reverting malicious changes
- **Area isolation**: Player housing rooms are in special areas with limited connectivity

---

## File Inventory

### 10.1 New Files Created (Phase 0–1)

| File | Lines | Purpose |
|------|-------|---------|
| `editors/common/olc_editor.h` | ~540 | Editor definition, lifecycle, permissions, history API |
| `editors/common/olc_editor.c` | ~900 | Framework implementation (interp, perms, history display) |
| `editors/common/olc_display.h` | ~400 | Theme-aware display renderer API |
| `editors/common/olc_display.c` | ~700 | Display renderer implementation |
| `editors/common/olc_commands.h` | ~215 | Common command helpers API (8 functions) |
| `editors/common/olc_commands.c` | ~300 | Command helper implementation |
| `io/json/json_olc.h` | ~105 | History type constants, persistence API |
| `io/json/json_olc.c` | ~515 | Dirty tracking, serialization, per-type file management |

### 10.2 Files to Modify (During Migration)

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `editors/common/olc_editor.c` and `editors/common/olc_display.c` |
| `Makefile` | Add corresponding object files |
| `olc.c` | Gradually remove command tables + interpreter functions (4,633 → ~500 lines) |
| `olc.h` | Remove corresponding `extern` declarations as tables move |
| Each editor `.c` file | Add `OLC_EDITOR_DEF`, replace interpreter, migrate show function |

### 10.3 Existing Files (Reference)

| File | Lines | Contents |
|------|-------|----------|
| `editors/common.h` | 217 | Legacy display helpers, `OLC_LAYOUT_CTX`, `OLC_EDITOR_TABS` |
| `editors/common.c` | 976 | Legacy renderers, `process_olc_command`, `olc_render_*` |
| `olc.c` | 4,633 | Editor tables, interpreters, shared helpers |
| `olc.h` | 620 | ED_* constants, EDIT_* macros, all prototypes |
| `olc_act.c` | 3,312 | Shared editor action functions |
| `olc_act2.c` | ~200 | Condition phrase helpers |
| `olc_save.c` | ~2,000 | Area save routines (legacy .are format) |

### 10.4 Summary Statistics

| Metric | Before | After (Projected) | Current (Phase 1 Done) |
|--------|--------|-------------------|------------------------|
| Lines of interpreter boilerplate | ~550 (25 × 22 editors) | ~44 (2 × 22 editors) | ~510 (6 migrated) |
| Show function patterns | 4 different styles | 1 unified style via `olc_display_*()` | 6 editors converted |
| Permission patterns | 6 different patterns | 1 centralized check | 6 editors converted |
| Change tracking patterns | 5 different patterns | 4 well-defined modes | 6 editors use explicit save |
| Audit logging (server-side) | None | All editors | 6 editors |
| Change history (in-game) | gameedit only | All editors (opt-in per struct) | 6 editors fully wired |
| History persistence | None (gameedit: json_changesets) | Per-type files, async writes | 6 type files implemented |
| Lines in `olc.c` | 4,633 | ~500 | 4,633 (not yet reduced) |
| Color consistency | None (per-editor ad hoc) | 6 category themes | 6 editors themed |
| MXP support in show | 1 editor (tedit) | All editors (labels, vnums, tabs, lists) | 6 editors (history IDs) |
| `bprintf` usage | Partial | All new display code | All new display code |
| NAWS screen-width support | 1 editor (tedit) | All editors | 6 editors |
| Tab UI support | 1 editor (tedit) | All editors that benefit | Framework ready, tabs planned |
| Player-accessible editors | 0 | Prepared for housing | Framework ready |
| Data/display separation | None | Display via renderers (JSON-ready) | 6 editors via renderers |
