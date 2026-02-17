# OLC Editor Migration Assessment

Comprehensive assessment of 5 editors to be migrated to the unified OLC editor framework.

---

## 1. Ship Editor — `shedit`

### A. File & Location
- **Editor file**: `editors/ships/shedit.c` (903 lines)
- **Command table**: `boat.c` L8271–8293
- **Interpreter**: `boat.c` L8420–8465 (`void shedit()`)
- **Entry point**: `boat.c` L8482–8528 (`void do_shedit()`)

### B. Includes
```c
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
```
**Does NOT include**: `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### C. ED Constant
`ED_SHIP` = 8 (olc.h L41)

### D. Command Table (`shedit_table[]` in boat.c)

| Command | Handler | Line (shedit.c) | Purpose |
|---------|---------|-----------------|---------|
| `?` | `show_help` | (olc.c) | Show help |
| `armor` | `shedit_armor` | L811 | Set base armor rating |
| `blueprint` | `shedit_blueprint` | L299 | Assign blueprint to ship |
| `capacity` | `shedit_capacity` | L787 | Set ship passenger/item capacity |
| `class` | `shedit_class` | L260 | Set ship class type |
| `commands` | `show_commands` | (olc.c) | List editor commands |
| `create` | `shedit_create` | L161 | Create new ship index |
| `crew` | `shedit_crew` | L618 | Set min/max crew |
| `desc` | `shedit_desc` | L243 | Edit ship description (string_append) |
| `flags` | `shedit_flags` | L280 | Toggle ship flags |
| `guns` | `shedit_guns` | L561 | Set gun count |
| `hit` | `shedit_hit` | L481 | Set hit points |
| `keys` | `shedit_keys` | L836 | Manage special keys (list/add/remove) |
| `list` | `shedit_list` | L39 | List all ship indexes |
| `move` | `shedit_move` | L673 | Set move delay and steps |
| `name` | `shedit_name` | L224 | Set ship name |
| `oars` | `shedit_oars` | L592 | Set number of oar positions |
| `object` | `shedit_object` | L433 | Set ship object reference |
| `show` | `shedit_show` | L44 | Display all ship properties |
| `turning` | `shedit_turning` | L518 | Set turning power (degrees) |
| `weight` | `shedit_weight` | L752 | Set max weight |

### E. Show Function (`shedit_show`, L44–155)
Displays: vnum + name, ship class, flags, blueprint (vnum + name), ship object (vnum + name), hit points, max guns, min crew, max crew, oars, move delay, move steps, max turning, max weight, capacity, base armor, description, special keys list.

**Suggested tab structure**: Single tab — all fields are related to ship properties.

### F. Interpreter (`shedit()` in boat.c L8420)
- Standard pattern: `smash_tilde`, copy arg, `one_argument`
- Security check: `can_edit_ships(ch)` — requires `security >= 9` AND `tot_level >= MAX_LEVEL`
- On `done`: `edit_done(ch)`
- Empty command: `shedit_show(ch, argument)`
- On success: sets `AREA_CHANGED` flag on ship's area AND `ships_changed = true`
- Fallthrough: `interpret(ch, arg)`

### G. Change/Save Model
- **Area flag**: `SET_BIT(ship->area->area_flags, AREA_CHANGED)` — triggers save in area save cycle
- **Global flag**: `ships_changed = true`
- Saved as part of area files

### H. Permission Model
- `can_edit_ships(ch)`: requires `security >= 9` AND `tot_level >= MAX_LEVEL`
- `shedit_create`: additionally checks `IS_BUILDER(ch, pArea)`
- Entry via `interp.c`: `POS_DEAD, L5, LOG_NORMAL`

### I. Framework Features Used
**None**. Does not use `olc_set_editor()`, `olc_commands.h`, `olc_display.h`, or `olc_editor.h`.

---

## 2. Help Editor — `hedit`

### A. File & Location
- **Editor file**: `editors/help/hedit.c` (1106 lines)
- **Command table**: `olc.c` L152–178 (`hedit_table[]`)
- **Interpreter**: `olc.c` L1575–1636 (`void hedit()`)
- **Entry point**: `olc.c` L1638–1654 (`void do_hedit()`)

### B. Includes
```c
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
```
**Does NOT include**: `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### C. ED Constant
`ED_HELP` = 9 (olc.h L42)

### D. Command Table (`hedit_table[]` in olc.c)

| Command | Handler | Line (hedit.c) | Purpose |
|---------|---------|-----------------|---------|
| `commands` | `show_commands` | (olc.c) | List editor commands |
| `show` | `hedit_show` | L37 | Display help entry or category |
| `builder` | `hedit_builder` | L682 | Toggle builders on help/category |
| `addcategory` | `hedit_addcat` | L385 | Add new help category |
| `description` | `hedit_description` | L557 | Edit category description |
| `name` | `hedit_name` | L530 | Rename current category |
| `remcategory` | `hedit_remcat` | L440 | Remove a help category |
| `opencategory` | `hedit_opencat` | L413 | Navigate into a category |
| `upcategory` | `hedit_upcat` | L427 | Navigate up to parent category |
| `shiftcategory` | `hedit_shiftcat` | L474 | Shift category left/right in list |
| `delete` | `hedit_delete` | L1050 | Delete a help file |
| `edit` | `hedit_edit` | L224 | Open a help file for editing |
| `keyword` | `hedit_keywords` | L583 | Set help file keywords |
| `level` | `hedit_level` | L612 | Set minimum level |
| `make` | `hedit_make` | L169 | Create new help entry |
| `move` | `hedit_move` | L259 | Move help/category between categories |
| `security` | `hedit_security` | L647 | Set security level |
| `text` | `hedit_text` | L519 | Edit help text (string_append) |
| `addtopic` | `hedit_addtopic` | L904 | Add related topic |
| `remtopic` | `hedit_remtopic` | L968 | Remove related topic |

### E. Show Function (`hedit_show`, L37–163)
**Dual-mode display** — behavior depends on whether `ch->desc->pEdit` is set:

**When editing a help entry** (`pEdit != NULL`): keywords, security, builders, creator, created date, modified by, modified date, category, minimum level, horizontal rule, help text, related topics list.

**When browsing a category** (`pEdit == NULL`): category name, security, builders, creator, created date, modified by, modified date, minimum level, description, then a listing of subcategories and helpfiles in the category.

**Suggested tab structure**: This editor has a unique dual-mode. A single "Properties" tab per mode, but the category listing is conceptually a separate view.

### F. Interpreter (`hedit()` in olc.c L1575)
- Standard pattern: `smash_tilde`, copy arg, `one_argument`
- Security: `IS_IMMORTAL(ch)` 
- `done` has special dual-mode behavior:
  - If `pEdit == NULL` (browsing categories): `edit_done(ch)` exits editor entirely
  - If `pEdit != NULL` (editing help entry): clears `pEdit` and stays in `ED_HELP` mode (returns to category browse)
- On success: updates `modified_by` and `modified` timestamp on help entry or category
- Fallthrough: `interpret(ch, arg)`

### G. Change/Save Model
- **Immediate metadata update**: On successful command, sets `modified_by = ch->name` and `modified = current_time`
- **No dirty flag within interpreter** — help is saved via `save_helpfiles_new()` in the global save cycle or explicit `asave` command
- Help data is saved to its own file format (not area-based)

### H. Permission Model
- Interpreter: `IS_IMMORTAL(ch)` required
- Individual commands have their own checks:
  - `hedit_builder`: requires `tot_level >= MAX_LEVEL - 1`
  - `hedit_security`: requires `tot_level >= MAX_LEVEL`
  - `hedit_delete`: requires `tot_level >= MAX_LEVEL - 4`
  - Many commands check `has_access_helpcat()` and/or `has_access_help()`
- Entry via `interp.c`: `POS_DEAD, L4, LOG_ALWAYS`

### I. Framework Features Used
**None**. Does not use `olc_set_editor()`, `olc_commands.h`, `olc_display.h`, or `olc_editor.h`.

---

## 3. Project Editor — `pedit`

### A. File & Location
- **Editor file**: `editors/projects/pedit.c` (445 lines)
- **Command table**: `olc.c` L206–219 (`pedit_table[]`)
- **Interpreter**: `olc.c` L791–838 (`void pedit()`)
- **Entry point**: `olc.c` L879–960 (`void do_pedit()`)

### B. Includes
```c
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
```
**Does NOT include**: `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### C. ED Constant
`ED_PROJECT` = 12 (olc.h L45)

### D. Command Table (`pedit_table[]` in olc.c)

| Command | Handler | Line (pedit.c) | Purpose |
|---------|---------|-----------------|---------|
| `?` | `show_help` | (olc.c) | Show help |
| `create` | `pedit_create` | L23 | Create new project |
| `show` | `pedit_show` | L57 | Display project properties |
| `name` | `pedit_name` | L155 | Set project name |
| `leader` | `pedit_leader` | L293 | Assign project leader |
| `area` | `pedit_area` | L370 | Toggle areas on project |
| `security` | `pedit_security` | L173 | Set security level (0-9) |
| `summary` | `pedit_summary` | L349 | Set one-line summary |
| `description` | `pedit_description` | L336 | Edit detailed description (string_append) |
| `pflag` | `pedit_pflag` | L193 | Toggle project flags |
| `builder` | `pedit_builder` | L219 | Toggle builders on project |
| `completed` | `pedit_completed` | L272 | Set completion percentage |

### E. Show Function (`pedit_show`, L57–153)
Displays: name, area(s), project leader, security, project flags, created date, summary, description, completion bar (colored progress), total building time, per-builder breakdown (name, time, date), project inquiries.

**Suggested tab structure**:
- **Tab 1 "Properties"**: name, leader, security, flags, summary, description
- **Tab 2 "Progress"**: completion status, areas, builders list, time tracking

### F. Interpreter (`pedit()` in olc.c L791)
- Standard pattern: `smash_tilde`, copy arg, `one_argument`
- Security: `get_staff_rank(ch) < STAFF_IMPLEMENTOR` check
- On `done`: `edit_done(ch)`
- Empty command: `pedit_show(ch, argument)`
- On success: sets `projects_changed = true`
- Fallthrough: `interpret(ch, arg)`

### G. Change/Save Model
- **Global flag**: `projects_changed = true`
- Also set within `pedit_create` itself
- Saved in global save cycle via `olc_save.c` when `projects_changed` is true

### H. Permission Model
- Both interpreter and entry point require `get_staff_rank(ch) >= STAFF_IMPLEMENTOR`
- `pedit_create`: additionally checks `tot_level >= MAX_LEVEL`
- `pedit_completed`: only project leader or `tot_level >= MAX_LEVEL` can adjust
- Entry via `interp.c`: `POS_DEAD, ML (MAX_LEVEL), LOG_ALWAYS`

### I. Framework Features Used
**None**. Does not use `olc_set_editor()`, `olc_commands.h`, `olc_display.h`, or `olc_editor.h`.

---

## 4. Command Editor — `cmdedit`

### A. File & Location
- **Editor file**: `editors/commands/cmdedit.c` (968 lines)
- **Command table**: `olc.c` L3261–3283 (`cmdedit_table[]`)
- **Interpreter**: `olc.c` L3326–3365 (`void cmdedit()`)
- **Entry point**: `olc.c` L3285–3324 (`void do_cmdedit()`)

### B. Includes
```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <math.h>
#include "../../strings.h"
#include "../../merc.h"
#include "../../interp.h"
#include "../../db.h"
#include "../../recycle.h"
#include "../../tables.h"
#include "../../bootstrap/bootstrap_internal.h"
#include "../../olc.h"
#include "../../olc_save.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../io/json/json_commands.h"
```
**Does NOT include**: `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### C. ED Constant
`ED_CMDEDIT` = 22 (olc.h L58)

### D. Command Table (`cmdedit_table[]` in olc.c)

| Command | Handler | Line (cmdedit.c) | Purpose |
|---------|---------|-------------------|---------|
| `?` | `show_help` | (olc.c) | Show help |
| `additional` | `cmdedit_additional` | L953 | Toggle additional command types |
| `commands` | `show_commands` | (olc.c) | List editor commands |
| `comments` | `cmdedit_comments` | L414 | Edit coder comments (string_append) |
| `create` | `cmdedit_create` | L331 | Create new command entry |
| `delete` | `cmdedit_delete` | L380 | Delete command (WIP/stub) |
| `description` | `cmdedit_description` | L401 | Edit command description (string_append) |
| `enabled` | `cmdedit_enabled` | L608 | Enable/disable command |
| `flags` | `cmdedit_flags` | L644 | Toggle command flags |
| `function` | `cmdedit_function` | L664 | Set/clear do_ function binding |
| `rank` | `cmdedit_rank` | L456 | Set required staff rank |
| `log` | `cmdedit_log` | L505 | Set log level |
| `name` | `cmdedit_name` | L384 | Set command name |
| `order` | `cmdedit_order` | L839 | Reorder command position in list |
| `position` | `cmdedit_position` | L530 | Set minimum position |
| `reason` | `cmdedit_reason` | L560 | Set/clear disabled reason |
| `sethelp` | `cmdedit_help` | L724 | Link help keywords |
| `show` | `cmdedit_show` | L365 | Display command properties |
| `summary` | `cmdedit_summary` | L815 | Set command summary |
| `type` | `cmdedit_type` | L430 | Set primary command type |

### E. Show Function (`cmdedit_show`, L365–377)
Displays: name, type, additional types, rank, position, log level, order, enabled status, disabled reason (if applicable), function name, help keywords (with hyperlink), summary, command flags, description, coders' comments.

**Suggested tab structure**:
- **Tab 1 "Properties"**: name, type, additional types, rank, position, log level, enabled, reason, function, order
- **Tab 2 "Documentation"**: help keywords, summary, description, comments

### F. Interpreter (`cmdedit()` in olc.c L3326)
- Standard pattern: `smash_tilde`, copy arg, `one_argument`
- On `done`: `edit_done(ch)`
- Empty command: `cmdedit_show(ch, argument)`
- On success: calls `save_commands()` immediately
- Fallthrough: `interpret(ch, arg)`
- **No security check in interpreter** (relies on entry point)

### G. Change/Save Model
- **Immediate save**: calls `save_commands()` after every successful command
- `save_commands()` calls `json_save_commands(COMMANDS_JSON_FILE)`
- Saves to JSON format

### H. Permission Model
- Entry point: `do_cmdedit()` has no explicit rank check but called from `interp.c` with `MAX_LEVEL`
- Entry via `interp.c`: `POS_DEAD, MAX_LEVEL, LOG_NORMAL`
- **Interpreter has no security check** — trusts that only MAX_LEVEL can enter

### I. Framework Features Used
- **Partially**: uses `olc_set_editor(ch, ED_CMDEDIT, command)` in both `do_cmdedit` and `cmdedit_create`
- Does NOT use `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### J. Additional Functions in File
- `get_cmd_data()` (L63): Find CMD_DATA by name prefix
- `do_func_lookup/name/display` via `FUNC_LOOKUPS` macro (L108–148)
- `save_command()` (L150): Write single command to file (legacy format, likely unused)
- `save_commands()` (L167): Save all commands to JSON
- `insert_command()` (L172): Append command to list
- `delete_command()` (L177): List delete callback
- `load_commands()` (L182): Load commands from JSON (or bootstrap from cmd_table)
- `do_cmdlist()` (L207): Staff command to list all commands with colors/links
- `do_cmdshow()` (referenced in olc.c L3367): Show command without entering editor

---

## 5. Social Editor — `socialedit`

### A. File & Location
- **Editor file**: `editors/socials/socialedit.c` (526 lines)
- **Command table**: `olc.c` L3423–3440 (`socialedit_table[]`)
- **Interpreter**: `olc.c` L3380–3420 (`void socialedit()`)
- **Entry point**: `editors/socials/socialedit.c` L29–78 (`void do_socialedit()`)

### B. Includes
```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <math.h>
#include "../../strings.h"
#include "../../merc.h"
#include "../../interp.h"
#include "../../db.h"
#include "../../recycle.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../olc_save.h"
#include "../../scripts.h"
#include "../../wilds.h"
```
**Does NOT include**: `olc_commands.h`, `olc_editor.h`, `olc_display.h`

### C. ED Constant
`ED_SOCIAL` = 26 (olc.h L63)

### D. Command Table (`socialedit_table[]` in olc.c)

| Command | Handler | Line (socialedit.c) | Purpose |
|---------|---------|----------------------|---------|
| `show` | `socialedit_show` | L80 | Display social properties |
| `commands` | `show_commands` | (olc.c) | List editor commands |
| `create` | `socialedit_create` | L134 | Create new social |
| `name` | `socialedit_name` | L179 | Rename social |
| `charnoarg` | `socialedit_char_no_arg` | L202 | Set "char sees (no arg)" message |
| `othersnoarg` | `socialedit_others_no_arg` | L225 | Set "others see (no arg)" message |
| `charfound` | `socialedit_char_found` | L248 | Set "char sees (target found)" message |
| `othersfound` | `socialedit_others_found` | L271 | Set "others see (target found)" message |
| `victfound` | `socialedit_vict_found` | L294 | Set "victim sees" message |
| `charnotfound` | `socialedit_char_not_found` | L317 | Set "char sees (target not found)" message |
| `charauto` | `socialedit_char_auto` | L340 | Set "char sees (self-target)" message |
| `othersauto` | `socialedit_others_auto` | L363 | Set "others see (self-target)" message |
| `delete` | `socialedit_delete` | L386 | Delete social from table |
| `list` | `socialedit_list` | L114 | List all socials |
| `save` | `socialedit_save` | L431 | Save social table to file |
| `?` | `show_help` | (olc.c) | Show help |

### E. Show Function (`socialedit_show`, L80–111)
Displays: name, char_no_arg, others_no_arg, char_found, others_found, vict_found, char_not_found, char_auto, others_auto — all 8 message strings.

**Suggested tab structure**: Single tab — all fields are social message strings.

### F. Entry Point (`do_socialedit`, L29–78)
**Unique**: The `do_socialedit` is defined in the editor file itself, not in olc.c. It handles:
- No argument: shows syntax help
- `list`: calls `socialedit_list()`
- `save`: calls `socialedit_save()`
- `create <name>`: calls `socialedit_create()`
- Otherwise: searches `social_table[]` by prefix match, sets `pEdit` and `editor = ED_SOCIAL`

### F2. Interpreter (`socialedit()` in olc.c L3380)
- Standard pattern: `smash_tilde`, copy arg, `one_argument`
- Security: `ch->pcdata->security < 9` check
- On `done`: `edit_done(ch)`
- Empty command: `socialedit_show(ch, argument)`
- On success: **no save/dirty flag set** — user must manually `save`
- Fallthrough: `show_help(ch, "socialedit")` (NOT `interpret(ch, arg)`)

### G. Change/Save Model
- **Explicit save only**: user must run `save` command within editor
- `socialedit_save` writes directly to `SOCIALS_FILE` in a custom text format
- **No dirty flag** — changes are lost if not explicitly saved
- `socialedit_delete` calls `edit_done(ch)` after deletion

### H. Permission Model
- Interpreter: requires `ch->pcdata->security >= 9`
- Entry via `do_socialedit`: no explicit security check (relies on interp.c)
- **Not registered in `interp.c` cmd_table** — socialedit is not in the grep results for interp.c. It's called through the `olc` command via `editor_table` (olc.c L129: `{ "social", do_socialedit }` or similar).
- Actually found via `editor_table` in olc.c (not directly in interp.c)

### I. Framework Features Used
**None**. Does not use `olc_set_editor()`, `olc_commands.h`, `olc_display.h`, or `olc_editor.h`.

---

## Summary Table

| Editor | Lines | ED_ Constant | Cmd Table Location | Interpreter Location | Entry Point Location | Framework Features | Save Model |
|--------|-------|-------------|-------------------|---------------------|---------------------|--------------------|------------|
| shedit | 903 | ED_SHIP (8) | boat.c L8271 | boat.c L8420 | boat.c L8482 | None | AREA_CHANGED + ships_changed |
| hedit | 1106 | ED_HELP (9) | olc.c L152 | olc.c L1575 | olc.c L1638 | None | Auto-save in cycle |
| pedit | 445 | ED_PROJECT (12) | olc.c L206 | olc.c L791 | olc.c L879 | None | projects_changed flag |
| cmdedit | 968 | ED_CMDEDIT (22) | olc.c L3261 | olc.c L3326 | olc.c L3285 | `olc_set_editor()` | Immediate save_commands() |
| socialedit | 526 | ED_SOCIAL (26) | olc.c L3423 | olc.c L3380 | socialedit.c L29 | None | Explicit save command only |

### Key Migration Observations

1. **shedit**: Cleanest candidate. Single data type, no dual modes, straightforward numeric/string/flag fields. Command table and interpreter are in boat.c (need to be moved). Has external function `can_edit_ships()`.

2. **hedit**: Most complex. Has dual-mode (category browser vs. help entry editor). The `done` command has special behavior. Category navigation commands (open/up/shift) don't map to standard field editing. Will require a custom sub-editor or navigation model.

3. **pedit**: Simplest editor. Small command set, single data type. Interpreter and entry point in olc.c. Clean candidate for migration.

4. **cmdedit**: Already partially migrated — uses `olc_set_editor()`. Contains extra non-editor functions (`load_commands()`, `save_commands()`, `do_cmdlist()`, FUNC_LOOKUPS macro) that should stay outside the editor. Immediate save pattern is unique.

5. **socialedit**: Has unique entry point pattern (in the editor file, not olc.c). Uses raw `social_table[]` array (not a linked list). Save is manual/explicit. Fallthrough goes to `show_help` instead of `interpret()`. All 8 string setter handlers are near-identical (candidate for `olc_cmd_string()`).

### Migration Priority (Suggested)
1. **pedit** — smallest, simplest, fewest edge cases
2. **shedit** — clean structure, just needs relocation of table/interpreter
3. **socialedit** — simple fields but unique entry point and save model
4. **cmdedit** — already partially migrated, but has extra functions to untangle
5. **hedit** — most complex due to dual-mode and category navigation
