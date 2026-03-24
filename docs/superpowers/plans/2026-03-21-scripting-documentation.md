# Scripting System Documentation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite and complete the Sentience MUD scripting system documentation for builders, producing ~52 verified reference, tutorial, and guide files.

**Architecture:** Phase 1 extracts structured data from C source into session artifacts. Phases 2-5 write documentation files referencing that extracted data. Phase 6 cross-links everything. All content is verified against source code, not copied from existing docs.

**Tech Stack:** Markdown with Jekyll front matter. Source verification against C code in `/sentience/src/`. Output to `/sentience/docs-guides/builder-docs/scripting/`. Separate git repo at `/sentience/docs-guides/builder-docs/`.

**Spec:** `/sentience/src/docs/superpowers/specs/2026-03-21-scripting-documentation-design.md`

---

## Context for All Tasks

### Project Structure
- **Source code:** `/sentience/src/` — C files implementing the scripting engine
- **Output directory:** `/sentience/docs-guides/builder-docs/scripting/` — Jekyll Markdown files
- **Session artifacts:** `/home/sentiencemud/.copilot/session-state/4972e333-e7e5-4e17-85d7-bb730afb2b93/files/` — extracted reference data
- **Docs git repo:** `/sentience/docs-guides/builder-docs/` (remote: megacosm-entertainment/builder-docs)

### Key Source Files
| File | Purpose |
|------|---------|
| `src/scripts.c` | Core execution engine (~13K lines) |
| `src/script_commands.c` | Shared command implementations (~15K lines) |
| `src/script_expand.c` | Variable/expression expansion (~11K lines) |
| `src/script_ifc.c` | Ifcheck functions (~6.5K lines) |
| `src/script_comp.c` | Bytecode compilation (~2.6K lines) |
| `src/script_const.c` | Constants and tables (~2.5K lines) |
| `src/script_vars.c` | Variable system (~4.3K lines) |
| `src/script_mpcmds.c` | Mob command table (167 commands) |
| `src/script_opcmds.c` | Object command table (162 commands) |
| `src/script_rpcmds.c` | Room command table (157 commands) |
| `src/script_tpcmds.c` | Token command table (166 commands + tokenother) |
| `src/scripts.h` | Enums, structs, defines |
| `src/merc.h` | Core data structures |

### Script Syntax Reference (for writing examples)
Scripts use this syntax (NOT C-like, NOT %-prefixed — verify against real area files):
- Entity prefix per script type: `mob`, `obj`, `room`, `token`, `area`, `instance`, `dungeon`, `quest`, `event`
- Quick codes: `$n` (actor name), `$i` (self name), `$t` (target), `$p` (object), `$q` (token)
- Entity references: `$(self)`, `$(enactor)`, `$(target)`, `$(victim)`
- Variable expansion: `$<varname>`, `$(varname:type.field)`
- Arithmetic: `$[expression]`
- Conditionals: `if`/`else`/`elseif`/`endif`
- Loops: `while`/`endwhile`
- Commands prefixed: `mob echo`, `obj echoat`, `room transfer`, etc.

### Jekyll Front Matter Pattern
All files use this format (adjust `nav_order`, `parent`, `grand_parent` per page):
```yaml
---
layout: default
title: Page Title
nav_order: N
parent: Parent Section
grand_parent: Scripting
---
```

### Writing Conventions
- Audience: Builders with no programming background
- Language: "When a player enters..." not "When CHAR_DATA enters..."
- Code blocks: Use triple backticks with no language tag
- Command syntax: `mob command <required> [optional]`
- Cross-references: Relative Markdown links
- Stubs/unimplemented: Document with "**Not Yet Implemented**" note

---

## Phase 1: Source Extraction & Verification

### Task 1: Extract Command Tables

Extract the command name, handler function name, and available-per-type information from all command table source files. Produce a structured reference artifact.

**Files to read:**
- `/sentience/src/script_mpcmds.c` — `mob_cmd_table[]`
- `/sentience/src/script_opcmds.c` — `obj_cmd_table[]`
- `/sentience/src/script_rpcmds.c` — `room_cmd_table[]`
- `/sentience/src/script_tpcmds.c` — `token_cmd_table[]` and `tokenother_cmd_table[]`
- Find area/instance/dungeon/quest/event command tables (likely in `script_commands.c` or similar — search for `area_cmd_table`, `instance_cmd_table`, `dungeon_cmd_table`)

**Output artifact:** `/home/sentiencemud/.copilot/session-state/4972e333-e7e5-4e17-85d7-bb730afb2b93/files/extracted-commands.md`

- [ ] **Step 1:** Search for all command table definitions across `script_*.c` files. Use `grep` for patterns like `cmd_table[]`, `_cmd_table`, `CMD_TABLE`.
- [ ] **Step 2:** For each table found, extract every entry: command name, handler function, and any flags/notes. Record which tables share entries (e.g., quest uses area_cmd_table).
- [ ] **Step 3:** Build a master list of unique commands with columns: command name, description (from function name/comments), mob, obj, room, token, tokenother, area, instance, dungeon, quest, event (✓/✗ per type).
- [ ] **Step 4:** For each command, read the handler function to write a one-line description of what it does. Focus on the implementation in `script_commands.c` since most handlers are shared.
- [ ] **Step 5:** Identify stub/unimplemented commands (empty function bodies or TODO comments).
- [ ] **Step 6:** Save the complete extracted command reference to the output artifact file.

### Task 2: Extract Trigger Definitions

Extract all trigger types, which entity types support them, and what the phrase parameter means.

**Files to read:**
- `/sentience/src/script_const.c` — trigger name tables, trigger slot assignments
- `/sentience/src/scripts.h` — `TRIG_*` enums, `TRIGSLOT_*` defines
- `/sentience/src/scripts.c` — trigger dispatch functions (search for where triggers are fired: `p_act_trigger`, `p_greet_trigger`, etc.)

**Output artifact:** `/home/sentiencemud/.copilot/session-state/4972e333-e7e5-4e17-85d7-bb730afb2b93/files/extracted-triggers.md`

- [ ] **Step 1:** Extract all `TRIG_*` enum values from `scripts.h`. Record the numeric value and name.
- [ ] **Step 2:** From `script_const.c`, extract the trigger name strings and their slot assignments. Map each trigger to its slot (TRIGSLOT_GENERAL, TRIGSLOT_SPEECH, etc.).
- [ ] **Step 3:** Determine which entity types support each trigger. Search `scripts.c` and `scripts.h` for trigger-to-entity-type mappings (e.g., which triggers are in `mprog_trigger_table` vs `oprog_trigger_table`).
- [ ] **Step 4:** For each trigger, determine the entity bindings ($n, $t, $p, $q) by reading the trigger dispatch function. The function that calls `execute_script` will show which entities are passed as `ch`, `obj1`, `vch`, etc.
- [ ] **Step 5:** Document phrase parameter meaning for each trigger (percentage chance, keyword match, hp threshold, etc.) by examining the trigger dispatch logic.
- [ ] **Step 6:** Save the complete extracted trigger reference to the output artifact file. Organize by trigger slot, then alphabetically within each slot.

### Task 3: Extract Ifcheck Definitions

Extract all ifcheck functions, their parameters, return types, and entity type support.

**Files to read:**
- `/sentience/src/script_ifc.c` — ifcheck function table and implementations
- `/sentience/src/scripts.h` — ifcheck-related defines

**Output artifact:** `/home/sentiencemud/.copilot/session-state/4972e333-e7e5-4e17-85d7-bb730afb2b93/files/extracted-ifchecks.md`

- [ ] **Step 1:** Extract the ifcheck function table from `script_ifc.c`. For each entry: name, handler function, parameter types, entity type flags (IFC_M, IFC_O, IFC_R, IFC_T, etc.).
- [ ] **Step 2:** For each ifcheck, read the handler function to determine: what it checks, what parameters it takes, what values are valid, and what it returns.
- [ ] **Step 3:** Cross-reference against the existing `ifchecks-reference.md` in the docs. Flag any ifchecks that exist in source but not in docs (new), in docs but not source (removed), or with changed behavior.
- [ ] **Step 4:** Categorize each ifcheck using the existing category structure from the current docs.
- [ ] **Step 5:** Save the complete extracted ifcheck reference to the output artifact file.

### Task 4: Extract Variable & Token System Details

Extract variable types, token data structures, and persistence mechanisms.

**Files to read:**
- `/sentience/src/script_vars.c` — variable operations
- `/sentience/src/scripts.h` — `VAR_*` enum, `VARIABLE` struct
- `/sentience/src/merc.h` — `TOKEN_DATA` struct, token-related fields on other entities
- `/sentience/src/script_expand.c` — variable expansion logic, entity field access

**Output artifact:** `/home/sentiencemud/.copilot/session-state/4972e333-e7e5-4e17-85d7-bb730afb2b93/files/extracted-variables-tokens.md`

- [ ] **Step 1:** Extract all `VAR_*` enum values from `scripts.h`. Document each variable type.
- [ ] **Step 2:** Read the `VARIABLE` struct and related structures. Document the variable data model.
- [ ] **Step 3:** Read `TOKEN_DATA` struct from `merc.h`. Document token fields: vnum, name, values, timer, flags, variable list.
- [ ] **Step 4:** From `script_vars.c`, extract all variable operations: varset, varclear, varseton, varclearon, varcopy, varsave, and their parameter handling.
- [ ] **Step 5:** From `script_expand.c`, document the variable expansion rules: `$<varname>`, `$(entity.field)`, `$(varname:type.field)`, `$[arithmetic]`, quick codes.
- [ ] **Step 6:** Document variable persistence: which operations save to disk, how variables survive reboots, scope rules.
- [ ] **Step 7:** Save the complete extracted reference to the output artifact file.

### Task 5: Build Gap Analysis Document

During Tasks 1-4, collect any gaps found in the scripting engine itself.

**Output file:** `/sentience/src/docs/TODO_SCRIPT_ENGINE_GAPS.md`

- [ ] **Step 1:** Review the extracted data from Tasks 1-4 artifacts. Identify: triggers that exist for some entity types but are logically missing from others, commands that are stubs/unimplemented, ifchecks with incomplete implementations, inconsistencies between entity types.
- [ ] **Step 2:** Check if any existing gap analysis docs exist in `/sentience/src/docs/` (e.g., `TODO_SCRIPT_*.md`, `PLAN_SCRIPT_*.md`). Cross-reference to avoid duplicating known gaps.
- [ ] **Step 3:** Write `TODO_SCRIPT_ENGINE_GAPS.md` with sections: Missing Triggers, Stub Commands, Incomplete Ifchecks, Entity Type Inconsistencies, Suggested Improvements. Each entry should note what's missing and why it would be useful.

---

## Phase 2: Core Reference Pages

### Task 6: Write scripting-basics.md

Rewrite the core scripting language reference. This is the foundation page that all other docs reference.

**Read first:** Session artifacts from Phase 1. Also read current file: `/sentience/docs-guides/builder-docs/scripting/scripting-basics.md`
**Write to:** `/sentience/docs-guides/builder-docs/scripting/scripting-basics.md` (overwrite)

- [ ] **Step 1:** Read the current `scripting-basics.md` to understand existing structure and content.
- [ ] **Step 2:** Read Phase 1 artifacts to ensure accuracy of syntax, commands, and features.
- [ ] **Step 3:** Rewrite `scripting-basics.md` covering: script syntax overview, comments, control flow (if/else/elseif/endif), loops (while/endwhile, for/endfor, list/endlist), switch/case (switch/endswitch), entity references and quick codes, variable expansion basics, arithmetic expressions ($[...]), script flags (secured, system, disabled), security levels, compilation and error handling. Include practical examples throughout.
- [ ] **Step 4:** Verify all examples use correct syntax by cross-referencing against real scripts in area JSON files (check `/sentience/area/*.json` for `"code":` fields).
- [ ] **Step 5:** Add Jekyll front matter: `layout: default`, `title: Scripting Basics`, `nav_order: 2`, `parent: Scripting`.

### Task 7: Write advanced-scripting.md

Rewrite the advanced features reference.

**Read first:** Phase 1 artifacts (especially extracted-variables-tokens.md). Current file: `/sentience/docs-guides/builder-docs/scripting/advanced-scripting.md`
**Write to:** `/sentience/docs-guides/builder-docs/scripting/advanced-scripting.md` (overwrite)

- [ ] **Step 1:** Read the current `advanced-scripting.md` and Phase 1 variable/token artifact.
- [ ] **Step 2:** Rewrite covering: variable system deep-dive (all types, scope, lifetime), Random String Generators (RSG), entity cast syntax `$(varname:type.field)`, nested variable expansion, sub-scripts (call, xcall) and call depth, async flow (delay, cancel, queue, dequeue, interrupt, scriptwait), registers and temporary storage (tempstore1-4), the `at` command for remote execution, the `condition` command, script-to-script communication patterns. Include practical examples for each feature.
- [ ] **Step 3:** Add Jekyll front matter: `title: Advanced Scripting`, `nav_order: 3`, `parent: Scripting`.

### Task 8: Write variables-and-tokens.md (Reference)

Create the new standalone reference for the variable and token systems.

**Read first:** Phase 1 artifact `extracted-variables-tokens.md`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/variables-and-tokens.md` (new file)

- [ ] **Step 1:** Write the complete variable type reference: boolean, number, string (dynamic/static/indexed), room, mob, obj, token, affect, exit, shop_stock, and any other types found in extraction.
- [ ] **Step 2:** Write the token data model section: what tokens are (invisible attachable objects), vnum, name, values (array of integers), timer, flags, how tokens attach to characters/objects/rooms.
- [ ] **Step 3:** Write token operations section: giving tokens, junking tokens, attaching/detaching, checking token existence, reading token values/timers.
- [ ] **Step 4:** Write variable operations section: varset (all types), varclear, varseton, varclearon, varcopy, varsave. Include syntax and examples for each.
- [ ] **Step 5:** Write persistence section: which variables survive reboots, varsave behavior, token persistence.
- [ ] **Step 6:** Write "Token vs Variable: When to Use Which" decision guide.
- [ ] **Step 7:** Write practical patterns section: buff tracking, quest progress, cooldowns, state machines using tokens+variables.
- [ ] **Step 8:** Add Jekyll front matter: `title: Variables & Tokens`, `nav_order: 7`, `parent: Scripting`.

### Task 9: Write entity-reference.md

Rewrite the entity types and relationships reference.

**Read first:** Phase 1 artifacts. Current file: `/sentience/docs-guides/builder-docs/scripting/entity-reference.md`
**Write to:** `/sentience/docs-guides/builder-docs/scripting/entity-reference.md` (overwrite)

- [ ] **Step 1:** Read current file and Phase 1 trigger artifact (for entity bindings).
- [ ] **Step 2:** Rewrite covering: all entity types (Character, Object, Room, Token, Area, Instance, Dungeon, Quest, Event), trigger entity bindings per trigger type ($n/$t/$p/$q mappings), entity field access syntax, navigating entity hierarchies (room→area, obj→carrier, token→owner, etc.), targeting syntax (keyword, vnum, widevnum).
- [ ] **Step 3:** Add Jekyll front matter: `title: Entity Reference`, `nav_order: 5`, `parent: Scripting`.

### Task 10: Write quick-codes.md

Rewrite the quick code expansion reference.

**Read first:** Phase 1 variable artifact (expansion rules). Current file: `/sentience/docs-guides/builder-docs/scripting/quick-codes.md`
**Write to:** `/sentience/docs-guides/builder-docs/scripting/quick-codes.md` (overwrite)

- [ ] **Step 1:** Read current file and the expansion logic from `script_expand.c` artifact.
- [ ] **Step 2:** Rewrite with complete quick code table: self ($i/$I), actor ($n/$N/$e/$E/$m/$M/$s/$S), target ($t/$T and pronouns), object ($p/$P), token ($q/$Q), room ($r), variable ($<varname>), entity cast ($(entity.field:type)). Include uppercase vs lowercase meaning for each.
- [ ] **Step 3:** Add practical examples showing quick codes in echo commands.
- [ ] **Step 4:** Add Jekyll front matter: `title: Quick Codes`, `nav_order: 6`, `parent: Scripting`.

### Task 11: Write shared-commands.md

Create the new shared command reference — the master list of commands available across most/all entity types.

**Read first:** Phase 1 artifact `extracted-commands.md`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/shared-commands.md` (new file)

- [ ] **Step 1:** From the extracted commands, identify all commands available in 3+ entity types (these are "shared").
- [ ] **Step 2:** For each shared command, write: name, syntax line (using `<prefix>` placeholder for mob/obj/room/token/etc.), description, parameters table, example, notes on any type-specific behavior differences.
- [ ] **Step 3:** Organize commands by category: Output (echo, echoat, echoaround, etc.), Movement (goto, transfer, teleport, etc.), Loading (mload, oload, etc.), Combat (damage, kill, etc.), Variables (varset, varclear, etc.), Affects (addaffect, stripaffect, etc.), Entity Manipulation (alter*, string*, etc.), Flow Control (call, xcall, delay, etc.), Quest (questaccept, questcomplete, etc.), Miscellaneous.
- [ ] **Step 4:** Add Jekyll front matter: `title: Shared Commands Reference`, `nav_order: 8`, `parent: Scripting`.

### Task 12: Write command-availability.md

Create the command × entity type availability matrix.

**Read first:** Phase 1 artifact `extracted-commands.md`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/command-availability.md` (new file)

- [ ] **Step 1:** Build a Markdown table with columns: Command, Mob, Obj, Room, Token, TokOther, Area, Instance, Dungeon, Quest, Event. One row per unique command. Use ✓/✗ for availability.
- [ ] **Step 2:** Sort alphabetically. Group by category with section headers if the table is very long (180+ rows).
- [ ] **Step 3:** Add Jekyll front matter: `title: Command Availability Matrix`, `nav_order: 9`, `parent: Scripting`.

### Task 13: Write ifchecks-reference.md

Rewrite the comprehensive ifchecks reference, verified against source.

**Read first:** Phase 1 artifact `extracted-ifchecks.md`. Current file: `/sentience/docs-guides/builder-docs/scripting/ifchecks-reference.md`
**Write to:** `/sentience/docs-guides/builder-docs/scripting/ifchecks-reference.md` (overwrite)

- [ ] **Step 1:** Read current file to understand existing category structure and formatting.
- [ ] **Step 2:** Using the extracted ifcheck data, rewrite with consistent formatting per entry: name, syntax, description, parameters, entity type support flags, example.
- [ ] **Step 3:** Adopt existing category structure with additions as needed. Ensure every ifcheck from the extraction is present.
- [ ] **Step 4:** Flag any ifchecks found in extraction but missing from old docs as "**New**".
- [ ] **Step 5:** Add Jekyll front matter: `title: Ifchecks Reference`, `nav_order: 10`, `parent: Scripting`.

**Note:** This is the largest single file (~44KB currently). The rewrite will be similarly large. Prioritize accuracy and consistent formatting over brevity.

---

## Phase 3: Entity-Type Reference Pages

Each task in this phase produces 4 files for one entity type. The pattern is identical for all 9 types but the content differs.

### Task 14: Write Mobile Programs (mob)

**Read first:** Phase 1 artifacts (commands, triggers). Existing files in `/sentience/docs-guides/builder-docs/scripting/mobile-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/mobile-programs/` (overwrite existing, create new)

- [ ] **Step 1:** Read existing mob program docs to understand what's already covered.
- [ ] **Step 2:** Write `index.md`: What mob programs are, when to use them (NPC behavior, combat AI, quest givers, shopkeeper customization), how to access `mpedit` (editor walkthrough: creating a script, writing code, compiling, attaching triggers), overview of mob-specific capabilities. Front matter: `title: Mobile Programs`, `nav_order: 1`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `mprog-triggers.md`: Complete trigger list from extraction, filtered to mob-supported triggers. For each: name, when it fires, phrase meaning, entity bindings, example. Front matter: `title: Triggers`, `parent: Mobile Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `mprog-commands.md`: List ALL commands available to mob programs. Shared commands get brief description + link to `shared-commands.md`. Type-specific commands (if any) get full documentation. Front matter: `title: Commands`, `parent: Mobile Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `mprog-examples.md`: 5 practical examples — greeting NPC, quest-giving NPC, shopkeeper with custom behavior, combat boss with HP phases, patrol guard with speech responses. Each includes complete script, trigger attachment, and explanation. Front matter: `title: Examples`, `parent: Mobile Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `mpedit-create-prog.md` and `mprog-script-commands.md` (content absorbed into new files).

### Task 15: Write Object Programs (obj)

**Read first:** Phase 1 artifacts. Existing files in `/sentience/docs-guides/builder-docs/scripting/object-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/object-programs/`

- [ ] **Step 1:** Read existing object program docs.
- [ ] **Step 2:** Write `index.md`: What object programs are, when to use them (equipment effects, consumables, interactive objects, puzzle items), `opedit` walkthrough. Front matter: `title: Object Programs`, `nav_order: 2`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `oprog-triggers.md`: Complete trigger list for objects. Front matter: `title: Triggers`, `parent: Object Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `oprog-commands.md`: All commands for object programs. Front matter: `title: Commands`, `parent: Object Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `oprog-examples.md`: 5 examples — equipment with on-wear/remove effects, consumable potion, trapped chest, key that unlocks on speech, growing/evolving weapon. Front matter: `title: Examples`, `parent: Object Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `opedit-create-prog.md` and `oprog-script-commands.md`.

### Task 16: Write Room Programs (room)

**Read first:** Phase 1 artifacts. Existing files in `/sentience/docs-guides/builder-docs/scripting/room-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/room-programs/`

- [ ] **Step 1:** Read existing room program docs.
- [ ] **Step 2:** Write `index.md`: What room programs are, when to use them (environmental effects, puzzles, ambushes, area transitions), `rpedit` walkthrough. Front matter: `title: Room Programs`, `nav_order: 3`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `rprog-triggers.md`: Complete trigger list for rooms. Front matter: `title: Triggers`, `parent: Room Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `rprog-commands.md`: All commands for room programs. Front matter: `title: Commands`, `parent: Room Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `rprog-examples.md`: 5 examples — trap room (damage on entry), puzzle room (speech-activated), environmental narration (random descriptions), teleporter room, ambush room (spawns mobs). Front matter: `title: Examples`, `parent: Room Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `rpedit-create-prog.md` and `rprog-script-commands.md`.

### Task 17: Write Token Programs (token)

**Read first:** Phase 1 artifacts (especially extracted-variables-tokens.md). Existing files in `/sentience/docs-guides/builder-docs/scripting/token-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/token-programs/`

- [ ] **Step 1:** Read existing token program docs.
- [ ] **Step 2:** Write `index.md`: What token programs are, why they're the most versatile script type (140+ triggers), when to use them (buffs, debuffs, quest tracking, temporary abilities, cooldowns, persistent effects), `tpedit` walkthrough, how tokens differ from objects (invisible, attachable, timer-based). Include section on TokenOther commands (cross-entity token manipulation). Front matter: `title: Token Programs`, `nav_order: 4`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `tprog-triggers.md`: Complete trigger list for tokens (largest trigger set). Front matter: `title: Triggers`, `parent: Token Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `tprog-commands.md`: All commands for token programs, including TokenOther subsection. Front matter: `title: Commands`, `parent: Token Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `tprog-examples.md`: 5 examples — timed buff with expiry message, quest progress tracker, cooldown ability, combat modifier (damage boost), persistent curse that survives logout. Front matter: `title: Examples`, `parent: Token Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `tpedit-create-prog.md` and `tprog-script-commands.md`.

### Task 18: Write Area Programs (area)

**Read first:** Phase 1 artifacts. Existing files in `/sentience/docs-guides/builder-docs/scripting/area-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/area-programs/`

- [ ] **Step 1:** Read existing area program docs.
- [ ] **Step 2:** Write `index.md`: What area programs are, when to use them (zone-wide events, area resets, global state management), `apedit` walkthrough, note that quest programs share the same command table. Front matter: `title: Area Programs`, `nav_order: 5`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `aprog-triggers.md`: Complete trigger list for areas (limited set: random, reset). Front matter: `title: Triggers`, `parent: Area Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `aprog-commands.md`: All commands for area programs with full documentation. Front matter: `title: Commands`, `parent: Area Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `aprog-examples.md`: 3 examples — area-wide weather/announcement system, reset-triggered repopulation logic, zone-wide variable tracking. Front matter: `title: Examples`, `parent: Area Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `apedit-create-prog.md` and `aprog-script-commands.md`.

### Task 19: Write Instance Programs (instance)

**Read first:** Phase 1 artifacts. Existing files in `/sentience/docs-guides/builder-docs/scripting/instance-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/instance-programs/`

- [ ] **Step 1:** Read existing instance program docs.
- [ ] **Step 2:** Write `index.md`: What instance programs are, when to use them (instanced dungeons, private events, scaled content), the relationship to dungeons and blueprints, editor walkthrough. Front matter: `title: Instance Programs`, `nav_order: 6`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `iprog-triggers.md`: Complete trigger list. Front matter: `title: Triggers`, `parent: Instance Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `iprog-commands.md`: All commands. Front matter: `title: Commands`, `parent: Instance Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `iprog-examples.md`: 3 examples — instance completion tracking, scaled mob spawning, instance-wide variable state. Front matter: `title: Examples`, `parent: Instance Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `ipedit-create-prog.md` and `ipedit-script-commands.md`.

### Task 20: Write Dungeon Programs (dungeon)

**Read first:** Phase 1 artifacts. Existing files in `/sentience/docs-guides/builder-docs/scripting/dungeon-programs/`.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/dungeon-programs/`

- [ ] **Step 1:** Read existing dungeon program docs.
- [ ] **Step 2:** Write `index.md`: What dungeon programs are, when to use them (procedural content, dungeon progression, boss encounters), relationship to instances and blueprints, editor walkthrough. Front matter: `title: Dungeon Programs`, `nav_order: 7`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `dprog-triggers.md`: Complete trigger list. Front matter: `title: Triggers`, `parent: Dungeon Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `dprog-commands.md`: All commands. Front matter: `title: Commands`, `parent: Dungeon Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `dprog-examples.md`: 3 examples — dungeon completion handler, progressive difficulty scaling, boss room setup. Front matter: `title: Examples`, `parent: Dungeon Programs`, `grand_parent: Scripting`.
- [ ] **Step 6:** Delete old `dpedit-create-prog.md` and `dpedit-script-commands.md`.

### Task 21: Write Quest Programs (quest) — NEW

**Read first:** Phase 1 artifacts. No existing files.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/quest-programs/` (create directory and all files)

- [ ] **Step 1:** Create the `quest-programs/` directory.
- [ ] **Step 2:** Write `index.md`: What quest programs are, when to use them (quest lifecycle scripting, dynamic quest generation, quest rewards), the relationship to area programs (shared command table), editor walkthrough. Front matter: `title: Quest Programs`, `nav_order: 8`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `qprog-triggers.md`: Complete trigger list for quests (from extraction). Front matter: `title: Triggers`, `parent: Quest Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `qprog-commands.md`: Document quest-specific usage of the shared area command table. Link to `aprog-commands.md` for full reference. Focus on quest-relevant commands and patterns. Front matter: `title: Commands`, `parent: Quest Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `qprog-examples.md`: 3 examples — quest acceptance handler, quest completion with reward scaling, multi-part quest progression. Front matter: `title: Examples`, `parent: Quest Programs`, `grand_parent: Scripting`.

### Task 22: Write Event Programs (event) — NEW

**Read first:** Phase 1 artifacts. No existing files.
**Write to:** `/sentience/docs-guides/builder-docs/scripting/event-programs/` (create directory and all files)

- [ ] **Step 1:** Create the `event-programs/` directory.
- [ ] **Step 2:** Write `index.md`: What event programs are, when to use them (PvP tournaments, seasonal events, world events), editor walkthrough. Front matter: `title: Event Programs`, `nav_order: 9`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `eprog-triggers.md`: Complete trigger list for events (from extraction). Front matter: `title: Triggers`, `parent: Event Programs`, `grand_parent: Scripting`.
- [ ] **Step 4:** Write `eprog-commands.md`: All commands for event programs. Front matter: `title: Commands`, `parent: Event Programs`, `grand_parent: Scripting`.
- [ ] **Step 5:** Write `eprog-examples.md`: 3 examples — PvP arena event, timed world event, seasonal boss spawn. Front matter: `title: Examples`, `parent: Event Programs`, `grand_parent: Scripting`.

---

## Phase 4: Tutorials

### Task 23: Write tutorials/getting-started.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/getting-started.md`

- [ ] **Step 1:** Create `tutorials/` directory if it doesn't exist.
- [ ] **Step 2:** Write `tutorials/index.md`: Overview of tutorial progression, links to each tutorial, recommended reading order. Front matter: `title: Tutorials`, `nav_order: 1`, `parent: Scripting`, `has_children: true`.
- [ ] **Step 3:** Write `getting-started.md`: What scripts are and why builders use them, the 9 entity types that can have scripts, the script lifecycle (create → write → compile → attach → test), accessing the script editors from within the game, basic editor commands (create, code, compile, show, name, flags, security), a complete walkthrough of opening an editor, creating a script, and compiling it. Front matter: `title: Getting Started`, `nav_order: 1`, `parent: Tutorials`, `grand_parent: Scripting`.

### Task 24: Write tutorials/your-first-script.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/your-first-script.md`

- [ ] **Step 1:** Write a step-by-step tutorial: create a mob that greets players when they enter the room. Walk through every line of the script with explanation. Show how to attach a `greet` trigger. Show how to test it. Then extend the script with conditional behavior (greet differently based on alignment using `isgood`/`isevil` ifchecks). Introduce quick codes ($n, $i, $e). Front matter: `title: Your First Script`, `nav_order: 2`, `parent: Tutorials`, `grand_parent: Scripting`.

### Task 25: Write tutorials/working-with-variables.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/working-with-variables.md`

- [ ] **Step 1:** Write a tutorial covering: what variables are and why you need them, variable types (number, string, boolean), setting and reading variables, using variables in echo commands and conditions, variables on other entities (varseton), persistence (varsave), introduction to tokens as "invisible objects" (what they are, giving tokens, checking for tokens, token values and timers). Practical walkthrough: build a "visit counter" that tracks how many times a player has visited an NPC using a token with a variable. Front matter: `title: Working with Variables`, `nav_order: 3`, `parent: Tutorials`, `grand_parent: Scripting`.

### Task 26: Write tutorials/building-a-quest.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/building-a-quest.md`

- [ ] **Step 1:** Write a complete quest-building tutorial: design a multi-stage fetch quest from scratch. Quest-giving NPC (speech trigger), tracking progress with token variables, item collection checks (carries ifcheck + hastoken), quest completion with rewards (award, chargemoney), handling edge cases (player logs out mid-quest, player drops the quest item). Show the complete scripts for each entity involved and how they connect. Front matter: `title: Building a Quest`, `nav_order: 4`, `parent: Tutorials`, `grand_parent: Scripting`.

### Task 27: Write tutorials/combat-scripting.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/combat-scripting.md`

- [ ] **Step 1:** Write a combat scripting tutorial: overview of combat triggers (fight, hpcnt, death, attack_*, defense, barrier), building a boss mob with special abilities at HP thresholds (hpcnt trigger at 75%, 50%, 25%), death script for loot drops (conditional oload), defensive triggers for damage mitigation, making combat feel dynamic with echo messages and delays. Front matter: `title: Combat Scripting`, `nav_order: 5`, `parent: Tutorials`, `grand_parent: Scripting`.

### Task 28: Write tutorials/advanced-patterns.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/tutorials/advanced-patterns.md`

- [ ] **Step 1:** Write an advanced patterns tutorial: delay and asynchronous execution (delay command, cancel, queue/dequeue), sub-script calls (call, xcall — when and why), cross-entity communication (token passing between mobs, variable sharing via varseton), Random String Generators (RSG variables for dynamic text), the `at` command for remote actions, performance tips (what to avoid in random triggers, recursion limits). Front matter: `title: Advanced Patterns`, `nav_order: 6`, `parent: Tutorials`, `grand_parent: Scripting`.

---

## Phase 5: Guides

### Task 29: Write cookbook.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/cookbook.md`

- [ ] **Step 1:** Write copy-paste recipe patterns organized by difficulty (Beginner → Intermediate → Advanced). Each recipe: goal, complete script, trigger type, brief explanation.
  - **Beginner:** Greeting NPC, random emote mob, item with wear message, room description on entry.
  - **Intermediate:** Shopkeeper with custom dialogue, locked door with key check, buff token with timer, mob that remembers players.
  - **Advanced:** Multi-stage quest chain, boss fight with phases, instanced content, area-wide event system, economy interactions (charging/banking).
- [ ] **Step 2:** Add Jekyll front matter: `title: Cookbook`, `nav_order: 11`, `parent: Scripting`.

### Task 30: Write troubleshooting.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/troubleshooting.md`

- [ ] **Step 1:** Write troubleshooting guide covering: common compilation errors and their meanings, runtime error messages, "my script isn't firing" checklist (trigger attached? compiled? not disabled? security level? entity loaded?), debugging techniques (echo breadcrumbs, wiznet flag for script tracing), performance guidance (avoid expensive operations in random triggers, loop limits, recursion depth), security levels explained (who can edit what), script depth and call limits, variable scope gotchas (local vs entity vs persistent), common mistakes (forgetting endif, wrong entity prefix, case sensitivity).
- [ ] **Step 2:** Add Jekyll front matter: `title: Troubleshooting`, `nav_order: 12`, `parent: Scripting`.

---

## Phase 6: Landing Page & Final Pass

### Task 31: Rewrite index.md

**Write to:** `/sentience/docs-guides/builder-docs/scripting/index.md` (overwrite)

- [ ] **Step 1:** Rewrite the scripting landing page with: brief overview of the scripting system, two learning paths ("New to scripting? Start with [Tutorials](tutorials/)" and "Looking something up? Jump to the [Reference](#reference)"), links organized by section (Tutorials, Core Reference, Entity-Type Reference, Guides), brief description of each linked page.
- [ ] **Step 2:** Add Jekyll front matter: `title: Scripting`, `nav_order: 6`, `has_children: true`.

### Task 32: Cross-Reference Pass

Review all written files for internal consistency and cross-linking.

- [ ] **Step 1:** Check every cross-reference link in every file. Ensure all relative links point to files that exist with correct paths.
- [ ] **Step 2:** Add "See Also" sections at the bottom of pages that reference related content (e.g., trigger pages link to entity-reference.md, command pages link to shared-commands.md).
- [ ] **Step 3:** Verify all quick code references are consistent with quick-codes.md.
- [ ] **Step 4:** Verify all ifcheck references in examples actually exist in ifchecks-reference.md.
- [ ] **Step 5:** Verify all command references in examples actually exist in shared-commands.md or entity command pages.

### Task 33: Git Commit

Commit all documentation changes to the docs repo.

- [ ] **Step 1:** `cd /sentience/docs-guides/builder-docs && git add scripting/`
- [ ] **Step 2:** Review staged changes: `git --no-pager diff --cached --stat`
- [ ] **Step 3:** Commit: `git commit -m "docs: complete rewrite of scripting system documentation

- Rewrote all core reference pages for consistency
- Added 6 progressive tutorials
- Added shared command reference and availability matrix
- Added variables & tokens deep-dive reference
- Completed all 9 entity-type reference sections
- Added cookbook and troubleshooting guides
- Verified all content against C source code

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"`
