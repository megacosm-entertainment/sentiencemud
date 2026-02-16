# Script System Analysis & Improvement Plan

## Overview

The scripting system (`scripts.h`, `scripts.c`, `script_*.c`) is a large,
feature-rich engine allowing area builders to write programs (mprogs, oprogs,
rprogs, tprogs, aprogs, iprogs, dprogs) that control game behavior. It
includes entity expansion (e.g. `$(enactor.leader.mount.short)`), compiled
bytecode execution, if-checks, variables, and a command system.

**Total script system size: ~70,700 lines across 14 files.**

| File                | Lines  | Purpose                              |
|---------------------|-------:|--------------------------------------|
| `scripts.h`         | 3,231  | Core types, enums, entity fields     |
| `scripts.c`         | 9,095  | Engine runtime, trigger dispatch     |
| `script_expand.c`   | 7,835  | Entity expansion / dot-notation      |
| `script_mpcmds.c`   | 8,452  | Mob script commands (163 entries)    |
| `script_tpcmds.c`   | 7,688  | Token script commands (156 entries)  |
| `script_opcmds.c`   | 7,399  | Obj script commands (155 entries)    |
| `script_rpcmds.c`   | 6,903  | Room script commands (147 entries)   |
| `script_commands.c` | 6,359  | Shared/consolidated commands (~55)   |
| `script_ifc.c`      | 5,179  | If-check implementations             |
| `script_vars.c`     | 3,789  | Variable system                      |
| `script_comp.c`     | 2,426  | Script compiler                      |
| `script_const.c`    | 1,910  | Constant tables & entity field defs  |
| `script_lua.c`      |   382  | Lua bridge (minimal)                 |
| `script_cmds.c`     |    21  | Stub                                 |

---

## Architecture

### Script Compilation (`script_comp.c`)

Scripts are compiled from source text to bytecode. Entity references like
`$(enactor.leader.mount.short)` are parsed into a chain of byte codes:

1. **Primary entity** — e.g. `ENTITY_ENACTOR` (byte)
2. **Field codes** — each field is an `unsigned char code` value from
   the `ENT_FIELD` table, written as `*p++ = ftype->code`
3. **Terminator** — `ESCAPE_END` (0x01)

### Entity Field Encoding: The 128-Entry Limit

Each entity type (mobile, object, room, etc.) has an `ENT_FIELD[]` table
mapping field names to byte codes. The critical constraint:

```c
struct entity_field_type {
    char *name;
    unsigned char code;    // <-- SINGLE BYTE, range 0x00-0xFF
    unsigned char type;    // Result entity type
};
```

Field codes start at `ESCAPE_EXTRA = 0x05` and the single-letter escape
codes (`$i`, `$n`, etc.) occupy `0x80-0xB9`:

| Range       | Usage                           | Available |
|-------------|--------------------------------|-----------|
| 0x00-0x04   | Reserved escape codes          | —         |
| 0x05-0x7F   | Entity field codes             | **123**   |
| 0x80-0xB9   | Single-letter escapes ($A-$z)  | —         |
| 0xBA-0xFF   | Unused                         | 70        |

**Current usage per entity type:**

| Entity Type   | Fields Used | Headroom (to 0x80) |
|---------------|-------------|-------------------|
| `mobile`      | 76          | 47                |
| `object`      | 57          | 66                |
| `room`        | 32          | 91                |
| `game`        | 27          | 96                |
| `exit`        | 17          | 106               |
| `token`       | 15          | 108               |
| `church`      | 15          | 108               |
| `area`        | 8           | 115               |
| `conn`        | 7           | 116               |

**The mobile enum is the most constrained at 76/123 entries**, but still has
significant headroom. The real issue was that specific sub-categories (like
equipment slots under mob) were consuming field slots individually. This was
solved by introducing sub-entities like `ENT_EQUIPMENT`, where
`$(mob.eq.light)` traverses mob → equipment sub-entity → slot lookup,
instead of having `ENTITY_MOB_EQ_LIGHT`, `ENTITY_MOB_EQ_BODY`, etc. as
individual field codes.

### Entity Expansion (`script_expand.c`)

Entity expansion is a multi-pass system:

1. **Primary lookup** — Resolves `$(enactor)`, `$(victim)`, etc. from
   `entity_primary[]` table
2. **Field chaining** — Each `.field` in the chain looks up the next
   `ENT_FIELD` entry and resolves to a new entity type
3. **Type dispatch** — Large switch on entity type
   (`ENT_MOBILE`, `ENT_OBJECT`, etc.) determines how to read the field

The system supports deep chaining: `$(enactor.leader.mount.room.name)` works
by resolving each segment to a new entity, then looking up the next field
in that entity's field table.

### If-Check System (`script_ifc.c`, `script_const.c`)

If-checks are boolean/numeric tests used in script conditionals:

```
if ispc($n) and level($n) > 10
```

| Aspect       | Details                                |
|-------------|----------------------------------------|
| Total checks | ~300+ (`CHK_MAXIFCHECKS`)             |
| Storage      | `ifcheck_enum` — plain `int` values    |
| No byte limit| Unlike entities, ifchecks use `int`    |
| Lookup       | Linear scan of `ifcheck_table[]`       |

### Variable System (`script_vars.c`)

Scripts can store typed variables on entities. Variable types include
booleans, integers, strings, and entity references (mob, obj, room, token,
area, wilds, church, etc.). Variables can be saved persistently.

### Script Commands

Commands are the imperative actions scripts can perform (e.g. `mob transfer`,
`mob damage`, `mob echo`). Each script type has its own command table:

| Table              | Source File        | Entries |
|--------------------|--------------------|---------|
| `mob_cmd_table[]`  | `script_mpcmds.c`  | 163     |
| `token_cmd_table[]`| `script_tpcmds.c`  | 156     |
| `obj_cmd_table[]`  | `script_opcmds.c`  | 155     |
| `room_cmd_table[]` | `script_rpcmds.c`  | 147     |
| `area_cmd_table[]` | `script_commands.c`| 32      |
| `instance_cmd_table[]` | `script_commands.c` | 33  |
| `dungeon_cmd_table[]`  | `script_commands.c` | 33  |

---

## Problem #1: Massive Command Duplication

### The Scale

Of the ~160 commands across mob/obj/room/token tables:
- **~136 commands appear in ALL FOUR** tables
- **~55 already consolidated** via shared `scriptcmd_*` functions
- **~81 remain duplicated** with type-specific `do_mp*`/`do_op*`/`do_rp*`/`do_tp*` implementations

**Estimated 12,000-14,000 lines (~40-45%) of duplicated code** across the
four type-specific files.

### Duplication Patterns

#### Pattern 1: Trivial Guard + Delegation (~7 commands × 4 = 28 functions)

The only difference is the entity null-check:

```c
// script_mpcmds.c
SCRIPT_CMD(do_mpvarset) {
    if(!info || !info->mob || !info->var) return;
    script_varseton(info, info->var, argument, arg);
}

// script_opcmds.c
SCRIPT_CMD(do_opvarset) {
    if(!info || !info->obj || !info->var) return;
    script_varseton(info, info->var, argument, arg);
}
```

Applies to: `varset`, `varclear`, `varcopy`, `varsave`, `varsaveon`,
`varseton`, `varclearon`

#### Pattern 2: Different Room Resolution + Identical Logic (~15 commands)

The implementations are ~70 lines each, differing only in:
1. Guard check (`info->mob` vs `info->obj` vs `info->room` vs `info->token`)
2. Room resolution (`info->mob->in_room` vs `obj_room(info->obj)` vs
   `info->room` vs `token_room(info->token)`)
3. Log prefix (`"MpTransfer"` vs `"OpTransfer"` etc.)

Applies to: `transfer`, `goto`, `gtransfer`, `force`, `gforce`, `vforce`,
`echoroom`, `echoaround`, `echogrouparound`, `echogroupat`, `echoleadaround`,
`echoleadat`, `echonotvict`, `echobattlespam`, `echochurch`, `zecho`, `gecho`

#### Pattern 3: Massive Identical Switch Statements (~5 commands)

Commands with 200-500+ line switch statements duplicated 4 times:

| Command       | Lines/copy | Total waste |
|---------------|-----------|-------------|
| `altermob`    | ~500      | ~2,000      |
| `alterexit`   | ~290      | ~1,160      |
| `addaffect`   | ~170      | ~680        |
| `addaffectname` | ~170    | ~680        |
| `alteraffect` | ~150      | ~600        |

### Commands Unique to One Type

| Command       | Type    | Notes                           |
|--------------|---------|----------------------------------|
| `appear`     | mob     | mob becomes visible              |
| `assist`     | mob     | mob assists in combat            |
| `chargemoney`| mob     | charges money directly           |
| `disappear`  | mob     | mob becomes invisible            |
| `hunt`       | mob     | mob hunts a target               |
| `kill`       | mob     | mob attacks someone              |
| `take`       | mob     | mob takes an item                |
| `teleport`   | mob     | teleport mob                     |
| `adjust`     | token   | token adjusts values             |
| `castfailure`| token   | handles cast failure             |
| `castrecover`| token   | handles cast recovery            |
| `give`       | token   | token gives items                |

---

## Problem #2: Entity Field Code Constraints

While the current 123-entry limit per entity type is not immediately
exhausted, the `unsigned char code` encoding creates structural constraints:

1. **Fragile byte ordering** — Compiled scripts store field codes as raw
   bytes. Adding/reordering enum values invalidates all compiled scripts.
   New fields must be **appended** to preserve compatibility.

2. **No namespacing** — All fields for an entity type share one flat
   byte-code space. Sub-entities (like object typed data) help, but each
   sub-entity also has its own 123-entry limit.

3. **Object sub-entities grow with item types** — Every new item type needs
   a new `ENT_OBJ_*` entity type, a new `entity_obj_*_enum`, and a new
   `entity_obj_*[]` field table. Currently ~30 sub-entity types for objects.

4. **Linear lookup** — `entity_type_lookup()` does `O(n)` string comparison
   for every field access in script compilation.

---

## Problem #3: Ifcheck Sprawl

The if-check system has ~300+ entries in a single enum. While not
byte-constrained (uses `int`), it has scalability issues:

1. **Linear scan** — `ifcheck_lookup()` iterates the entire table
2. **Type flags per check** — Each check declares which script types can
   use it via bitflags (`IFC_M|IFC_O|IFC_R|IFC_T`)
3. **Mixed concerns** — The same table contains entity property queries
   (`level`, `race`), game state tests (`sunlight`, `hour`), and complex
   multi-parameter checks

---

## Comparison with src_20_dev Branch

The dev branch (`/sentience/src_20_dev/`) evolved the script system
significantly. Key differences:

### Overall Size

| Metric                  | Current    | Dev Branch |
|------------------------|-----------|------------|
| Total script lines     | 70,668    | 86,481     |
| `script_commands.c`    | 6,359     | **15,264** |
| `scripts.h` enums      | 3,231     | 3,905      |
| `script_expand.c`      | 7,835     | 11,274     |
| `scriptcmd_*` refs (shared) | 62 (mob) | 89 (mob) |

### Features Worth Back-Porting

#### 1. Enhanced Command Consolidation (HIGH PRIORITY)

The dev branch moved **50+ additional commands** to shared `scriptcmd_*`
implementations:

- `scriptcmd_altermob` — consolidated mob alteration (was 4× ~500 lines)
- `scriptcmd_alterexit` — consolidated exit alteration
- `scriptcmd_alter` — generic alter framework
- `scriptcmd_addspell`, `scriptcmd_remspell` — spell management
- `scriptcmd_settitle`, `scriptcmd_setposition` — player state
- `scriptcmd_lockset` — lock management
- Various trigger helpers (`scriptcmd_acttrigger`, `scriptcmd_greettrigger`, etc.)
- `scriptcmd_multitype`, `scriptcmd_addtype`, `scriptcmd_remtype` — object multi-typing
- `scriptcmd_shop`, `scriptcmd_shop_stock` — shop management
- `scriptcmd_reassign` — entity reassignment

This cut the type-specific files from ~30,442 total to ~30,234 while adding
more features, because shared logic grew from 6,359 to 15,264 lines.

#### 2. ENT_FIELD Description Metadata (MEDIUM PRIORITY)

```c
// Dev branch struct
struct entity_field_type {
    char *name;
    unsigned char code;
    unsigned char type;
    char *description;    // NEW: Human-readable field description
    bool deprecated;      // NEW: Deprecation marker
};
```

Entity field tables include descriptions:
```c
{"short", ENTITY_MOB_SHORT, ENT_STRING, "Short description"},
{"room",  ENTITY_OBJ_ROOM,  ENT_ROOM,  "Room description"},
```

This enables self-documenting scripting and in-game help for script authors.

#### 3. Table-Based Entity Sub-Types (MEDIUM PRIORITY)

The dev branch introduced compact entity types that decode indexed values:

- `ENT_SEX_STRING_TABLE` — `TABLE.neuter`, `TABLE.male`, `TABLE.female`
- `ENT_STATS_TABLE` — `TABLE.strength`, `TABLE.dexterity`, etc.
- `ENT_VITALS_TABLE` — `TABLE.hp`, `TABLE.mana`, `TABLE.move`

This pattern reduces field count by encoding indexed lookups as parameterized
types rather than individual entity fields.

#### 4. Wider Ifcheck Return Type (LOW PRIORITY)

```c
// Current
DECL_IFC_FUN(x) bool x (..., int *ret, ...)

// Dev branch
DECL_IFC_FUN(x) bool x (..., long *ret, ...)
```

Using `long` for ifcheck return values avoids overflow for large numeric
comparisons (e.g. timestamps, large gold amounts).

#### 5. Instance/Dungeon Extended Ifcheck Params (LOW PRIORITY)

Dev branch added `INSTANCE *instance` and `DUNGEON *dungeon` parameters
to the ifcheck function signature, enabling direct access without
variable-based workarounds.

#### 6. New Ifchecks (VARIES)

29 new ifchecks in dev branch not present in current:

| Category          | Checks                                              |
|-------------------|-----------------------------------------------------|
| Type queries      | `isbook`, `iscontainer`, `isfluidcontainer`, `isfood`, `isfurniture`, `islight`, `ismoney`, `isportal` |
| Object state      | `objmaxrepairs`, `objrepairs`, `objval8`, `objval9` |
| Validation        | `isvalid`, `isvaliditem`, `iswnum`, `wnumvalid`    |
| Class/Church      | `haschurch`, `hasclass`, `isclass`                  |
| Missions          | `mission`, `onmission`, `totalmissions`             |
| Reputation        | `hasreputation`, `hasfaction`                       |
| Misc              | `gc`, `number`, `savage`, `tempstore5`              |

#### 7. Massive New Entity Fields (+467 new ENTITY_ values)

The dev branch added 467 new entity field values, including:

- **Area regions** — `ENTITY_AREA_REGION_*` (region system)
- **Missions** — `ENTITY_MISSION_*`, `ENTITY_MISSION_PART_*`
- **Reputation** — `ENTITY_REPUTATION_*`, `ENTITY_REPINDEX_*`
- **Object sub-entities** — Renamed `ENT_OBJ_*` → `ENT_OBJECT_*` for
  consistency
- **Adornments** — `ENTITY_ADORNMENT_*` (cosmetic attachments)
- **Waypoints** — `ENTITY_WAYPOINT_*`
- **Blueprints** — `ENTITY_BLUEPRINT_*`
- **Shops** — `ENT_SHOP`, `ENT_SHOP_STOCK`, `ENT_SHOP_BUYTYPES`
- **Weapon attacks** — `ENT_WEAPON_ATTACKS`, `ENT_WEAPON_ATTACK`
- **Armor protections** — `ENT_ARMOR_PROTECTIONS`

### Features NOT Ported (Intentionally)

- IMC (Inter-MUD Communication) — removed in current branch
- Mission system — separate backport planned
- Reputation system — separate backport planned
- `gmon.out` / profiling artifacts

---

## Recommended Improvement Roadmap

### Phase 1: Command Consolidation (High Impact, Low Risk)

**Goal:** Reduce ~12,000 lines of duplication to shared implementations.

1. **Add a `SCRIPT_VARINFO` helper for "get current room":**
   ```c
   ROOM_INDEX_DATA *script_info_room(SCRIPT_VARINFO *info);
   ```
   Returns `info->mob->in_room`, `obj_room(info->obj)`, `info->room`, or
   `token_room(info->token)` depending on which entity is set.

2. **Add a "valid entity" guard helper:**
   ```c
   bool script_info_valid(SCRIPT_VARINFO *info);
   ```
   Returns true if the controlling entity (mob/obj/room/token) exists.

3. **Migrate Pattern 1 commands** (trivial wrappers): Convert `varset`,
   `varclear`, etc. to single shared function with entity-type-agnostic guard.

4. **Migrate Pattern 2 commands** (room-resolution variants): `transfer`,
   `goto`, `echoroom`, `echoaround`, `force`, etc.

5. **Migrate Pattern 3 commands** (large switch statements): `altermob`,
   `alterexit`, `alteraffect`, `addaffect`, `addaffectname`.

**Estimated savings: ~10,000–12,000 lines removed.**

### Phase 2: Entity Field Metadata (Medium Impact, Low Risk)

1. **Add `description` field** to `ENT_FIELD` struct (as dev did)
2. **Add `deprecated` bool** to mark legacy fields
3. **Populate descriptions** in `script_const.c`
4. **Add in-game `scripthelp` command** that queries field tables to show
   available fields, their types, and descriptions

### Phase 3: Encoding Modernization (High Impact, Medium Risk)

1. **Widen `code` to `uint16_t`** — Doubles available field space to 65K
   per entity type. Requires:
   - Updating `script_comp.c` to write 2 bytes per field code
   - Updating `script_expand.c` to read 2 bytes per field code
   - **Full recompile of all scripts** (one-time migration)
   - Changing `ESCAPE_*` defines to use 2-byte patterns

2. **Hash-based field lookup** — Replace `entity_type_lookup()`'s linear
   scan with hash table for O(1) compilation performance.

3. **Consider removing single-letter escapes** (`$i`, `$n`) — These are
   legacy DikuMUD patterns that consume the 0x80-0xB9 range. If they can
   be deprecated, the full 0x05-0xFF range (251 entries) becomes available
   even without widening to `uint16_t`.

### Phase 4: Table-Based Entity Patterns (Medium Impact, Low Risk)

Back-port the dev branch pattern of indexed table entities:
- `ENT_STATS_TABLE` → `$(mob.stats.strength)` resolves by index
- `ENT_SEX_STRING_TABLE` → `$(mob.sexstr.male)` resolves by sex index
- `ENT_VITALS_TABLE` → `$(mob.vitals.hp)` resolves by vital index

This pattern avoids consuming one entity field per stat/vital and is
extensible without code changes.

### Phase 5: Ifcheck Improvements (Low Impact, Low Risk)

1. **Widen return type** to `long` (as dev did)
2. **Add instance/dungeon params** to ifcheck signature
3. **Port useful new ifchecks** from dev: type-query checks (`isbook`,
   `iscontainer`, etc.), validation checks (`isvalid`, `isvaliditem`),
   and `hasclass`/`haschurch`

---

## Risk Assessment

| Change                      | Risk   | Impact | Notes                        |
|-----------------------------|--------|--------|------------------------------|
| Command consolidation       | Low    | High   | Pure refactor, testable      |
| ENT_FIELD description       | Low    | Medium | Additive change              |
| Code field widening         | Medium | High   | Requires script recompile    |
| Hash-based lookup           | Low    | Low    | Performance improvement      |
| Table entity patterns       | Low    | Medium | New sub-entity types         |
| Ifcheck signature changes   | Medium | Low    | Touches all ifcheck impls    |

---

## Performance Analysis

### Execution Pipeline Overview

A script's lifetime has two phases with very different performance profiles:

1. **Compilation** (once per edit/load) — text → bytecode
2. **Runtime** (every trigger fire) — bytecode interpretation + entity expansion

Runtime dominates total CPU cost because triggers fire constantly during
gameplay. A single `act()` call can fire `p_act_trigger` against every mob,
object, and token in the room.

### Hot Path #1: Trigger Dispatch

**This is the single most performance-critical path in the entire system.**

When a game event occurs (speech, movement, combat, act messages), the engine
must find and execute matching scripts. The flow is:

```
Game Event (e.g. act())
  → p_act_trigger() / p_percent_trigger() / p_greet_trigger()
    → test_string_trigger() / test_number_trigger()
      → For each entity (mob/obj/room) in scope:
        → For each token on entity:           ← nested iteration
          → For each prog on token[slot]:     ← inner loop
            → is_trigger_type() check
            → Pattern match (substr/exact)
            → execute_script() if matched
        → For each prog on entity[slot]:
          → Same checks + execution
```

**Key performance characteristics:**

| Aspect                | Current Implementation       | Complexity  |
|-----------------------|-----------------------------|-------------|
| Trigger slot lookup   | `trigger_table[type].slot`  | O(1) — array index |
| Prog list per slot    | `progs[slot]` — LLIST per TRIGSLOT | O(1) lookup, O(n) iterate |
| Trigger type check    | `is_trigger_type(tindex, type)` — integer compare | O(1) |
| String matching       | `match_substr` / `match_exact_name` | O(n×m) worst case |
| Entity scope scan     | Iterate room people + contents + tokens | O(entities × tokens) |

**TRIGSLOT optimization**: Programs are bucketed into 15 trigger slots
(`TRIGSLOT_GENERAL` through `TRIGSLOT_ANIMATE`). This means a TRIG_SPEECH
event only iterates `progs[TRIGSLOT_SPEECH]`, not all programs. This is a
major win — most entities have programs in only 1-3 slots.

**Bottleneck: greet triggers**. `p_greet_trigger` calls `p_location_trigger`
which iterates ALL mobs in the room (for MPROG), ALL objects on the char +
in the room + carried by other mobs (for OPROG). In crowded rooms with many
scripted entities, this becomes quadratic: O(mobs × tokens_per_mob).

**Call frequency** (static call site count, indicating relative frequency):

| Trigger Function        | Call Sites | Notes                           |
|------------------------|------------|---------------------------------|
| `p_percent_trigger`    | 433        | Most common — fires everywhere  |
| `execute_script`       | 102        | Direct calls (bypassing trigger)|
| `p_give_trigger`       | 22         | Item/gold transactions          |
| `p_act_trigger`        | 17         | Every `act()` message           |
| `p_greet_trigger`      | 13         | Every room entry × 3 prog types|

`p_act_trigger` appears at only 17 call sites but fires with extreme
frequency because `act()` is called for virtually every visible game action.
Each `act()` call iterates all people in the room, triggering
`p_act_trigger` for each mob+obj+room.

### Hot Path #2: Entity Expansion (`expand_string` / `expand_argument`)

Entity expansion runs on **every script line that contains `$()` references,
`$i/$n` codes, or `[expressions]`**. This is the second hottest path.

**Call frequency**: `expand_argument` has 1,159 call sites; `expand_string`
has 115.

**The expansion pipeline:**

```
expand_string() / expand_argument()
  → For each escape in the bytecode string:
    → ESCAPE_ENTITY: expand_argument_entity()
      → Loop: switch on arg->type (131 cases)
        → expand_entity_mobile() — 371-line switch on byte code
        → expand_entity_object() — 367 lines
        → expand_entity_room() — 209 lines
        → (etc., one function per entity type)
    → ESCAPE_EXPRESSION: expand_argument_expression()
    → ESCAPE_VARIABLE: expand_argument_variable()
    → ESCAPE_UA..ESCAPE_LZ: expand_argument_simple_code()
```

**Performance characteristics:**

| Aspect                      | Implementation              | Cost        |
|-----------------------------|-----------------------------|-------------|
| Entity field byte dispatch  | `switch(*str)` on byte code | O(1) — jump table |
| Entity type dispatch        | `switch(arg->type)` — 131 cases | O(1) — jump table |
| Sub-entity chaining         | Loop until ESCAPE_END       | O(chain_depth) |
| String building             | `add_buf()` to BUFFER       | O(output_len) |

**The good**: Runtime entity expansion is very efficient. Compiled bytecodes
store field codes as raw bytes, so expansion is direct switch dispatch — no
string comparisons at runtime, no hash lookups. The chain
`$(enactor.leader.mount.short)` compiles to 4 bytes and expands through
4 jump-table dispatches.

**The bad**: Every `expand_argument` call does `malloc(strlen(str) + 1)` and
`free(buf)` for a temporary buffer — even when the argument is a pure entity
reference or number that doesn't need it. This is a needless allocation on
every single argument parse.

**The worse**: Every command execution (`opc_mob`, `opc_obj`, etc.) calls
`new_script_param()` which calls `alloc_mem()` → `calloc()` plus
`new_buf()` → `malloc()`. Then immediately `free_script_param()` →
`free()` + `free()`. **Two heap allocations and frees per script line
executed.** For a 50-line script this means 100 malloc/free cycles.

### Hot Path #3: Command Dispatch

Command dispatch is already well-optimized:

```
opc_mob(block)    ← opcode_table[] indexed by pre-compiled opcode
  → mob_cmd_table[block->cur_line->param].func()  ← pre-compiled index
```

Both lookups are O(1) array indexing — the compiler pre-resolves command
names to integer indices. No string comparison at runtime.

**No performance issues here.** The 4× command duplication wastes binary
size and I-cache but has zero effect on dispatch speed since each is a
direct function pointer call.

### Hot Path #4: If-Check Evaluation

Boolean expressions are pre-compiled into a tree (`BOOLEXP`) with short-
circuit evaluation. The ifcheck index is stored as a pre-compiled integer.

```
opc_if(block)
  → boolexp_evaluate(block, boolexp_tree)
    → ifcheck_comparison(info, param_index, rest, arg)
      → ifcheck_table[param_index].func(...)   ← O(1) indexed
```

**Runtime is O(1)** — the compiler resolves ifcheck names to integer indices.
The recursive tree evaluation adds O(clauses) overhead for complex boolean
expressions but this is inherent and unavoidable.

### Compilation-Time Costs (One-Time)

These only run when a builder edits a script, not during gameplay.

| Operation                  | Current Implementation        | Cost            |
|----------------------------|-------------------------------|-----------------|
| Entity field name lookup   | `entity_type_lookup()` — linear scan of ENT_FIELD[] | O(n) per field |
| If-check name lookup       | `ifcheck_lookup()` — linear scan of 424 entries | O(424) per check |
| Command name lookup        | `mpcmd_lookup()` — linear scan of ~160 entries | O(160) per command |
| Trigger name lookup        | Linear scan of `trigger_table[]` (215 entries) | O(215) per trigger |

**Entity field lookup table sizes** (elements per linear scan):

| Table             | Entries | Average scan cost |
|-------------------|---------|-------------------|
| `entity_mobile`   | 78      | ~39 comparisons   |
| `entity_object`   | 63      | ~32               |
| `entity_game`     | 40      | ~20               |
| `entity_room`     | 36      | ~18               |
| `entity_exit`     | 22      | ~11               |
| `entity_token`    | 16      | ~8                |
| `entity_church`   | 16      | ~8                |
| `entity_area`     | 9       | ~5                |
| `entity_conn`     | 8       | ~4                |

These are acceptable for compile-time but would be catastrophic if they
ran at execution time. Fortunately, compilation resolves everything to
integer indices/byte codes.

### Memory Allocation Patterns

| Path                       | Allocations/call | Notes                    |
|----------------------------|-----------------|--------------------------|
| `new_script_param()`       | 2 (calloc + malloc) | Called per script line |
| `expand_argument()`        | 1 (malloc + free) | Called per argument    |
| `new_buf()` in param       | 1 (malloc for string) | 1KB default buffer   |
| `execute_script()` setup   | 0 (stack `block`)    | Efficient — no heap  |
| Trigger dispatch           | 0                    | No allocations        |

**The main allocation hotspot** is `SCRIPT_PARAM` lifecycle:
- `new_script_param()` = `calloc(1, sizeof(SCRIPT_PARAM))` + `new_buf()` = `malloc(1024)`
- `free_script_param()` = `free()` + `free()`
- Called once per opcode execution, ifcheck evaluation, and argument parse

This means a typical script execution of 30 lines with 2 arguments each
produces ~90 malloc/free cycles just for SCRIPT_PARAM management.

### Performance Impact of Each Phase

#### Phase 1: Command Consolidation

**Runtime performance: Neutral to slightly positive.**

- Dispatch mechanism is unchanged — still `cmd_table[index].func()` O(1)
- Shared `scriptcmd_*` functions replace per-type wrappers with identical logic
- **Binary size reduction ~40-50KB** from eliminating ~12,000 lines of
  duplicated code → better I-cache utilization
- The added `script_info_room(info)` helper adds one branch per call
  (check which entity type is set) — negligible vs the cost of the
  command itself
- No new allocations, no new string comparisons

**Compilation performance: Neutral.**

- Command lookup tables remain the same size per script type

#### Phase 2: Entity Field Metadata

**Runtime performance: Neutral (zero impact).**

- `description` and `deprecated` fields on ENT_FIELD are only accessed
  during compilation and OLC help — never during bytecode execution
- The entity field byte codes and switch dispatch are unchanged
- Struct is slightly larger in memory but ENT_FIELD tables are static and
  small (~400 total entries across all tables)

**Compilation performance: Negligible negative.**

- `entity_type_lookup()` linear scan touches slightly larger structs
  (worse cache line utilization), but ENT_FIELD tables are small enough
  that this is immeasurable

#### Phase 3: Encoding Modernization

**Runtime performance: Slight negative (entity expansion).**

Widening `unsigned char code` to `uint16_t`:
- Entity expansion changes from `switch(*str++)` (1 byte read) to reading
  2 bytes per field code. This doubles the bytecode size for entity chains.
- `$(enactor.leader.mount.short)` goes from 5 bytes to 8 bytes
- The switch dispatch remains O(1) but the bytecodes are twice as large,
  marginally worsening data cache utilization
- **Impact is small** — entity chains are typically 2-4 fields deep

**Hash-based field lookup** (compile-time only):
- Replaces O(78) linear scan with O(1) hash for `entity_mobile` lookups
- Only benefits compilation, not runtime
- Worth doing if script editing responsiveness matters (large scripts)

**Single-letter escape deprecation**:
- If `$i`, `$n` etc. are removed, `expand_string_simple_code()` path
  vanishes, simplifying the expansion loop. Minor positive.
- Existing scripts would need migration (high effort, low urgency)

#### Phase 4: Table-Based Entity Patterns

**Runtime performance: Slight positive.**

Table entities like `ENT_STATS_TABLE` consolidate what would be 6+ separate
switch cases (`ENTITY_MOB_STR`, `ENTITY_MOB_DEX`, ...) into a single
`expand_entity_stats_table()` function that indexes by field code.

- Fewer switch cases in `expand_entity_mobile()` → smaller function →
  better I-cache behavior
- The indexing lookup is still O(1) via direct byte code
- Net effect: slightly smaller code, same speed

**Enum pressure relief** is the main win here, not performance.

#### Phase 5: If-Check Improvements

**Runtime performance: Neutral.**

- `long *ret` instead of `int *ret` — same machine-word size on 64-bit,
  no performance difference
- Instance/dungeon parameters — adds two pointer parameters to the ifcheck
  function signature. On x86-64 these are passed in registers, so zero cost
  when not used. When used, avoids a variable lookup that would otherwise
  be needed (slight positive).

**Compilation performance: Neutral.**

### Additional Optimization Opportunities

#### SCRIPT_PARAM Recycling Pool (High Impact)

Replace `calloc`/`free` in `new_script_param()`/`free_script_param()` with
a free-list pool, similar to how `BUFFER` already uses `buf_free`:

```c
static SCRIPT_PARAM *param_free = NULL;

SCRIPT_PARAM *new_script_param() {
    SCRIPT_PARAM *arg;
    if (param_free) {
        arg = param_free;
        param_free = arg->next;
        clear_buf(arg->buffer);  // Reset existing buffer
    } else {
        arg = alloc_perm(sizeof(SCRIPT_PARAM));
        arg->buffer = new_buf();
    }
    arg->type = ENT_NONE;
    return arg;
}
```

**Estimated impact**: Eliminates ~90 malloc/free cycles per script execution.
On a busy server with 100+ scripts firing per second, this saves thousands
of heap operations per second.

#### expand_argument Stack Buffer (Medium Impact)

Replace the `malloc(strlen(str) + 1)` / `free(buf)` in `expand_argument()`
with a stack buffer:

```c
char buf[MIL];  // Stack-allocated, ~4KB
// Instead of: char *buf = malloc(strlen(str) + 1);
```

Most argument strings are well under 4KB. This eliminates one malloc/free
per argument parse — with 1,159 call sites, this is significant.

#### Trigger Dispatch Short-Circuit (Medium Impact)

Currently `test_number_trigger` and `test_string_trigger` iterate through
entities and their tokens even when most entities have no programs at all.
Adding a program-count check early would skip empty entities:

```c
if (mob && mob->pIndexData && mob->pIndexData->progs &&
    list_size(mob->pIndexData->progs[slot]) > 0) {
    // Only then iterate the prog list
}
```

Many mobs and objects in a room have zero programs. Skipping them before
starting iterator setup would reduce overhead in crowded rooms.

---

## Dev Branch: Custom Trigger System ("xtriggers")

The `src_20_dev` branch replaced the hardcoded `trigger_table[]` array
with a dynamic, OLC-editable trigger system. This is a significant
architectural change worth understanding for both functionality and
performance implications.

### What Changed

**Current branch**: `trigger_table[]` is a static C array of 215
`struct trigger_type` entries, each hardcoded with name, slot, and
which prog types (mob/obj/room/token/area/instance/dungeon) can use
the trigger. Adding a new trigger type requires a code change, rebuild,
and restart.

**Dev branch**: `trigger_table[]` still exists as a compiled-in table of
"builtin" triggers (`TRIG_ACT`, `TRIG_SPEECH`, etc.), but a parallel
`trigger_list` (LLIST) is loaded from a file (`TRIGGERS_FILE`) at startup.
Each entry wraps or extends builtin triggers, and builders can create
entirely new "custom" trigger types (type >= `TRIG__MAX`) at runtime
without code changes.

### Data Structure

```c
// Same struct in both branches, but dev adds usage + scriptable fields
struct trigger_type {
    char *name;
    char *alias;
    int type;        // < TRIG__MAX = builtin, >= TRIG__MAX = custom
    int slot;        // TRIGSLOT_* for prog bucket
    int progs;       // Bitmask: PRG_MPROG|PRG_OPROG|... (replaces
                     // individual bool mob/obj/room/token fields)
    int usage;       // Reference count of scripts using this trigger
    bool scriptable; // Can scripts fire this trigger type?
};
```

The current branch uses individual bools (`bool mob`, `bool obj`, etc.)
while dev consolidated to a bitmask `int progs` — cleaner and extensible.

### Storage & Lookup

| Operation              | Current                        | Dev Branch                   |
|------------------------|-----------------------------|------------------------------|
| **Storage**            | Static `trigger_table[]`     | `trigger_list` (LLIST) loaded from file |
| **By-name lookup**     | Linear scan of `trigger_table[]` | Linear scan of `trigger_list` |
| **By-type lookup**     | Direct index: `trigger_table[type]` | Linear scan: `get_trigger_type_bytype()` |
| **Slot lookup**        | `trigger_table[type].slot` — O(1) | Same, once trigger_type* is found |
| **At-runtime dispatch**| `is_trigger_type(tindex, type)` — int compare | Same: `tindex == type` — O(1) |

**Performance concern**: The dev branch's `get_trigger_type_bytype(type)`
does a linear scan of the linked list for every type lookup. This is only
used in specific contexts (save/load/display), not in the trigger dispatch
hot path. The hot path (`is_trigger_type`) remains O(1) in both branches.

### OLC Integration

The dev branch adds a `do_triggers` command for implementors:

```
triggers list                    — Show all triggers with usage counts
triggers add <name> [slot]       — Create a new custom trigger type
triggers install <name> <type>   — Install a builtin trigger type
triggers scriptable <name> <y/n> — Toggle whether scripts can fire it
triggers space <name> <space>    — Change which prog types can use it
triggers remove <name>           — Remove unused custom triggers
```

Custom triggers are persisted to a file and loaded at boot via
`init_scripting()` → `load_triggers()`. Each trigger maintains a `usage`
counter tracking how many scripts reference it.

### How Custom Triggers Fire

Custom triggers (type >= `TRIG__MAX`) use the same dispatch mechanism as
builtin triggers. The `slot` field determines which `progs[slot]` bucket
they're stored in. When scripts call trigger-firing commands, the custom
trigger type integer flows through the same `test_number_trigger()` /
`test_string_trigger()` path.

A script can fire a custom trigger via existing commands like
`mob trigger <name>`, which resolves the trigger name to its type integer
and dispatches normally.

### Performance Implications

**Neutral at runtime**: Custom triggers use the same integer-based dispatch
as builtins. `is_trigger_type()` is `return tindex == type` in both
branches — O(1). The `slot` field puts custom triggers in the same
buckets, so they don't add iteration overhead to unrelated trigger checks.

**Slightly negative at compile time**: `get_trigger_type(name, progs)`
scans a linked list instead of a static array. Since custom triggers go
at the end of the list (type > `TRIG__MAX`), they may take longer to find.
A hash map would fix this, but compilation is infrequent so this is low
priority.

**Positive for extensibility**: Area builders can define new trigger types
for domain-specific events without C code changes. This avoids the current
pattern of adding TRIG_ enum values, rebuilding, and restarting. However,
each custom trigger consumes a slot in the `progs[]` array, so slot
assignments must be reused thoughtfully.

### Considerations for Back-Porting

If custom triggers are brought to the current branch:

1. **Migrate the `progs` field**: Replace the 7 individual bool fields
   (`mob`, `obj`, `room`, `token`, `area`, `instance`, `dungeon`) with a
   single `int progs` bitmask. This is a prerequisite.

2. **Add `usage` and `scriptable` fields**: Required for the OLC to work
   and for safety (preventing removal of in-use triggers).

3. **File persistence**: Add `save_triggers()` / `load_triggers()` and
   the `TRIGGERS_FILE` constant.

4. **Consider hash-based lookup**: For `get_trigger_type_byname()` and
   `get_trigger_type_bytype()`, especially if the trigger count grows
   past a few hundred. A string hash for by-name and a flat array for
   by-type would maintain O(1) dispatch.

5. **Slot pressure**: With only 15 slots and custom triggers sharing them,
   heavy custom trigger use could pack a single slot with too many entries.
   Monitor `progs[TRIGSLOT_GENERAL]` list lengths.

---

## Performance Summary

### What's Fast (No Action Needed)

| Component                  | Why It's Fast                           |
|---------------------------|-----------------------------------------|
| Bytecode execution loop   | `opcode_table[opcode](block)` — O(1) dispatch |
| Entity field expansion    | `switch(*str)` on byte code — O(1) jump table |
| Command dispatch          | `cmd_table[index].func()` — O(1) pre-compiled |
| If-check evaluation       | Pre-compiled index + tree with short-circuit |
| Trigger slot bucketing    | `progs[slot]` avoids scanning all 215 trigger types |
| Trigger type comparison   | `tindex == type` — single integer compare |

### What's Slow (Action Recommended)

| Component                  | Issue                                   | Fix                    |
|---------------------------|-----------------------------------------|------------------------|
| `new_script_param()` per line | 2 heap allocs per script line       | Free-list pool         |
| `expand_argument()` malloc | 1 heap alloc per argument parse        | Stack buffer           |
| Crowded room trigger scan  | O(entities × tokens) per trigger event | Short-circuit empty progs |
| Entity field compile-time lookup | O(78) linear scan per field     | Hash table (low priority) |
| If-check compile-time lookup | O(424) linear scan per check         | Hash table (low priority) |

### Phase Impact Summary

| Phase | Runtime Impact | Compile Impact | Main Benefit               |
|-------|---------------|----------------|----------------------------|
| 1. Command Consolidation | Neutral+ | Neutral | Code maintainability, I-cache |
| 2. Field Metadata    | Zero          | Negligible-  | Self-documenting scripts    |
| 3. Encoding Modernization | Slight-  | Positive   | Removes 123-entry ceiling   |
| 4. Table Entity Patterns | Slight+   | Neutral    | Enum pressure relief        |
| 5. If-Check Improvements | Neutral   | Neutral    | Feature completeness        |
| Custom Triggers      | Neutral       | Slight-      | Runtime extensibility       |
| SCRIPT_PARAM pooling | **Positive**  | N/A          | Eliminates ~90 allocs/script|
| expand_argument stack buf | **Positive** | N/A       | Eliminates 1 alloc/argument |

The two highest-impact performance improvements (**SCRIPT_PARAM pooling**
and **expand_argument stack buffer**) are independent of all five phases
and could be implemented immediately with minimal risk.

---

## Quick Reference: Script System File Map

```
scripts.h          — All enums, structs, prototypes
scripts.c          — Engine core, trigger dispatch, execution loop
script_comp.c      — Compiler: source text → bytecode
script_expand.c    — Entity expansion: bytecode → values
script_const.c     — Entity field tables, ifcheck table, constant data
script_ifc.c       — If-check implementations
script_vars.c      — Variable management (get/set/save/load)
script_commands.c  — Shared command implementations (scriptcmd_*)
script_mpcmds.c    — Mob-specific commands (do_mp*)
script_opcmds.c    — Object-specific commands (do_op*)
script_rpcmds.c    — Room-specific commands (do_rp*)
script_tpcmds.c    — Token-specific commands (do_tp*)
script_lua.c       — Lua bridge (minimal)
script_cmds.c      — Stub file
```

---

## Feature: Script Imports and Functions

### Summary

Add compile-time `import` and `function`/`endfunction` support to the scripting
language, allowing utility scripts to define reusable named function blocks that
other scripts can include and invoke. This eliminates common code patterns being
duplicated across scripts and enables builders to maintain shared logic in one
place.

### Syntax

#### Defining functions (utility script)

```
* Script 500 (MPROG) — Utility helpers
* This script is never triggered directly; it only provides functions.

function heal_target
  if actor() == null
    return
  endif
  mob heal $n 100
endfunction

function announce
  mob echoat $n {GYou feel a warm glow.{x
endfunction
```

#### Importing and calling functions

```
* Script 1001 (MPROG) — Combat healing trigger
import 500

mob say Healing you now, $n!
gosub heal_target
gosub announce
```

#### Keyword reference

| Keyword | Context | Description |
|---|---|---|
| `import <vnum>` | Top of script, before any code | Includes all functions from the target script |
| `function <name>` | Defines a named callable block | Skipped during linear execution |
| `endfunction` | Ends a function block | Implicit `return` |
| `gosub <name>` | First-class keyword (not sub-command) | Jumps to named function, returns after `endfunction`/`return` |
| `return` | Inside a function body | Returns to the line after the `gosub` call |

### Design: Compile-Time Inclusion

The `import` directive is processed entirely at compile time. When `compile_script()`
is called, a new pre-pass resolves all imports by textually appending the imported
script's source after the main script body, then compiling the combined result as a
single unit. At runtime, the bytecode is indistinguishable from a single script —
there is zero runtime overhead from the import mechanism itself.

#### Why compile-time, not runtime

| Concern | Compile-time | Runtime |
|---|---|---|
| Performance overhead | Zero (resolved once) | Function lookup per call |
| Variable sharing | Natural (same scope) | Must explicitly share or isolate |
| Control flow safety | Compiler validates nesting | Must save/restore state |
| Code debuggability | Line numbers need source mapping | Simpler, but harder to step through |

### Call vs. Gosub vs. Xcall

The existing `call`/`xcall` commands and the new `gosub` serve different purposes:

| | `call <vnum>` | `xcall <entity> <vnum>` | `gosub <name>` |
|---|---|---|---|
| Resolution | Runtime vnum lookup | Runtime vnum lookup | Compile-time label |
| Stack frame | New `execute_script()` | New `execute_script()` | Same `SCRIPT_CB` |
| Variables | Isolated (separate context) | Isolated (different entity) | Shared (same scope) |
| Return value | Via `lastreturn` | Via `lastreturn` | Inline (no return value) |
| Entity context | Same entity | Different entity | Same entity |
| Use case | Call another script | Cross-entity scripting | Invoke utility function |
| Overhead | Full stack frame setup | Full stack frame setup | Single jump + return address push |

### New Opcodes

Add before `OP_LASTCODE` in the opcode enum:

```c
OP_FUNCTION,      // Start of function block — linear execution skips to OP_ENDFUNCTION
OP_ENDFUNCTION,   // End of function block — implicit return (pop call stack)
OP_GOSUB,         // Push return address, jump to function label
OP_RETURN,        // Pop return address, resume caller
```

### New Data Structures

#### `SCRIPT_DATA` additions (merc.h)

```c
struct script_data {
    // ... existing fields ...
    int n_imports;           // Count of imported script WNUMs
    WNUM *imports;           // Array of imported script WNUMs (for dependency tracking)
};
```

The `imports` array is derived from the source text during compilation and does not
need to be persisted separately — recompilation naturally rebuilds it.

#### `SCRIPT_CB` additions (scripts.h)

```c
#define MAX_GOSUB_DEPTH  16  // Maximum nested gosub calls within a single script

struct script_control_block {
    // ... existing fields ...
    int call_stack[MAX_GOSUB_DEPTH];  // Return-address stack for gosub/return
    int call_sp;                       // Stack pointer (0 = empty)
};
```

### Implementation: File-by-File Changes

#### 1. `scripts.h` — Enums, constants, structs

- Add `OP_FUNCTION`, `OP_ENDFUNCTION`, `OP_GOSUB`, `OP_RETURN` to the opcode enum
  (before `OP_LASTCODE`)
- Add `MAX_GOSUB_DEPTH` constant (16)
- Add `call_stack[]` and `call_sp` to `struct script_control_block`

#### 2. `merc.h` — SCRIPT_DATA

- Add `int n_imports` and `WNUM *imports` to `struct script_data`

#### 3. `script_comp.c` — Compiler (bulk of the work)

Three additions to the compilation pipeline:

**(a) Import resolution pre-pass** — New function called at the top of `compile_script()`:

```c
char *resolve_imports(char *source, AREA_DATA *area, int type,
                      WNUM *out_imports, int *out_n_imports,
                      BUFFER *err_buf)
```

- Scans source for lines matching `import <vnum>` (must appear before any code)
- For each import:
  - Resolves the vnum via `get_script_index()` using the compiling script's area context
  - Validates: target exists, same script type, and is not this script (direct self-import)
  - Checks for circular imports by tracking an include-visit stack (prevents A→B→A)
  - Extracts only `function`...`endfunction` blocks from the target's `edit_src`
    (non-function code in the imported script is ignored — only function definitions
    are included)
  - Appends the extracted function blocks after the main script source
- Records imported WNUMs in `out_imports` for dependency tracking
- Returns the combined source string (caller must free if different from input)

**Import ordering**: Imports are processed in declaration order. If script 500 itself
imports script 499, that transitive import is resolved recursively (with circular
detection).

**(b) New keyword parsing** — In the `if/else if` chain that handles `if`, `while`,
`for`, `switch`, `mob`, etc.:

- `function <name>`: Emits `OP_FUNCTION`. Records the function name as a named label.
  Pushes `IN_FUNCTION` onto the nesting state stack. At runtime, `opc_function()`
  skips forward to the matching `OP_ENDFUNCTION` (same skip mechanism as
  `OP_IF`→`OP_ENDIF`).
- `endfunction`: Emits `OP_ENDFUNCTION`. Pops the function state. The `param` field
  stores nothing — `opc_endfunction()` pops the gosub call stack.
- `return`: Emits `OP_RETURN`. Valid only inside a `function` block (compiler error
  otherwise).
- `gosub <name>`: Emits `OP_GOSUB`. The label name is resolved to a code line index
  at compile time (same mechanism as `gotoline` uses `code[cline].label`). Validation:
  the label must reference a `function` block. Compiler error if the label doesn't exist.

`gosub` is a first-class keyword — not a sub-command of `mob`/`obj`/`room`/etc.
This avoids needing to add handling to every entity command table, since functions
are type-agnostic within the same script.

**(c) Post-pass linkage** — After all lines are compiled:

- Resolve `OP_FUNCTION` → `OP_ENDFUNCTION` skip offsets (store the target line in
  `code[cline].param`, mirroring how `OP_IF` → `OP_ENDIF` works)
- Resolve `OP_GOSUB` label references to the `OP_FUNCTION` line numbers
- Validate all `function`/`endfunction` blocks are properly paired

#### 4. `script_const.c` — Opcode table registration

Add four new entries to `opcode_table[OP_LASTCODE]` and `opcode_names[OP_LASTCODE]`:

```c
// opcode_table:
opc_function,
opc_endfunction,
opc_gosub,
opc_return,

// opcode_names:
"FUNCTION",
"ENDFUNCTION",
"GOSUB",
"RETURN",
```

#### 5. `scripts.c` — Runtime opcode handlers

```c
// OP_FUNCTION: Skip over the function body during linear execution.
// Functions are only entered via OP_GOSUB.
bool opc_function(SCRIPT_CB *block) {
    // param holds the line number of the matching OP_ENDFUNCTION
    block->line = block->cur_line->param + 1;
    return true;
}

// OP_ENDFUNCTION: Implicit return — pop the gosub call stack.
bool opc_endfunction(SCRIPT_CB *block) {
    if (block->call_sp > 0) {
        block->line = block->call_stack[--block->call_sp];
    } else {
        // Reached endfunction without a gosub (shouldn't happen with
        // proper compilation, but safe fallback: advance past it)
        block->line++;
    }
    return true;
}

// OP_GOSUB: Push return address and jump to function.
bool opc_gosub(SCRIPT_CB *block) {
    if (block->call_sp >= MAX_GOSUB_DEPTH) {
        pbugf(LOG_SCRIPTS, "Script %d: gosub stack overflow.", block->script->vnum);
        return false;  // Halt script
    }
    block->call_stack[block->call_sp++] = block->line + 1;  // return address
    block->line = block->cur_line->param;  // jump to OP_FUNCTION line
    // Note: the OP_FUNCTION handler will then advance into the body (+1)
    // so we jump to param, not param+1. The function handler itself skips
    // the body during linear execution, but here we're entering it on
    // purpose — so we jump to param+1 (the first line of the body).
    block->line = block->cur_line->param + 1;
    return true;
}

// OP_RETURN: Explicit return from function — pop the gosub call stack.
bool opc_return(SCRIPT_CB *block) {
    if (block->call_sp > 0) {
        block->line = block->call_stack[--block->call_sp];
    } else {
        pbugf(LOG_SCRIPTS, "Script %d: return without gosub.", block->script->vnum);
        return false;  // Halt script
    }
    return true;
}
```

Also add initialization of the call stack in `execute_script()`:
```c
block.call_sp = 0;
```

#### 6. `editors/scripting/olc_mpcode.c` — Dependency-aware recompilation

After `scriptedit_compile` succeeds, trigger recompilation of any scripts that
import the just-compiled script:

```c
void recompile_importers(SCRIPT_DATA *changed_script)
```

- Iterates all scripts of matching type across all areas
- For each script whose `imports[]` list contains `changed_script`'s WNUM,
  recompile it via `compile_script()`
- This is O(all scripts) but only runs on manual in-game `compile`, not at boot
  or runtime — acceptable for an OLC operation

#### 7. Boot-time compilation order

At boot time (`olc_save.c` / `json_area.c`) scripts are batch-compiled as areas load.
For imports to work, the imported script must be compiled (or at least have its
source available) before the importer. Since `resolve_imports()` reads the target's
`edit_src` (raw source text, not bytecode), compilation order doesn't matter —
the pre-pass just needs the source to exist, which it does after area loading.

However, if a circular dependency exists (A imports B imports A), the circular
detection in `resolve_imports()` will catch it and report a compile error regardless
of boot order.

### Edge Cases and Safety

#### Circular imports

`resolve_imports()` maintains a visited-set of script vnums. If a vnum is encountered
twice during recursive resolution, it emits a compile error and aborts:

```
Line 1: Circular import detected: script 500 → 501 → 500
```

#### Cross-type imports

Importing a MPROG function into an OPROG would allow `mob`-only commands to appear
in an object script. Two options:

1. **Strict**: Only allow importing scripts of the same type. Simple and safe.
2. **Library type**: Add a new `PRG_LIBRARY` script type that skips entity-specific
   command validation. Functions in library scripts can only use universal commands
   (variables, math, string ops, ifchecks).

**Recommended**: Start with strict same-type-only imports. This covers the primary
use case (shared mob utilities, shared room utilities, etc.) and avoids complexity.
A library type can be added later if needed.

#### Function name collisions

If two imported scripts define functions with the same name, the compiler should
report an error:

```
Line 1: Function 'heal_target' already defined (imported from script 500).
```

This uses the existing named-label duplicate detection, extended to track which
import contributed each label.

#### Nesting restrictions

- Functions cannot be nested (`function` inside `function` is a compile error)
- `gosub` can be called from inside `if`/`while`/`for`/`switch` blocks (the nesting
  level of the caller is preserved across the call since the function operates at
  its own nesting level — the compiler validates that each function's
  `if`/`endif`/`for`/`endfor` etc. are self-contained)
- `gosub` can call another `gosub` (up to `MAX_GOSUB_DEPTH` = 16)
- `return` exits only the innermost function; it does not exit `if`/`for`/`while`
  blocks within the function (the compiler should validate this)

#### Entity destruction during gosub

The existing `SCRIPTEXEC_HALT` flag and `script_destructed` checks in the execution
loop (`scripts.c:3198-3210`) already handle entity destruction mid-script. A `gosub`
does not create a new stack frame or `SCRIPT_CB`, so the same halt mechanism works
naturally — if the entity is destroyed during a function call, the script halts on
the next iteration of the execution loop, just as it does for inline code.

#### Line numbers in error messages

When displaying compile errors or `wiznet` script debugging, imported code has
shifted line numbers. Two approaches:

1. **Simple**: Prefix imported function errors with the source script vnum:
   `[import 500] Line 3: Invalid ifcheck 'foo'.`
2. **Detailed source map**: Track `(source_vnum, source_line)` tuples per compiled
   code line. More complex but more precise.

**Recommended**: Start with approach 1 (prefixed errors). A source map can be added
later if debugging becomes difficult.

### What stays the same

- `call <vnum>` / `xcall <entity> <vnum>` remain unchanged (runtime cross-script
  calls with isolated stack frames)
- Script serialization format doesn't change (`import` lines live in the source text)
- No new file types or OLC editors needed (`sedit` works as-is)
- No runtime performance impact from the import mechanism itself (resolved at
  compile time)
- Entity trigger dispatch is unaffected
- Variable system is unaffected

### Implementation priority

This feature is independent of the five improvement phases (command consolidation,
field metadata, encoding modernization, table entity patterns, ifcheck improvements)
and can be implemented at any time. It benefits most from being implemented *after*
Phase 1 (Command Consolidation), since consolidated commands would be more
naturally reusable across imported functions.

### Complexity estimate

| Component | Effort | Risk |
|---|---|---|
| Import pre-pass (`resolve_imports`) | Medium | Low — textual preprocessing |
| `function`/`endfunction` compiler keywords | Medium | Low — follows existing `for`/`endfor` pattern |
| `gosub`/`return` compiler keywords | Low | Low — similar to `gotoline` |
| Opcode handlers (4 functions) | Low | Low — simple jump/stack operations |
| Linkage fixup (function↔endfunction) | Low | Low — mirrors if↔endif |
| Dependency recompilation | Medium | Medium — iterates all scripts |
| Circular import detection | Low | Low — visited-set DFS |
| **Total** | **Medium** | **Low-Medium** |
