# Technical Debt Tracker

**Last Updated:** February 26, 2026

This document tracks known technical debt in the Sentience codebase. Items are prioritized and linked to implementation plans where applicable.

**⚠️ SECURITY ALERT**: Vnum range tracking (top_vnum_*/bottom_vnum_*) from src_20_dev is vulnerable to DoS attacks. Use hash table iteration instead. See Item #15 below.

---

## Critical Priority

### 1. String Safety / C Hardening

**Status:** In Progress — audit complete, Phase 1 BUFFER hardening implemented (2026-02-26)
**Estimated Effort:** 6-8 weeks (phased)
**Risk:** Security vulnerabilities, buffer overflows, silent data corruption
**Detailed docs:**
- [PLAN_C_HARDENING.md](PLAN_C_HARDENING.md) — Full remediation roadmap (inspired by cURL/Stenberg practices)
- [AUDIT_UNSAFE_FUNCTIONS.md](AUDIT_UNSAFE_FUNCTIONS.md) — Per-file audit with counts and risk assessment

#### Audit Results (2026-02-26)

| Banned Function | Count | Safe Alternative | Adoption % |
|----------------|------:|-----------------|----------:|
| sprintf | 4,347 | snprintf (985) | 18.7% |
| strcpy | 336 | strlcpy (250) | 31.1% |
| strcat | 1,147 | strlcat (48) | 3.9% |
| atoi/atol | 1,178 | strtol (8) | 1.0% |

**Total: ~7,300 banned function calls across 114 files.**

Additionally, 56 functions exceed cyclomatic complexity 100 (cURL's threshold), worst being `script_varseton` at CC=693.

#### Solution Plan

Five-phase approach modeled on cURL's practices. See [PLAN_C_HARDENING.md](PLAN_C_HARDENING.md) for full details:

1. **Harden BUFFER system** — add length tracking, replace internal strcpy/strcat (1-2 days)
2. **Create safe helpers** — `sent_strlcpy`, `sent_snprintf`, `sent_parse_int` (2-3 days)
3. **Systematic migration** — file-by-file replacement of all banned functions (4-6 weeks)
4. **Complexity reduction** — decompose CC>100 functions (ongoing)
5. **CI enforcement** — banned function scanner, complexity gate, static analysis (1-2 days + ongoing)
- `src/script_*.c` - Script parsing (~40 instances)
- `src/string.c` - String utilities (~20 instances)

Medium-priority files (internal use):
- `src/db*.c` - Database loading
- `src/comm.c` - Network protocol
- `src/fight*.c` - Combat system
- Others (~60 instances)

#### Phase 1 Progress (BUFFER System)

Reference guide: [BUFFER System Guide](guides/BUFFER_SYSTEM_GUIDE.md)

- ✅ BUFFER internals moved to dedicated module: `src/utils/buffer.c` + `src/utils/buffer.h`
- ✅ `buf_type` now tracks length (`len`) for O(1) appends
- ✅ Removed internal `strcpy`/`strcat` usage in BUFFER paths (`add_buf`, `bprintf`)
- ✅ Added hard cap enforcement (`MAX_BUF_TOTAL`) and overflow-state behavior checks
- ✅ Added accessors: `buf_len`, `buf_capacity`, `buf_remaining`
- ✅ Added overflow arithmetic guards (`size_t` + `INT_MAX` boundaries)
- ✅ Added null-argument/invalid-buffer guards on BUFFER public API entry points
- ✅ Added modular unit coverage:
    - `buffer_core_unit_tests`
    - `buffer_permutation_unit_tests`

#### BUFFER Refactor Trade-offs (Old vs New)

- **Old (`mem.c` embedded):** lower short-term churn, but weaker safety guarantees and mixed concerns.
- **New (`utils/buffer.c` module):** stronger safety contract and maintainability, at cost of stricter failure behavior and slightly more code/test surface.
- **Net gain:** deterministic overflow/invalid-argument handling with targeted unit coverage.
- **Net loss/change:** previously permissive misuse patterns now fail fast and can generate guard logs.

#### Remaining BUFFER Hardening Work

- [~] Audit and fix unchecked `add_buf` / `add_buf_char` / `bprintf` call sites in high-risk paths (editor output and network-adjacent formatting paths first).
    - Completed first slice in:
        - `src/editors/areas/aedit.c` (regions list rendering)
        - `src/editors/blueprints/bpedit.c` (section list and channel list rendering)
        - `src/editors/commands/cmdedit.c` (`do_cmdlist` rendering)
        - `src/editors/dungeons/dngedit.c` (`dngedit_channel list`, `dngedit_floors list`)
        - `src/editors/dungeons/dngedit.c` (buffer builder success propagation in floors/levels/special exits)
        - `src/editors/dungeons/dngedit.c` (tab-level direct output paths: general/special/variables/notes)
        - `src/editors/dungeons/dngedit.c` (local list paths: levels weight list, grouped weight list, special room list)
        - `src/editors/dungeons/dngedit.c` (special exit list deduplicated to shared hardened builder)
        - `src/editors/dungeons/dngedit.c` (readability consolidation: helper-based weighted exit/floor/room/channel renderers; removed remaining `append_ok` plumbing)
        - `src/editors/blueprints/bsedit.c` (tab-level direct output paths: general/links/maze/notes)
        - `src/editors/blueprints/bpedit.c` (tab-level direct output paths: general/sections/layout/variables/notes)
- [ ] Standardize caller behavior on mutation failure (abort render, truncate with marker, or report to operator).
- [ ] Re-assess guard log volume after caller-side failure handling is in place.

    #### Caller-side hardening pattern (maintainability + readability)

    See the companion guidance in [PLAN_C_HARDENING.md](PLAN_C_HARDENING.md#caller-side-hardening-style-rule-maintainability--readability).

    Use this ordering for BUFFER call-site work:

    1. **Harden first**: ensure mutation calls fail closed and report a clear user-facing overflow message.
    2. **Consolidate second**: where list/table render logic repeats, extract local helpers and keep existing message text stable.
    3. **Reduce scaffolding**: prefer helper-owned append/cleanup/paging flow over long inline `append_ok` chains.
    4. **Constrain scope**: refactor one editor/file at a time to limit churn and review risk.

    This keeps security behavior intact while improving readability and long-term maintainability.

#### Success Criteria

- [ ] All user-input paths use safe functions
- [ ] ASan tests run clean
- [ ] No performance regression
- [ ] Documentation updated

#### References

- IMC module has `imcstrlcpy`/`imcstrlcat` implementations (though module is deprecated)
- BUFFER system (`new_buf`, `add_buf`) already exists for dynamic string building

---

## High Priority

### 2. Memory Management (DEFERRED)

**Status:** Investigation Complete - No Action Needed  
**Estimated Effort:** N/A (not worth migrating)  
**Risk:** Low

#### Investigation Results

Custom allocators (`alloc_perm`, `alloc_mem`, `str_dup`, `free_string`) were originally pool-based for performance. Analysis shows they're now just thin wrappers around standard `calloc()`/`free()`:

**From db.c:3945-4050:**
```c
void *alloc_perm(int sMem) {
    void *pMem;
    
    // Pool allocators disabled via #if 0
    pMem = calloc(1, sMem);  // Just calls calloc
    
    if (!pMem) {
        perror("Alloc_perm");
        exit(1);
    }
    
    return pMem;
}
```

**Recommendation:** Keep wrappers. Not worth the risk for minimal benefit.

#### str_dup Special Behavior

Note: `str_dup()` has special handling for read-only string pool:

```c
char *str_dup(const char *str) {
    if (str >= string_space && str < top_string)
        return (char *) str;  // Don't duplicate read-only strings
    
    // Normal duplication for dynamic strings
    str_new = alloc_mem(strlen(str) + 1);
    strcpy(str_new, str);
    return str_new;
}
```

This behavior would be lost with straight `strdup()` replacement.

#### Decision: DEFER

Keep custom allocators as thin wrappers. Focus on higher-value improvements.

---

## Medium Priority

### 2A. OLC Widevnum Parser Consolidation

**Status:** In Progress (qedit partially migrated)  
**Estimated Effort:** 1-2 weeks  
**Risk:** Medium (builder UX inconsistency, input parsing drift)

#### Problem

Some editor paths still use local/custom parsers for references, while others use shared widevnum parsing helpers. This creates inconsistent behavior and misleading prompts, especially for local-area shorthand such as `#<vnum>`.

Observed pattern:
- Shared parser path accepts `#<vnum>`, `<auid>#<vnum>`, area-name forms, and legacy bare vnums with context.
- Editor-local parsers often only accept `<auid>#<vnum>` or `<vnum>` and diverge over time.

#### Impact

- Builder commands behave differently between editors for equivalent input.
- Regression risk increases because fixes must be duplicated in multiple parser wrappers.
- Error/help text drifts away from actual accepted formats.

#### Consolidation Plan

1. Replace editor-local reference parsers with shared parser calls (`parse_widevnum` + `olc_relative_widevnum_context` where applicable).
2. Keep explicit compatibility cases only where syntax is domain-specific (e.g., `<stage_id>:<objective_id>` references).
3. Standardize help/error strings to list accepted forms consistently: `<auid>#<vnum>`, `#<vnum>`, and context-appropriate bare `<vnum>`.
4. Add/expand unit coverage for parser wrappers and editor reference entry points.

#### Current Note

- `qedit` has started this migration, but remaining editor wrappers should be audited and normalized.

---

### 3. Dead Code Removal

**Status:** Identified  
**Estimated Effort:** 1-2 weeks  
**Risk:** Low (just cleanup)

#### Known Dead Code

**imc.c - Intermud Chat System**
- ~2000+ lines of unused code
- Caused crashes, fully deprecated
- Has safe string implementations (`imcstrlcpy`/`imcstrlcat`) that could be extracted
- **Action:** Remove entire module, extract useful utilities first

**Other Candidates** (needs investigation):
- Unused skill/spell systems?
- Deprecated command handlers?
- Old area formats?

#### Removal Plan

**Week 1: Audit**
```bash
# Find functions never called
cscope -d -L3 function_name | wc -l  # 0 = unused

# Find files not in Makefile
grep "\.o" Makefile | cut -d. -f1 | sort > compiled.txt
ls *.c | cut -d. -f1 | sort > all_files.txt
comm -23 all_files.txt compiled.txt  # Uncompiled files
```

**Week 2: Remove**
- Extract useful utilities from imc.c
- Remove dead modules
- Update Makefile/CMakeLists.txt
- Verify build still works

---

### 4. Reset System Modernization

**Status:** Not Started  
**Estimated Effort:** 6-9 weeks  
**Risk Level:** High (touches core game mechanics, affects all content)  
**Dependencies:** Should be done AFTER widevnum migration (Phase 8 complete)  
**Reason for Priority:** Inflexible system causing content creators to use script workarounds

#### Problem Description

The current reset system is **very old and inflexible**, dating back to the original DikuMUD architecture. It only handles basic static operations:

**Current Reset Commands (from merc.h lines 5545-5555):**
```c
/*
 * Reset commands:
 *   '*': comment
 *   'M': read a mobile
 *   'O': read an object
 *   'P': put object in object
 *   'G': give object to mobile
 *   'E': equip object to mobile
 *   'D': set state of door
 *   'R': randomize room exits
 *   'S': stop (end of list)
 */
```

**Current RESET_DATA structure:**
```c
struct reset_data {
    RESET_DATA *next;
    char command;      // Single-character command
    long arg1;         // Vnum or value
    long arg2;         // Count/limit
    long arg3;         // Location
    long arg4;         // Min count
};
```

**Key Limitations:**

1. **No Conditional Logic:**  
   - Cannot check game state (city under siege, time of day, weather, etc.)
   - Cannot vary spawns based on player actions or world events
   - No support for variables or dynamic parameters

2. **No Script Integration:**  
   - Resets are completely separate from the scripting engine
   - TRIG_REPOP fires AFTER mob/object is created (can't prevent spawn)
   - TRIG_RESET fires AFTER room resets (can't modify reset behavior)

3. **Inflexible Spawn Logic:**  
   - Fixed counts (arg2=max, arg4=min)
   - No weighted randomization
   - No spawn groups or alternatives
   - No level scaling based on area state

4. **Workarounds Everywhere:**  
   User reports: "It has ended up leading to a lot of npcs, rooms, and objects with scripts that fire when they populate to do things more flexibly."
   - Content creators use TRIG_REPOP scripts to modify mobs after spawn
   - Rooms use TRIG_RESET to manually add/remove content
   - Objects use complex script logic to compensate for limited reset options

**Example from db.c lines 1806-1851 (Maze hardcoding):**  
The reset system can't handle dynamic pneuma drops, so it's hardcoded in C:
```c
if (!str_cmp(pMob->in_room->area->name, "Maze-Level1")) {
    i = number_range(1,2);
    for (c = 0; c < i; c++) {
        obj = create_object(get_obj_index(get_reserved_vnum("obj_pneuma_item")), 1, false);
        obj_to_char(obj, pMob);
    }
}
// Repeated for Maze-Level2 through Maze-Level5
```

This should be data-driven, not hardcoded.

#### Proposed Solution

**Modernize the reset system to be script-aware and game-state-aware.**

**Phase A: Conditional Reset System (3-4 weeks)**

1. **Extend RESET_DATA structure:**
   ```c
   struct reset_data {
       RESET_DATA *next;
       char command;
       WNUM arg1;              // Now uses widevnum
       long arg2;
       long arg3;
       long arg4;
       
       // NEW: Conditional execution
       char *condition_script; // Optional script expression
       int weight;             // For weighted randomization
       char *spawn_group;      // Group identifier for alternatives
   };
   ```

2. **Add Conditional Evaluation:**
   - Before executing reset, evaluate `condition_script`
   - Expression can check:
     - Area variables: `$area.under_siege == true`
     - Time: `$game.hour >= 18 && $game.hour <= 6` (night guards)
     - Weather: `$weather.condition == 'rain'`
     - Player count: `$area.nplayer > 5`
   - If condition fails, skip reset

3. **Weighted Spawn Groups:**
   - Multiple resets with same `spawn_group` name = alternatives
   - One chosen based on `weight` value
   - Example: 70% chance normal guard, 30% chance elite guard

**Phase B: Script-Integrated Resets (2-3 weeks)**

1. **New Trigger: TRIG_PRERELOAD (pre-spawn check):**
   - Fires BEFORE mob/object creation
   - Can return false to prevent spawn
   - Can modify reset parameters dynamically
   - Example: Room script checks if city gate is open before spawning merchants

2. **New Trigger: TRIG_RESETPREPARE (area-level):**
   - Fires BEFORE area resets
   - Sets area variables for conditional resets
   - Example: Count nearby enemy mobs, set $area.threat_level

3. **Dynamic Parameters:**
   - Allow `arg2` (count) to reference variables: `$area.guard_count`
   - Allow `arg1` (vnum) to be chosen from lists: `$area.current_invader_mob`

**Phase C: Blueprint/Dungeon Integration (1-2 weeks)**

1. **New Reset Command: 'B' (Blueprint):**
   - Spawns from blueprint definitions
   - Supports BSTYPE_MAZE for procedural generation
   - Replaces hardcoded maze spawning

2. **Instance-Aware Resets:**
   - Already partially implemented (instance_count_mob checks)
   - Extend to use instance/dungeon variables
   - Example: Dungeon floor 5 has different spawns than floor 1

#### Files Affected

**Core Reset System:**
- `/sentience/src/db.c` (reset_room function, lines 1720-2020)
- `/sentience/src/merc.h` (RESET_DATA structure, lines 5560-5570)
- `/sentience/src/mem.c` (new_reset_data, line 1325)

**OLC Editors:**
- `/sentience/src/editors/rooms/redit.c` (mreset, oreset commands)
- `/sentience/src/olc.c` (reset command, lines 2190-2380)
- `/sentience/src/olc_save.c` (save reset data to .are files)

**Scripting Integration:**
- `/sentience/src/scripts.c` (add TRIG_PRERELOAD, TRIG_RESETPREPARE)
- `/sentience/src/script_const.c` (trigger definitions)
- `/sentience/src/script_rpcmds.c` (room prog commands)

**Area Loading:**
- `/sentience/src/db2.c` (load_resets function)

**Example Usage (After Implementation):**

```
#RESETS
* Conditional guard spawning based on siege state
M 0 923#101 2 923#1001 1 if($area.under_siege)      ; Elite guards during siege
M 0 923#100 5 923#1001 1 if(!$area.under_siege)     ; Normal guards otherwise

* Weighted merchant spawns (70% common, 30% rare)
M 0 923#200 1 923#1005 1 weight(70) group(merchant) ; Common merchant
M 0 923#201 1 923#1005 1 weight(30) group(merchant) ; Rare merchant

* Time-based spawns
M 0 923#150 3 923#1010 1 if($game.hour >= 6 && $game.hour < 20) ; Day workers
M 0 923#151 2 923#1010 1 if($game.hour >= 20 || $game.hour < 6)  ; Night watch

S
```

#### Migration Plan

**Week 1-2: Core Conditional System**
- Extend RESET_DATA with condition_script, weight, spawn_group
- Implement condition parsing using existing script expression parser
- Add weighted randomization logic
- Update new_reset_data() allocator

**Week 3-4: OLC Integration**
- Update redit_mreset, redit_oreset to accept conditions
- Add syntax: `mreset <vnum> <max> <min> if(<condition>)`
- Add syntax: `mreset <vnum> <max> <min> weight(<value>) group(<name>)`
- Update display_resets() to show conditions

**Week 5-6: Script Triggers**
- Implement TRIG_PRERELOAD (pre-spawn)
- Implement TRIG_RESETPREPARE (area-level)
- Add dynamic parameter evaluation ($area.variable)
- Update p_percent_trigger calls in db.c

**Week 7: Blueprint Integration**
- Add 'B' reset command for blueprints
- Integrate with blueprint/dungeon system
- Remove hardcoded maze spawning from db.c

**Week 8-9: Testing & Content Migration**
- Comprehensive testing of all reset types
- Convert existing workaround scripts to new system
- Performance testing (conditional evaluation overhead)
- Update documentation

#### Success Criteria

- [ ] Conditional resets working (if statements evaluate correctly)
- [ ] Weighted spawn groups function (probabilities correct)
- [ ] TRIG_PRERELOAD can prevent spawns
- [ ] Dynamic parameters ($area.variable) resolve correctly
- [ ] OLC editors support new syntax
- [ ] All existing reset types still work (backward compatible)
- [ ] Blueprint integration removes hardcoded spawning
- [ ] Performance: <5ms overhead per conditional reset

#### Risks

1. **Backward Compatibility:** Old .are files must still load  
   *Mitigation:* Make new fields optional, default to old behavior

2. **Performance:** Evaluating conditions for every reset  
   *Mitigation:* Cache parsed condition scripts, optimize hot paths

3. **Content Complexity:** Builders might create unmaintainable conditionals  
   *Mitigation:* Provide clear examples, limit expression complexity

4. **Debugging:** Hard to diagnose why something didn't spawn  
   *Mitigation:* Add debug logging for condition evaluation failures

#### Related Tech Debt Items

- **Item #1 (String Safety):** Will touch same files, coordinate changes
- **Item #6 (Maze Generation):** Blueprint system integration overlaps
- **Widevnum Migration (Phase 3):** RESET_DATA.arg1 becomes WNUM

---

### 16. War System Naming Review

**Status:** Identified
**Estimated Effort:** 1 day
**Risk:** Low (terminology cleanup)

#### Problem

The automated war system in `/sentience/src/autowar.c` uses terminology from the early 2000s that may be problematic:

- **AUTO_WAR_GENOCIDE** - Race vs race battle mode
- **AUTO_WAR_JIHAD** - Good vs evil alignment battle mode

These names were common in MUD development of that era but are insensitive by modern standards.

#### Proposed Solution

Rename the war types to more neutral terminology:

| Current Name | Proposed Name | Description |
|-------------|---------------|-------------|
| `AUTO_WAR_GENOCIDE` | `AUTO_WAR_RACIAL` or `AUTO_WAR_KINSTRIFE` | Race vs race |
| `AUTO_WAR_JIHAD` | `AUTO_WAR_CRUSADE` or `AUTO_WAR_ALIGNMENT` | Good vs evil |

**Files Affected:**
- `src/autowar.c` - War type handling
- `src/tables.c` - `auto_war_table[]` definitions
- `src/merc.h` - War type constants
- Any player-facing help files

#### Implementation

1. Update constants in `merc.h`
2. Update `auto_war_table[]` name strings
3. Update any help files referencing war types
4. Update any player-facing messages

#### Success Criteria

- [ ] No problematic terminology in code
- [ ] No problematic terminology in player-facing text
- [ ] Existing functionality preserved
- [ ] Help files updated

---

## Low Priority

### 5. Index/Runtime Split for Rooms

**Status:** Deferred to Phase 9+  
**Estimated Effort:** 6-8 weeks  
**Risk:** Medium (architectural change)

Currently rooms use a hybrid model. Objects and mobiles have clean separation:
- `OBJ_INDEX_DATA` - Template (loaded from area files)
- `OBJ_DATA` - Runtime instance (spawned in game)

Rooms should follow the same pattern but currently mix both concerns.

**See:** ARCHITECTURE_CHANGES_ANALYSIS.md for full details

**Decision:** Wait until after widevnums are stable. Not required for widevnum functionality.

---

### 6. Maze Generation System Replacement

**Status:** Identified  
**Estimated Effort:** 3-4 weeks  
**Risk:** Medium (gameplay-affecting)

#### Problem

The `/sentience/maze` directory contains a standalone C program (`maze.c`) that generates `Maze-Level1-5.are` files. This system has several issues:

**Current Implementation:**
- Standalone program that generates static area files
- Should run on game boot (but has problems)
- Generates malformed `.are` files frequently
- Not integrated with game logic
- Hardcoded references throughout codebase:
  ```c
  // db.c - Hardcoded pneuma drops for Maze-Level1-5
  if (!str_cmp(pMob->in_room->area->name, "Maze-Level1"))
  
  // magic_holy.c - Exorcism spell references
  if (victim->tot_level <= LEVEL_HERO/5) area = find_area("Maze-Level1");
  
  // magic_astral.c - Maze spell
  if (!(area = find_area("Maze-Level1")))
  
  // fight.c, chat_rooms.c, act_obj2.c - Various checks
  if (!str_prefix("Maze-Level", ch->in_room->area->name))
  ```

**Problems:**
- File generation happens externally (not part of game)
- Generated `.are` files often malformed
- No dynamic regeneration during gameplay
- Maintenance burden (separate codebase)
- Not compatible with JSON area migration (Phase 3)

#### Solution: Blueprint/Dungeon System (from src_20_dev)

The src_20_dev codebase has a comprehensive procedural dungeon system that can replace static maze generation:

**Blueprint System Components:**

1. **BLUEPRINT_SECTION (merc.h:6261-6295)**
   ```c
   struct blueprint_section_data {
       long vnum;
       char *name;
       
       char type;  // BSTYPE_STATIC or BSTYPE_MAZE
       
       // For static sections:
       long lower_vnum;
       long upper_vnum;
       
       // For maze sections:
       int maze_x, maze_y;            // Maze dimensions
       LLIST *maze_templates;         // Room templates to use
       LLIST *maze_fixed_rooms;       // Fixed rooms at specific coords
       LLIST *maze_weighted_rooms;    // Weighted random room selection
   };
   ```

2. **BLUEPRINT (merc.h:6297-6362)**
   ```c
   struct blueprint_data {
       long vnum;
       char *name;
       
       int mode;  // BLUEPRINT_MODE_STATIC or BLUEPRINT_MODE_DYNAMIC
       
       LLIST *sections;  // List of BLUEPRINT_SECTION
       
       // Entry/exit points
       long entry_vnum;
       long exit_vnum;
   };
   ```

3. **INSTANCE (merc.h:6364-6457)**
   ```c
   struct instance_data {
       long uid;              // Unique instance ID
       BLUEPRINT *blueprint;  // Template this was created from
       
       LLIST *players;        // Players currently in instance
       LLIST *rooms;          // Runtime rooms (dynamically generated)
       LLIST *mobs;           // Spawned mobs
       LLIST *objects;        // Spawned objects
       
       time_t created;
       time_t expires;
       
       LLIST *variables;      // Instance-specific variables
   };
   ```

4. **DUNGEON (merc.h:6459-6611)**
   ```c
   struct dungeon_data {
       long vnum;
       char *name;
       
       LLIST *floors;         // List of INSTANCE (one per floor)
       LLIST *players;        // All players across all floors
       
       int current_floor;
       int max_floors;
       
       // Progression tracking
       int mobs_killed;
       int bosses_defeated;
       bool completed;
   };
   ```

**Maze Generation Algorithm (blueprint.c:1475-1819):**

The src_20_dev implementation uses depth-first maze carving:

```c
// Simplified from blueprint.c
void generate_maze_section(BLUEPRINT_SECTION *section, INSTANCE *instance) {
    int x, y;
    int cells[maze_x][maze_y];
    STACK *stack = new_stack();
    
    // 1. Initialize all cells as walls
    for (x = 0; x < section->maze_x; x++)
        for (y = 0; y < section->maze_y; y++)
            cells[x][y] = CELL_WALL;
    
    // 2. Start at random cell
    int start_x = number_range(0, section->maze_x - 1);
    int start_y = number_range(0, section->maze_y - 1);
    cells[start_x][start_y] = CELL_PATH;
    stack_push(stack, start_x, start_y);
    
    // 3. Depth-first carving
    while (!stack_empty(stack)) {
        int cur_x, cur_y;
        stack_peek(stack, &cur_x, &cur_y);
        
        // Find unvisited neighbors
        DIRECTION *unvisited = get_unvisited_neighbors(cells, cur_x, cur_y);
        
        if (unvisited) {
            // Carve path to random neighbor
            DIRECTION dir = choose_random(unvisited);
            carve_path(cells, cur_x, cur_y, dir);
            stack_push(stack, new_x, new_y);
        } else {
            stack_pop(stack);  // Backtrack
        }
    }
    
    // 4. Create rooms from cells
    for (x = 0; x < section->maze_x; x++) {
        for (y = 0; y < section->maze_y; y++) {
            if (cells[x][y] == CELL_PATH) {
                ROOM_INDEX_DATA *room = create_maze_room(section, x, y);
                list_add(instance->rooms, room);
            }
        }
    }
    
    // 5. Place fixed rooms at specific coordinates
    for (LLIST_NODE *node = section->maze_fixed_rooms->head; node; node = node->next) {
        MAZE_FIXED_ROOM *fixed = node->data;
        replace_room_at(instance, fixed->x, fixed->y, fixed->room_vnum);
    }
}
```

**Features:**
- Depth-first carving creates natural maze paths
- Weighted room templates for variety
- Fixed rooms at specific coordinates (start, boss, treasure)
- Configurable dimensions (can have 5x5, 10x10, 15x15, etc.)
- Dynamic generation at runtime (no static files)

#### Migration Plan

**Week 1: Backport Blueprint Infrastructure**
- Copy structures from src_20_dev/merc.h
- Add BLUEPRINT, BLUEPRINT_SECTION, INSTANCE, DUNGEON to legacy merc.h
- Create blueprint.c, dungeon.c in src/
- Update mem.c with new allocators

**Week 2: Maze Generation Algorithm**
- Port generate_maze_section() from src_20_dev/blueprint.c
- Implement depth-first carving algorithm
- Add room template system
- Test generation (can output to console/logs)

**Week 3: OLC Integration**
- Create bpedit (blueprint editor) command
- Create bsedit (blueprint section editor) command
- Allow configuring maze dimensions, templates, fixed rooms
- Save blueprints to JSON format

**Week 4: Runtime Integration**
- Integrate with existing instance system (if any, or create minimal version)
- Hook into reset system (new 'B' command)
- Replace Maze-Level1-5 references with blueprint calls
- Remove hardcoded pneuma drops (use reset system instead)

**Week 5: Testing & Cleanup**
- Remove `/sentience/maze` directory
- Remove static Maze-Level1-5.are files
- Test dynamic generation
- Validate pneuma drops still work
- Test exorcism/maze spells

#### Files Affected

**To Remove:**
- `/sentience/maze/` (entire directory)
- `/sentience/area/Maze-Level1.are` through `Maze-Level5.are`

**To Update:**
- `src/db.c` - Pneuma drops (lines 1806-1851)
- `src/magic_holy.c` - Exorcism spell (lines 198-202)
- `src/magic_astral.c` - Maze spell (line 120)
- `src/fight.c` - Combat restrictions (line 6916)
- `src/chat_rooms.c` - Chat restrictions (line 171)
- `src/act_obj2.c` - Object restrictions (lines 534-538)
- `src/olc_save.c` - Area save (line 377)
- `src/db2.c` - Reset generation (line 1564)

**To Backport from src_20_dev:**
- `src/blueprint.c` - Maze generation algorithm
- `src/dungeon.c` - Dungeon management
- `src/editors/blueprint.c` - OLC editor
- `src/mem.c` - new_blueprint(), new_instance(), etc.

#### Success Criteria

- [ ] Maze generation happens dynamically at runtime
- [ ] No static `.are` files for mazes
- [ ] Blueprints stored in JSON
- [ ] Mazes can be regenerated/reset without restart
- [ ] OLC editors allow configuring maze parameters
- [ ] All spells/commands work with new system
- [ ] Performance acceptable (<1 second generation time)

#### References

- src_20_dev/blueprint.c:1475-1819 - Maze generation algorithm
- src_20_dev/dungeon.c - Dungeon management
- src_20_dev/merc.h:6261-6611 - Structure definitions

---

## Backlog

### 7. Bootstrap System for New Deployments

**Status:** Identified  
**Estimated Effort:** 2-3 weeks  
**Risk:** Low (tooling improvement)

#### Problem

Currently, setting up a new Sentience instance requires:
- Manual creation of directory structure (`accounts/`, `characters/`, `data/`, `logs/`, etc.)
- Copying/creating essential data files (races, settings, base areas)
- Risk of missing required files causing boot failures
- No standardized way to distribute a "starter pack"

New deployments face:
- **Missing Structure:** Boot errors if `data/races/` doesn't exist
- **Empty Data:** No areas to explore, no races to choose from
- **Configuration Overhead:** Must manually create all JSON files
- **Inconsistent Setup:** Each deployment slightly different

#### Proposed Solution

Create a bootstrap system with two modes:

**Mode 1: Minimal Local Bootstrap**
Generate essential files for development/testing:

```bash
./sent --bootstrap-minimal

# Creates:
# - Directory structure (accounts/, characters/, data/races/, etc.)
# - data/races/human.json (single playable race)
# - data/game_settings.json (default configuration)
# - area/school.are (tutorial area)
# - data/help/*.json (basic help files)
```

**Mode 2: Package Download Bootstrap**
Download complete content package from URL:

```bash
./sent --bootstrap-url https://example.com/sentience-starter.tar.gz

# Downloads and extracts:
# - Complete directory structure
# - Full race collection (20+ races)
# - Starting areas (city, wilderness, dungeons)
# - Help system
# - Default configuration
```

#### Implementation Details

**Command-Line Options:**
```c
// comm.c - Main boot sequence
if (argc > 1) {
    if (!strcmp(argv[1], "--bootstrap-minimal")) {
        bootstrap_minimal_deployment();
        printf("Bootstrap complete. Run './sent' to start server.\n");
        exit(0);
    }
    
    if (!strcmp(argv[1], "--bootstrap-url") && argc > 2) {
        bootstrap_from_url(argv[2]);
        printf("Bootstrap complete. Run './sent' to start server.\n");
        exit(0);
    }
}
```

**Minimal Bootstrap Structure:**
```c
void bootstrap_minimal_deployment(void) {
    printf("Creating minimal deployment structure...\n");
    
    // 1. Create directory structure
    create_directories();
    
    // 2. Generate essential files
    generate_minimal_race_data();      // data/races/human.json
    generate_default_settings();       // data/game_settings.json
    generate_tutorial_area();          // area/school.are (or school.json)
    generate_basic_help();             // data/help/*.json
    generate_reserved_vnums();         // data/reserved_vnums.json
    
    // 3. Create empty collections
    generate_empty_accounts_dirs();    // accounts/a/ through accounts/z/
    generate_empty_character_dirs();   // characters/a/ through characters/z/
    
    printf("✓ Directory structure created\n");
    printf("✓ Human race available\n");
    printf("✓ Tutorial area created (vnum 3000-3099)\n");
    printf("✓ Basic help files generated\n");
}

void create_directories(void) {
    const char *dirs[] = {
        "accounts", "characters", "data", "logs", "area",
        "data/races", "data/classes", "data/help", "data/world",
        NULL
    };
    
    for (int i = 0; dirs[i]; i++) {
        mkdir_p(dirs[i]);  // Recursive mkdir
    }
    
    // Create letter subdirs for accounts/characters
    for (char c = 'a'; c <= 'z'; c++) {
        char path[256];
        sprintf(path, "accounts/%c", c);
        mkdir(path, 0755);
        sprintf(path, "characters/%c", c);
        mkdir(path, 0755);
    }
}

void generate_minimal_race_data(void) {
    FILE *fp = fopen("data/races/human.json", "w");
    if (!fp) {
        perror("Cannot create data/races/human.json");
        exit(1);
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"human\",\n");
    fprintf(fp, "  \"uid\": 1,\n");
    fprintf(fp, "  \"name\": \"Human\",\n");
    fprintf(fp, "  \"description\": \"Humans are versatile and adaptable.\",\n");
    fprintf(fp, "  \"playable\": true,\n");
    fprintf(fp, "  \"starting\": true,\n");
    fprintf(fp, "  \"size\": 2,\n");  // SIZE_MEDIUM
    fprintf(fp, "  \"stats\": { \"str\": 0, \"int\": 0, \"wis\": 0, \"dex\": 0, \"con\": 0 }\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
}

void generate_tutorial_area(void) {
    FILE *fp = fopen("area/school.are", "w");
    if (!fp) {
        perror("Cannot create area/school.are");
        exit(1);
    }
    
    // Generate minimal tutorial area with 1 room
    fprintf(fp, "#AREA\n");
    fprintf(fp, "school.are~\n");
    fprintf(fp, "The School of Sentience~\n");
    fprintf(fp, "{{BUILDER}} None~\n");
    fprintf(fp, "{{AREA_VERSION}} 1~\n");
    fprintf(fp, "1 50 0 0\n\n");  // Levels 1-50
    
    fprintf(fp, "#ROOMS\n");
    fprintf(fp, "#3001\n");
    fprintf(fp, "The School Courtyard~\n");
    fprintf(fp, "You stand in a peaceful courtyard.\n~\n");
    fprintf(fp, "0 0 0 0 0 0\n");
    fprintf(fp, "S\n\n");
    
    fprintf(fp, "#0\n\n");
    fprintf(fp, "#$\n");
    
    fclose(fp);
    
    // Add to area.lst
    fp = fopen("area/area.lst", "w");
    if (fp) {
        fprintf(fp, "school.are\n");
        fprintf(fp, "$\n");
        fclose(fp);
    }
}
```

**URL Download Bootstrap:**
```c
void bootstrap_from_url(const char *url) {
    char cmd[4096];
    
    printf("Downloading bootstrap package from %s...\n", url);
    
    // Download with curl/wget
    snprintf(cmd, sizeof(cmd), "curl -L -o /tmp/sentience-bootstrap.tar.gz '%s'", url);
    if (system(cmd) != 0) {
        printf("Error: Download failed. Trying wget...\n");
        snprintf(cmd, sizeof(cmd), "wget -O /tmp/sentience-bootstrap.tar.gz '%s'", url);
        if (system(cmd) != 0) {
            printf("Error: Both curl and wget failed. Install one and retry.\n");
            exit(1);
        }
    }
    
    printf("✓ Download complete\n");
    printf("Extracting package...\n");
    
    // Extract to current directory
    snprintf(cmd, sizeof(cmd), "tar xzf /tmp/sentience-bootstrap.tar.gz");
    if (system(cmd) != 0) {
        printf("Error: Extraction failed\n");
        exit(1);
    }
    
    printf("✓ Bootstrap package extracted\n");
    printf("✓ %d areas available\n", count_area_files());
    printf("✓ %d races available\n", count_race_files());
    
    unlink("/tmp/sentience-bootstrap.tar.gz");
}
```

#### Package Format

**Bootstrap packages should be tarball with structure:**
```
sentience-bootstrap/
├── accounts/           (empty letter dirs)
├── characters/         (empty letter dirs)
├── data/
│   ├── races/         (20+ race JSON files)
│   ├── classes/       (class definitions)
│   ├── help/          (help system)
│   ├── game_settings.json
│   └── reserved_vnums.json
├── area/
│   ├── school.are     (tutorial)
│   ├── city.are       (starting city)
│   ├── wilderness.are
│   └── area.lst
└── README.md          (deployment notes)
```

**Hosting Options:**
- GitHub releases (official packages)
- Community-hosted mirrors
- Private deployments can host custom packages

#### Files Affected

**New Files:**
- `src/bootstrap.c` - Bootstrap implementation
- `src/bootstrap.h` - Bootstrap functions
- `scripts/create_bootstrap_package.sh` - Package creation tool

**Modified Files:**
- `src/comm.c` - Add command-line option parsing
- `src/Makefile` - Add bootstrap.c to build
- `src/CMakeLists.txt` - Add bootstrap.c to build

#### Migration Plan

**Week 1: Minimal Bootstrap**
1. Create `bootstrap.c` with minimal bootstrap functions
2. Implement directory creation
3. Implement basic file generation (human.json, tutorial area)
4. Add command-line parsing to comm.c
5. Test minimal bootstrap

**Week 2: Package Download Bootstrap**
1. Implement URL download function
2. Add tarball extraction
3. Add validation (check required files exist)
4. Test with sample package

**Week 3: Package Creation & Documentation**
1. Create `scripts/create_bootstrap_package.sh`
2. Generate official starter package
3. Test package deployment
4. Write documentation (BOOTSTRAP.md)
5. Update README.md with bootstrap instructions

#### Testing Strategy

**Minimal Bootstrap Tests:**
```bash
# Fresh checkout
cd /tmp
git clone <repo> sentience-test
cd sentience-test
./src/build
./src/install

# Run bootstrap
./sent --bootstrap-minimal

# Verify structure created
ls -la accounts/  # Should have a-z dirs
ls -la data/races/  # Should have human.json
cat data/races/human.json  # Valid JSON?
cat area/area.lst  # Should list school.are

# Try to boot
./sent  # Should start without errors
```

**Package Bootstrap Tests:**
```bash
# Test with hosted package
./sent --bootstrap-url https://example.com/sentience-starter.tar.gz

# Verify contents
ls -la area/  # Should have multiple .are files
ls -la data/races/  # Should have 20+ races
```

#### Success Criteria

- [ ] `--bootstrap-minimal` creates working deployment
- [ ] Minimal bootstrap can boot server successfully
- [ ] Human race available for character creation
- [ ] Tutorial area accessible
- [ ] `--bootstrap-url` downloads and extracts package
- [ ] Package bootstrap includes full race/area collection
- [ ] Package creation script generates valid tarball
- [ ] Documentation complete (BOOTSTRAP.md)
- [ ] README.md updated with bootstrap instructions

#### Benefits

✓ **Easy Setup:** New deployments in one command  
✓ **Consistency:** All deployments have same base structure  
✓ **Testing:** Minimal bootstrap for CI/CD testing  
✓ **Distribution:** Official packages for stable releases  
✓ **Customization:** Custom packages for private deployments

#### Risks

- **Low Risk:** Pure tooling improvement, no game logic changes
- **Minimal Impact:** Only affects initial deployment

---

### 11. Instance Persistence Across Reboots

**Status:** Partially Implemented  
**Estimated Effort:** 2-3 weeks (if extension needed)  
**Risk:** Medium (affects active instances)

#### Problem

Active instances (dungeons, boats, custom areas) may need to survive reboots:
- **Dungeons**: Players mid-dungeon would lose progress on reboot
- **Boats**: Ship positions/state not preserved
- **Dynamic Instances**: Any runtime-generated areas lost

**Current State** (from code inspection):
- `save_instances()` and `load_instances()` exist in [db.c:8222-8350](../src/db.c#L8222-L8350)
- Saves to `INSTANCES_FILE` (instances.dat)
- Handles dungeons, ships, and standalone instances
- **Instances have flags**: `INSTANCE_NO_SAVE`, `INSTANCE_DESTROY`
- **Dungeons have flags**: `DUNGEON_NO_SAVE`, `DUNGEON_DESTROY`

**Code Evidence:**
```c
bool save_instances()
{
    // Save dungeons (skip if DUNGEON_NO_SAVE or DUNGEON_DESTROY)
    iterator_start(&it, loaded_dungeons);
    while( (dungeon = (DUNGEON *)iterator_nextdata(&it)) )
    {
        if( !IS_SET(dungeon->flags, (DUNGEON_NO_SAVE|DUNGEON_DESTROY)) )
            dungeon_save(fp, dungeon);
    }
    
    // Save ships
    iterator_start(&it, loaded_ships);
    while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
        ship_save(fp, ship);
    
    // Save standalone instances (skip if instance is part of dungeon/ship)
    iterator_start(&it, loaded_instances);
    while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
    {
        if( !IS_VALID(instance->dungeon) && !IS_VALID(instance->ship) && 
            !IS_SET(instance->flags, (INSTANCE_NO_SAVE|INSTANCE_DESTROY)) )
            instance_save(fp, instance);
    }
}
```

**What's Already Implemented:**
- ✓ `instance_save()` / `instance_load()` in [blueprint.c:2346+](../src/blueprint.c#L2346)
- ✓ `dungeon_save()` / `dungeon_load()` in [dungeon.c:2052+](../src/dungeon.c#L2052)
- ✓ `ship_save()` / `ship_load()` in [boat.c:1620+](../src/boat.c#L1620)
- ✓ Multi-room persistence via `persist_save_room()` in [db.c:6142+](../src/db.c#L6142)
- ✓ Instance sections saved (with all rooms, mobs, objects)
- ✓ Player ownership tracking (survives if owners reconnect)
- ✓ Idle timers (cleanup after inactivity)

#### Potential Gaps

**Need to Verify:**
1. **Complete Room State**: Does `persist_save_room()` save all room contents?
   - Mobs with inventory? ✓ (seen in code)
   - Objects on floor? ✓ (seen in code)
   - Room scripts/variables? ✓ (seen in code)
   - Exits/doors? ✓ (seen in code)

2. **Instance-Specific Data**:
   - Special rooms (boss rooms, treasure rooms)?
   - Special exits (one-way portals, etc.)?
   - Instance flags and state?
   - Floor progression in multi-level dungeons?

3. **Player Reconnection**:
   - Are players restored to instance on reconnect?
   - What if instance was destroyed during downtime?
   - Handle orphaned instances (no owners)?

4. **Blueprint Instances**:
   - Procedurally generated mazes?
   - Dynamic room layouts?
   - Are templates saved or regenerated?

#### Investigation Plan

**Week 1: Audit Current Implementation**
1. Test dungeon save/load across reboot
2. Test ship save/load across reboot
3. Verify all room state persists (mobs, objects, variables)
4. Check player restoration to instances
5. Test orphaned instance cleanup

**Week 2: Gap Analysis**
1. Document any missing persistence
2. Identify edge cases (mid-boss fight, locked doors, etc.)
3. Performance test (large dungeons with many rooms)
4. Validate instance flag handling

**Week 3: Implement Fixes (if needed)**
1. Add missing persistence fields
2. Improve player reconnection handling
3. Add validation/recovery for corrupted instances
4. Update documentation

#### Success Criteria

- [ ] Dungeons survive reboot with all state intact
- [ ] Ships survive reboot at correct position/heading
- [ ] Players reconnect to their active instances
- [ ] Multi-room instances (100+ rooms) save/load < 5 seconds
- [ ] Orphaned instances clean up properly
- [ ] No data corruption on failed saves
- [ ] Instance flags respected (NO_SAVE, DESTROY)

#### Benefits

✓ **Player Experience**: No progress lost on reboots  
✓ **Reliability**: Can reboot without kicking dungeon groups  
✓ **Testing**: Easier to test instances (can save/restore state)  
✓ **Debugging**: Can inspect saved instance data offline

#### Risks

- **Medium Risk**: Data corruption could lose player progress
- **Mitigation**: Atomic writes with backups, validation on load

#### Dependencies

- **Related to**: Item #6 (Blueprint/Maze Generation) - procedural instances
- **Requires**: JSON persistence complete (already done)
- **Benefits from**: Widevnum (cleaner vnum management in instances)

---

### 12. Testing Infrastructure

**Status:** Planned for Phase 1 (Week 4)  
**Framework:** Criterion + AddressSanitizer  
**See:** TESTING_STRATEGY.md

**Minimal approach:**
- Week 4 of Phase 1: Set up Criterion framework
- Write tests ONLY for new widevnum functions
- Full testing backfill after Phase 8 complete

---

### 9. Auxiliary Data File Formats

**Status:** Integrated into Widevnum Plan  
**Timeline:** Phase 4 (Week 15)

Migrate old binary/text formats to JSON:
- `mail.dat` → `data/mail/mail.json`
- `notes.txt` → `data/notes/*.json`
- `bans.dat` → `data/bans.json`
- `socials.dat` → `data/socials.json`

**See:** ARCHITECTURE_CHANGES_ANALYSIS.md, Section 4

---

### 10. Editor Modernization

**Status:** Integrated into Widevnum Plan  
**Timeline:** Phase 6 (Weeks 21-24)

Legacy has superior modular editor structure. Extract widevnum changes from src_20_dev's monolithic `olc_act.c` and port to modular files.

**No action needed** - handled during widevnum migration.

---

## Security Issues

### 15. Vnum Range Tracking Vulnerability

**Status:** Documented, DO NOT IMPLEMENT  
**Estimated Effort:** N/A (prevention)  
**Risk:** Denial of Service

#### Problem

The src_20_dev `top_vnum_*` / `bottom_vnum_*` tracking creates a **critical DoS vulnerability** exploitable by malicious builders:

```c
// Attack: Create sparse vnums
redit 1              // Room vnum 1
redit 2000000000     // Room vnum 2,000,000,000

// Result: Vnum searches iterate 2 BILLION vnums for 2 rooms
for (vnum = 1; vnum <= 2000000000; vnum++) {
    if ((room = get_room_index(vnum)))  // Server hangs
}
```

**Why it's unfixable:**
- Can't validate vnum gaps (legitimate areas might be sparse)
- Can't restrict vnum choices (defeats widevnum purpose)
- Any two vnums create a range that must be scanned
- Requires only basic builder access

#### Solution

**Use hash table iteration instead:**

```c
// SECURE - Iterates actual entities (O(thousands) not O(billions))
void do_vnum_room(CHAR_DATA *ch, char *argument) {
    ROOM_INDEX_DATA *room;
    int hash, found = 0;
    
    for (hash = 0; hash < MAX_KEY_HASH; hash++) {
        for (room = room_index_hash[hash]; room; room = room->next) {
            if (target_area && room->area != target_area)
                continue;
            if (is_name(argument, room->name))
                found++;
        }
    }
}
```

#### Implementation

- **DO NOT** backport `top_vnum_*` / `bottom_vnum_*` fields from src_20_dev
- Use hash table iteration for all vnum searches (Phase 4)
- Update: `do_vnum_obj()`, `do_vnum_mob()`, `do_vnum_room()` in `act_info.c`
- Update: OLC validation functions in `olc_act.c`, `olc_save.c`

#### Success Criteria

- [ ] No `top_vnum_*` / `bottom_vnum_*` fields in `AREA_DATA`
- [ ] All vnum search functions use hash iteration
- [ ] Test: Sparse vnum areas don't cause slowdown
- [ ] Documentation updated

#### References

- [WIDEVNUM_BACKPORT_ANALYSIS.md](WIDEVNUM_BACKPORT_ANALYSIS.md#performance--security-critical-notes) - Detailed analysis
- [WIDEVNUM_IMPLEMENTATION_PLAN.md](WIDEVNUM_IMPLEMENTATION_PLAN.md#vnum-search-security) - Implementation guide

---

## Completed

### ✓ Custom Allocators Investigation
**Completed:** January 27, 2026  
**Result:** Already using `calloc()` underneath, no migration needed

### ✓ Redis Integration
**Completed:** Prior to widevnum work  
**Result:** Operational, async disk writer working

### ✓ JSON Character/Account Migration
**Completed:** Prior to widevnum work  
**Result:** All player data uses JSON, Redis caching operational

---

## How to Use This Document

**Adding New Debt:**
1. Add to appropriate priority section
2. Include: Status, Effort, Risk, Problem, Solution, Success Criteria
3. Link to related documents if applicable

**Claiming Work:**
1. Update Status from "Identified" to "In Progress"
2. Add your name and start date
3. Create feature branch: `tech-debt/issue-name`

**Completing Work:**
1. Move to "Completed" section
2. Add completion date and outcome
3. Update any references in other documents

---

## Tech Debt Metrics

**Total Items:** 16
**Critical:** 1 (String Safety)
**High:** 1 (Memory Management - Deferred)
**Medium:** 3 (Dead Code, Reset System, War System Naming)
**Low:** 2 (Index/Runtime Split, Maze Generation)
**Security:** 1 (Vnum Range Tracking - Prevention)
**Backlog:** 5 (Bootstrap System, Instance Persistence, Testing, Data Formats, Editors)
**Completed:** 3

**Estimated Total Effort:** 32-46 weeks
**Prioritized for Next 6 Months:** String Safety (4-6 weeks), Reset System (6-9 weeks)
**Nice to Have (Low Priority):** Bootstrap System (2-3 weeks)
