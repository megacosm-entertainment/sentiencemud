# Scripting System Documentation — Design Spec

## Problem Statement

The Sentience MUD scripting system is a comprehensive, bytecode-compiled engine with 9 script types, 229 triggers, ~183 unique commands (shared across entity types), and 481 ifchecks. The existing builder documentation in `docs-guides/builder-docs/scripting/` covers ~55-60% of the system. Core fundamentals (scripting-basics, advanced-scripting, ifchecks-reference) are strong, but most entity-type command pages are stubs with no descriptions, all creation guides are empty, and newer systems (quest, event, dungeon, instance programs) are barely documented. No troubleshooting guide exists.

The goal is to produce complete, source-verified, builder-facing documentation for the entire scripting system, organized as a Tutorial-First + Reference structure.

## Approach

Rewrite all existing documentation in-place for consistency while keeping the Jekyll front matter format. Add tutorial progression, fill all stub pages, verify every command/trigger/ifcheck against the C source code, and provide practical examples throughout.

## Target Audience

Builders with basic MUD knowledge but no programming background. Documentation should explain "how to do X" rather than "how the engine works internally."

## Output Location

`/sentience/docs-guides/builder-docs/scripting/` — replacing existing files in-place.

## File Structure

```
scripting/
├── index.md                          # Landing page with learning paths
│
├── tutorials/                        # Progressive learning (NEW)
│   ├── index.md                      # Tutorial overview
│   ├── getting-started.md            # What scripts are, editor basics
│   ├── your-first-script.md          # Hello world → greeting NPC
│   ├── working-with-variables.md     # Variable system + token deep-dive
│   ├── building-a-quest.md           # Multi-stage quest walkthrough
│   ├── combat-scripting.md           # Combat triggers, damage, boss fights
│   └── advanced-patterns.md          # Delays, sub-scripts, cross-entity
│
├── scripting-basics.md               # Core language reference (REWRITE)
├── advanced-scripting.md             # Advanced features (REWRITE)
├── entity-reference.md               # Entity types & relationships (REWRITE)
├── quick-codes.md                    # $ expansion codes (REWRITE)
├── ifchecks-reference.md             # All ifchecks, verified (REWRITE)
├── variables-and-tokens.md           # Vars, tokens, persistence (NEW)
├── shared-commands.md                # Commands shared across all/most types (NEW)
├── command-availability.md           # Matrix: commands × entity types (NEW)
│
├── mobile-programs/                  # Mob scripting reference
│   ├── index.md                      # Overview + when to use mprogs
│   ├── mprog-triggers.md             # All triggers w/ descriptions (REWRITE)
│   ├── mprog-commands.md             # All commands w/ syntax+examples (REWRITE)
│   └── mprog-examples.md             # Practical mob script examples (NEW)
│
├── object-programs/                  # Object scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── oprog-triggers.md             # All triggers (REWRITE)
│   ├── oprog-commands.md             # All commands (REWRITE from stub)
│   └── oprog-examples.md             # Practical examples (NEW)
│
├── room-programs/                    # Room scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── rprog-triggers.md             # All triggers (REWRITE)
│   ├── rprog-commands.md             # All commands (REWRITE from stub)
│   └── rprog-examples.md             # Practical examples (NEW)
│
├── token-programs/                   # Token scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── tprog-triggers.md             # All triggers (REWRITE)
│   ├── tprog-commands.md             # All commands (REWRITE from stub)
│   └── tprog-examples.md             # Practical examples (NEW)
│
├── area-programs/                    # Area scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── aprog-triggers.md             # All triggers (REWRITE)
│   ├── aprog-commands.md             # All commands (REWRITE from stub)
│   └── aprog-examples.md             # Practical examples (NEW)
│
├── instance-programs/                # Instance scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── iprog-triggers.md             # All triggers (REWRITE)
│   ├── iprog-commands.md             # All commands (REWRITE from stub)
│   └── iprog-examples.md             # Practical examples (NEW)
│
├── dungeon-programs/                 # Dungeon scripting reference
│   ├── index.md                      # Overview (REWRITE)
│   ├── dprog-triggers.md             # All triggers (REWRITE)
│   ├── dprog-commands.md             # All commands (REWRITE from stub)
│   └── dprog-examples.md             # Practical examples (NEW)
│
├── quest-programs/                   # Quest scripting reference (NEW)
│   ├── index.md                      # Overview (uses area command table)
│   ├── qprog-triggers.md             # All triggers
│   ├── qprog-commands.md             # Quest-specific usage of area commands
│   └── qprog-examples.md             # Practical examples
│
├── event-programs/                   # Event scripting reference (NEW)
│   ├── index.md                      # Overview
│   ├── eprog-triggers.md             # All triggers
│   ├── eprog-commands.md             # All commands
│   └── eprog-examples.md             # Practical examples
│
├── cookbook.md                        # Common patterns & recipes (NEW)
└── troubleshooting.md                # Debugging, errors, performance (NEW)
```

**Total: ~52 files** (9 entity-type dirs × 4 files + 6 tutorials + 9 core reference + 2 guides)

## File Migration Plan

The existing file structure is being reorganized. Key renames and deletions:

### Renames (old → new)
| Old File | New File | Reason |
|----------|----------|--------|
| `mobile-programs/mprog-script-commands.md` | `mobile-programs/mprog-commands.md` | Consistent naming |
| `mobile-programs/mpedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `object-programs/oprog-script-commands.md` | `object-programs/oprog-commands.md` | Consistent naming |
| `object-programs/opedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `room-programs/rprog-script-commands.md` | `room-programs/rprog-commands.md` | Consistent naming |
| `room-programs/rpedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `token-programs/tprog-script-commands.md` | `token-programs/tprog-commands.md` | Consistent naming |
| `token-programs/tpedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `area-programs/aprog-script-commands.md` | `area-programs/aprog-commands.md` | Consistent naming |
| `area-programs/apedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `instance-programs/ipedit-script-commands.md` | `instance-programs/iprog-commands.md` | Consistent naming |
| `instance-programs/ipedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |
| `dungeon-programs/dpedit-script-commands.md` | `dungeon-programs/dprog-commands.md` | Consistent naming |
| `dungeon-programs/dpedit-create-prog.md` | *(absorbed into index.md)* | Editor guide folded into overview |

### Deletions
All `*edit-create-prog.md` files are deleted. Their content (editor-specific creation workflows) is absorbed into each entity type's `index.md`, which includes a "Using the Editor" section.

### New Files
- `tutorials/` directory (6 files)
- `variables-and-tokens.md` (reference)
- `shared-commands.md` (shared command reference)
- `command-availability.md` (command × entity type matrix)
- `cookbook.md`
- `troubleshooting.md`
- 9 × `*prog-examples.md` files
- `quest-programs/` directory (4 files, entirely new)
- `event-programs/` directory (4 files, entirely new)

## Source Verification Plan

Every documented command, trigger, and ifcheck must be verified against the C source code.

### Commands
- Extract from command tables in `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`, and equivalent files for area/instance/dungeon/quest/event
- For each command: read the C implementation to determine parameters, behavior, and side effects
- Flag stubs (commands that exist in the table but have empty or placeholder implementations) — stubs are documented with a "Not Yet Implemented" note so builders know they exist but don't waste time trying to use them
- Note which commands are shared across entity types vs. type-specific

**Shared vs. Type-Specific Command Strategy:**
- `shared-commands.md` documents the ~150+ commands available across all or most entity types. Each entry includes the full syntax, description, parameters, and example.
- Entity-type command pages (`mprog-commands.md`, etc.) list ALL commands available for that type, but shared commands get brief descriptions with links to the shared reference. Only type-specific commands or type-specific behavior differences get full documentation on the entity page.
- `command-availability.md` provides a matrix (rows = commands, columns = entity types, cells = ✓/✗) for quick lookup.

**Quest/Area Command Relationship:**
Quest programs (QPROGs) use the same command table as area programs (`area_cmd_table` in the source). `qprog-commands.md` documents quest-specific usage patterns and examples, linking to `aprog-commands.md` for the full shared command reference. This relationship is clearly noted in both the quest-programs and area-programs index pages.

**TokenOther Commands:**
The source defines a `tokenother_cmd_table` with commands for other script types to execute token commands on external tokens. This is documented as a subsection of `token-programs/tprog-commands.md` explaining the cross-entity token manipulation context.

### Triggers
- Extract from trigger definitions in `script_const.c` and enum definitions in `scripts.h`
- For each trigger: determine when it fires, what entities are bound to $n/$t/$p/$q, what the phrase parameter means
- Map which triggers are available for which entity types

### Gap Analysis
- During source verification, document any gaps discovered in the scripting engine itself (not the docs)
- Output to a separate file: `/sentience/src/docs/TODO_SCRIPT_ENGINE_GAPS.md`
- Categories to track: missing triggers that would be useful, ifchecks that exist as stubs, commands with incomplete implementations, entity types with limited trigger coverage, inconsistencies between entity types (e.g., a trigger available on mobs but not tokens where it logically should be)
- This is a byproduct of the verification work, not a separate research effort — capture gaps as they're discovered

### Ifchecks
- Extract from `script_ifc.c` function tables
- For each ifcheck: syntax, parameters, return type, which entity types support it
- Categorize by function (identity, combat, inventory, location, etc.)
- Verify against the existing 43KB reference document — flag additions, removals, changes

### Variables & Tokens
- Extract variable types from `script_vars.c` and `scripts.h`
- Document the full variable lifecycle: declaration, assignment, expansion, persistence
- Token system from source: token data structures, token values, token timers, token-entity relationships
- Variable-on-entity operations: varseton, varclearon, varcopy patterns

## Content Specifications

### Tutorials Section

**getting-started.md**
- What are scripts and why use them
- The 9 entity types that can have scripts
- How to access the script editors (mpedit, opedit, etc.)
- Script lifecycle: create → write code → compile → attach trigger → test
- Basic editor commands

**your-first-script.md**
- Create a mob that greets players
- Explain each line of the script
- Attach a greet trigger
- Test it in-game
- Add conditional behavior (different greetings by alignment)
- Introduce quick codes ($n, $i, $e)

**working-with-variables.md** (tutorial)
- What variables are and why you need them
- Variable types (number, string, boolean, entity references)
- Setting and reading variables
- Variables on other entities (varseton)
- Persistence (varsave)
- Introduction to tokens as "invisible objects"
- Token values and timers
- Practical example: tracking quest progress with tokens and variables

**building-a-quest.md**
- Design a multi-stage quest from scratch
- Quest-giving NPC with speech trigger
- Tracking progress with token variables
- Item collection with ifchecks (carries, hastoken)
- Quest completion and rewards
- Error handling (player logs out mid-quest)

**combat-scripting.md**
- Combat triggers overview (fight, hpcnt, death, attack_*, hit)
- Making a boss with special abilities at HP thresholds
- Death scripts for loot drops
- Defensive triggers (defense, barrier)
- Area-wide combat events

**advanced-patterns.md**
- Delay and asynchronous execution
- Sub-script calls (call, xcall)
- Cross-entity communication (token passing, variable sharing)
- Random string generators (RSG)
- The `at` command for remote execution
- Performance considerations

### Core Reference Pages

**scripting-basics.md** — Rewrite covering:
- Script syntax overview
- Comments
- Control flow (if/else/elseif/endif)
- Loops (while/endwhile, for/endfor, list/endlist)
- Switch/case
- Entity references and quick codes
- Variable expansion
- Arithmetic expressions
- Script flags and security
- Compilation and error handling

**advanced-scripting.md** — Rewrite covering:
- Variable system deep-dive (all types, scope, lifetime)
- Random String Generators
- Entity cast syntax $(varname:type.field)
- Nested variable expansion
- Sub-scripts and call depth
- Async flow (delay, queue, interrupt, scriptwait)
- Registers and temporary storage
- The `at` command
- The `condition` command
- Script-to-script communication patterns

**variables-and-tokens.md** (NEW standalone reference) — covering:
- Complete variable type reference
- Token data model (vnum, name, values, timer, flags)
- Token operations (give, junk, attach, detach)
- Token programs as the most versatile script type
- Token variable patterns
- Persistence model
- Token vs. variable: when to use which

**ifchecks-reference.md** — Full rewrite:
- Adopt the existing category structure from the current ifchecks-reference.md with minor reorganization as needed
- Consistent formatting per entry: name, syntax, description, parameters, entity type support, example
- Verify all 481 ifchecks against `script_ifc.c` — flag additions, removals, and behavioral changes since docs were last updated

**entity-reference.md** — Rewrite covering:
- All entity types and their relationships
- Trigger entity bindings per trigger type
- Entity field access syntax
- Navigating entity hierarchies (room → area, obj → carrier, etc.)

**quick-codes.md** — Rewrite covering:
- All $ quick codes with descriptions
- Self, actor, target, object, token, room codes
- Uppercase vs lowercase variants
- Variable expansion syntax
- Entity cast syntax

### Entity-Type Reference Pages

Each entity type directory follows the same structure:

**index.md**: What this entity type is, when to use its scripts, how to access and use its editor (absorbs content from the old `*edit-create-prog.md` files), overview of available triggers and commands.

**triggers.md**: Complete trigger list. For each trigger:
- Name
- When it fires
- Phrase parameter meaning (percentage, keyword, etc.)
- Entity bindings ($n = who, $t = what, etc.)
- Example usage
- Notes/caveats

**commands.md**: Complete command list, verified against source. For each command:
- Name
- Syntax line
- Description
- Parameters table
- Example
- Notes (side effects, restrictions, entity type availability)

**examples.md**: 3-5 practical scripts for common use cases specific to that entity type. Each example includes:
- Goal description
- Complete script code
- Line-by-line explanation
- Trigger attachment instructions
- Testing notes

### Cookbook (cookbook.md)

Copy-paste patterns organized by goal:
- Greeting NPCs (simple → complex)
- Shop keepers with custom behavior
- Quest-giving and tracking
- Boss fights with phases
- Puzzle rooms
- Timed events
- Buff/debuff tokens
- Area-wide announcements
- Instanced content scripting
- Economy interactions (charging money, bank operations)

### Troubleshooting (troubleshooting.md)

- Common compilation errors and what they mean
- Runtime error messages
- Script not firing? Checklist (trigger attached? compiled? not disabled? security?)
- Debugging techniques (echo breadcrumbs, wiznet flag)
- Performance: what to avoid (infinite loops, heavy random triggers)
- Security levels explained
- Script depth and recursion limits
- Variable scope gotchas

## Writing Conventions

- **Code blocks**: Use ``` with no language tag (MUD script syntax)
- **Command syntax**: `mob command <required> [optional]` format
- **Cross-references**: Link to related pages using relative Markdown links
- **Jekyll front matter**: Preserve `layout`, `title`, `nav_order`, `parent`, `grand_parent`
- **Consistent heading hierarchy**: H1 = page title (in front matter), H2 = major sections, H3 = subsections
- **Builder-focused language**: "When a player enters the room..." not "When the CHAR_DATA enters the ROOM_INDEX_DATA..."

## Execution Strategy

### Phase 1: Source Extraction & Verification
- Extract all command tables, trigger tables, and ifcheck tables from C source
- Build master lists of commands/triggers/ifchecks per entity type
- Identify stubs, shared commands, and type-specific commands
- Cross-reference against existing documentation

### Phase 2: Core Reference Pages
- Rewrite scripting-basics.md
- Rewrite advanced-scripting.md
- Create variables-and-tokens.md
- Rewrite entity-reference.md
- Rewrite quick-codes.md
- Rewrite ifchecks-reference.md

### Phase 3: Entity-Type Reference (9 types)
- For each type: index, triggers, commands, examples
- Start with mob programs (most complete existing docs, most triggers/commands)
- Then object, room, token (core 4)
- Then area, instance, dungeon, quest, event

### Phase 4: Tutorials
- Getting started
- Your first script
- Variables and tokens
- Building a quest
- Combat scripting
- Advanced patterns

### Phase 5: Guides
- Cookbook
- Troubleshooting

### Phase 6: Landing Page & Cross-linking
- Rewrite index.md with learning paths
- Add cross-references throughout
- Final review pass for consistency

## Success Criteria

1. Every command in the C source command tables is documented with syntax, description, and example
2. Every trigger is documented with firing conditions, entity bindings, and phrase meaning
3. Every ifcheck is documented with syntax, parameters, and entity type support
4. A new builder can follow the tutorials from zero to writing quest scripts
5. An experienced builder can look up any command/trigger/ifcheck quickly
6. Token and variable systems are prominently documented as central scripting tools
7. All content verified against current C source code, not copied from potentially outdated docs
8. Engine gaps discovered during verification are captured in `src/docs/TODO_SCRIPT_ENGINE_GAPS.md`
