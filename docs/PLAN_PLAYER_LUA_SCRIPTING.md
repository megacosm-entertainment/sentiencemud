# Plan: Player Lua Scripting Environment

**Date:** March 15, 2026
**Status:** Design Draft
**Depends on:** `PLAN_CLIENT_AUTOMATION.md`

---

## Policy: Server-Side Automation Only

All player automation — including Lua scripting — must be **server-side**. Client-side
scripting via Mudlet, TinTin++, MUSHclient, or any other client is not a supported or
sanctioned tool.

This policy is the reason the anti-bot and anti-arms-race controls in this document are
actually enforceable. Client-side scripts are invisible to staff and subject to no server
limits. Server-side scripts are readable by admins, rate-limited by the server, and
paused by the human-present check — none of which can be bypassed from the client side.

The goal is a **contained, observable, auditable** automation surface. Staff can see every
script every player has written. Content can be designed against known script capabilities.
Mechanical limits apply uniformly. The arms race — between players, and between players and
content — happens within bounds the server controls.

---

## Problem Statement

Dedicated MUD clients (Mudlet, TinTin++) provide full Lua environments. This plan brings
equivalent capability to the server level so all players have equal access regardless of
client — and so that access is fully controlled by the server.

The harder design problem is **preventing bots** while allowing genuine automation. The key
insight is:

> The problem is **unattended play**, not automation capability.
> A Lua script running while a player is present and watching is better tooling.
> That same script running for 8 hours while the player sleeps is a bot.

This reframes the design. Rather than limiting what scripts can *do*, controls focus on
whether a **human is demonstrably present**. A player who scripts their entire combat
rotation but checks in every few minutes is a different case from a gather bot running
overnight. The architecture enforces presence; game design enforces engagement.

---

## Design Principles

1. **Presence-gated**: All automated execution pauses when no human input is detected.
   Scripts cannot keep themselves alive — `send()` does not count as human input.

2. **Event-driven, not polling**: Scripts are invoked by events and must complete quickly.
   There are no persistent loops, no `while true`. The sandbox enforces this with instruction
   limits.

3. **Transparent to staff**: Automation state is visible to admins. Session statistics make
   bot-like behavior detectable.

4. **Minimal attack surface**: The Lua environment is a restricted sandbox. No file I/O,
   no network, no server internals. Only a controlled API surface.

5. **Throughput-capped**: All command sources — manual, alias, trigger, timer, Lua — share
   a single rate limit. Even a maximally-automated character cannot issue commands faster
   than the cap allows.

---

## Relationship to `PLAN_CLIENT_AUTOMATION.md`

The client automation plan defines the basic system: enhanced aliases, multi-command input,
speedwalking, triggers, timers, and variables. Lua is a **power tier** layered on top of it:

- Triggers can have either a **command string** (simple) or a **script reference** (Lua).
- Timers can have either a **command string** (simple) or a **script reference** (Lua).
- Aliases can have either a **command string** (simple) or a **script reference** (Lua).
- Scripts can also be invoked directly with `script run <name>`.

A player who doesn't want to write Lua never needs to. The basic system is fully functional
without it. Lua adds conditionals, state, computed responses, and complex event handling.

---

## Human-Present System

This is the central anti-bot mechanism. It applies to all automation, not just Lua.

### Timestamp Update

`DESCRIPTOR_DATA` gains `last_input_time` (a `time_t`). This timestamp is updated **only**
by actual keystrokes received from the network — bytes arriving on the socket. It is never
updated by:
- Commands enqueued by the multi-command queue
- Trigger responses
- Timer fires
- Lua `send()` calls
- Any automated source

### Human-Present Window

`automation_afk_seconds` is a game-wide setting (default: **300 seconds / 5 minutes**).

When `(now - last_input_time) > automation_afk_seconds`:
- All trigger matching is suppressed
- All timer fires are suppressed
- All Lua script invocations are suppressed
- The command queue still drains (already-queued commands from before the window expired
  continue to execute; new queuing from automation stops)

When the player sends any real input, automation resumes immediately.

### Why This Is Effective

A player running an overnight gather bot must either:
1. Accept that automation stops after 5 minutes of inactivity, or
2. Write a script that sends fake "human" input — but `send()` doesn't update the timestamp.
   There is no API call in the Lua environment that updates `last_input_time`. Only socket
   bytes do.

A player checking in every few minutes to send a command is, for the game's purposes,
present. This is the intended threshold.

---

## Throughput Limits

All command sources share a **single rate limit per character**:

- **Limit**: 60 commands per minute (1/second average)
- **Burst**: Up to 10 commands in a single second, then the rate gate applies
- **Enforcement**: Commands that exceed the rate are queued (not dropped), but the queue
  has a maximum depth of 200. New commands are dropped and the player is notified if the
  queue is full.

A real typist averages 20-30 commands/minute during active play. The 60 cap provides
comfortable headroom for human+automation hybrids while making pure bots visibly constrained.

The rate limit is per-character, not per-source. A player cannot "spend" their rate limit
on automation and then manually type commands at full speed on top of it.

---

## Sandbox Architecture

Sentience embeds **PUC Lua 5.4** via `liblua`. One Lua state (`lua_State`) is created per
connected descriptor and destroyed on disconnect. States are not shared between players.

### Allowed Standard Libraries

| Library | Allowed | Notes |
|---|---|---|
| `string` | Yes | Full |
| `table` | Yes | Full |
| `math` | Yes | Full |
| `utf8` | Yes | Full |
| `type`, `pairs`, `ipairs`, `next`, `select` | Yes | Base globals |
| `tostring`, `tonumber`, `pcall`, `xpcall`, `error`, `assert` | Yes | Base globals |
| `io` | No | File access |
| `os` | No | System access |
| `file` | No | File access |
| `require`, `dofile`, `loadfile`, `package` | No | Module loading |
| `debug` | No | Can bypass sandbox |
| `coroutine` | No | Can sidestep instruction limits |
| `load`, `loadstring` | No | Can load arbitrary code at runtime |

### Instruction Limit

A Lua debug count hook limits each script invocation to **100,000 instructions**. If a
script reaches the limit, it is killed with a sandboxed error that is displayed to the
player:

```
[Script 'autoheal'] Execution limit reached. Script terminated.
```

This prevents infinite loops and `while true do` patterns from hanging the server.

### Memory Limit

A custom Lua allocator tracks per-state memory usage. Each player's Lua state is limited
to **2MB total**. Attempting to allocate beyond the limit raises a Lua memory error.

Per-script invocation, allocation is bounded to **256KB** via a sub-allocator. Scripts that
try to build large tables or strings in a single invocation hit this limit first.

### Error Handling

All script invocations are wrapped in `lua_pcall`. Errors are caught and displayed to the
player in a formatted message. They do not propagate to the game engine.

---

## Sentience API (Lua-side)

All API access is via a global table `S` (for Sentience) injected into each script's
environment.

### Character State (read-only)

```lua
S.ch.name          -- string: character name
S.ch.hp            -- integer: current HP
S.ch.max_hp        -- integer: max HP
S.ch.mana          -- integer: current mana
S.ch.max_mana      -- integer: max mana
S.ch.move          -- integer: current movement
S.ch.max_move      -- integer: max movement
S.ch.position      -- string: "standing", "sitting", "sleeping", "fighting", "resting"
S.ch.level         -- integer: effective level
S.ch.job           -- string: current active job name (e.g., "blacksmith")
S.ch.room_name     -- string: current room name (display name)
S.ch.affects       -- table (array): list of active affect names
S.ch.fighting      -- string or nil: name of current combat target, nil if not fighting
```

These fields are populated fresh at the start of each script invocation. They are snapshots;
modifying them has no effect on the game.

### Action

```lua
S.send(command)        -- Queue one command for execution (subject to rate limits)
S.echo(text)           -- Display text to the player only (no in-game effect)
```

`S.send()` is the only way a script affects the game. It inserts a command into the
character's command queue with `from_script = true`. This bypasses alias expansion but goes
through `interpret()` normally — all standard checks apply.

`S.echo()` is purely cosmetic output to the player's terminal.

### Variables

```lua
S.var.get(name)          -- string or nil: retrieve a stored variable
S.var.set(name, value)   -- set a variable (persisted to character JSON)
S.var.delete(name)       -- remove a variable
```

Variable access goes through the same `VAR_DATA` store used by the basic automation system.
Scripts and aliases share the same variable namespace.

### Event Data

When a script is invoked by a trigger, timer, alias, or direct call, an `S.event` table is
populated:

```lua
S.event.type          -- string: "trigger", "timer", "alias", "manual"
S.event.name          -- string: name of the trigger/timer/alias that fired
S.event.line          -- string or nil: the output line that matched a trigger
S.event.captures      -- table or nil: capture groups if a glob pattern matched
```

For trigger events, `S.event.line` contains the full output line that matched. Glob captures
(from `*` wildcards) are available in `S.event.captures` as an ordered array.

### Utility

```lua
S.match(pattern, text)    -- boolean: true if text matches glob pattern (same as trigger matching)
S.gmatch(pattern, text)   -- string or nil: first captured group, or nil
S.trim(text)              -- string: whitespace-trimmed
S.split(text, delim)      -- table: split text on delimiter
S.contains(text, substr)  -- boolean: substring check
```

### What Is Intentionally Absent

- No access to other characters' state
- No access to room exits or map data
- No access to object or NPC lists in the room
- No access to economy data (prices, shop inventories)
- No access to timing internals (cannot query `last_input_time`)
- No file or network I/O of any kind

The API surface is intentionally narrow. It can be expanded in later phases as specific
use cases emerge.

---

## Script Storage

Scripts are named Lua chunks stored per-character in JSON:

```json
{
  "automation": {
    "scripts": [
      {
        "name": "autoheal",
        "source": "if S.ch.hp < S.ch.max_hp * 0.3 then S.send('cast heal self') end",
        "enabled": true
      },
      {
        "name": "combat_rotation",
        "source": "local t = S.event.line\nif S.contains(t, 'You swing') then S.send('bash ' .. (S.var.get('target') or '')) end",
        "enabled": true
      }
    ]
  }
}
```

**Maximum scripts**: 50 per character. Total stored script source size: 100KB per character.

Scripts are loaded into the Lua state at connection time and are available for the session.

---

## Commands

### Script Management

```
script                        List all scripts with enabled/disabled status
script list                   Same
script show <name>            Display script source
script add <name> <source>    Add a one-line script inline
script edit <name>            Open multi-line editor for script source
script delete <name>          Remove a script
script enable <name>          Enable a script
script disable <name>         Disable a script
script run <name>             Invoke a script manually (with S.event.type = "manual")
script rename <name> <new>    Rename a script
script clear                  Remove all scripts
```

### Connecting Scripts to Triggers, Timers, and Aliases

The existing trigger/timer/alias commands gain an optional `script` keyword:

```
trigger add combat_watch "* attacks you *" script combat_rotation
timer add rebuff 120 script refresh_buffs
alias heal script autoheal
```

When `script <name>` is used instead of a command string, the named script is invoked
instead of a command being queued.

---

## Multi-Line Editor

`script edit <name>` opens an in-game multi-line text editor (same mechanism as the note
editor, adapted for code). The editor supports:

```
/.show          Display current content with line numbers
/.insert <n>    Insert a line before line N
/.delete <n>    Delete line N
/.replace <n>   Replace line N (prompts for new content)
/.save          Compile and save the script
/.abort         Discard changes
/.test          Compile-check without saving
```

`/.save` and `/.test` compile the Lua source (`luaL_loadstring`) and report syntax errors
before saving. This prevents storing broken scripts.

For players on clients with better input support, the WebSocket protocol can support a
dedicated `SCRIPT_UPLOAD` message type that sends the full source in a single JSON payload,
bypassing the line-by-line editor entirely.

---

## Staff Visibility and Audit Tools

### Viewing Player Automation State

```
wizscript <player>            Show all scripts, triggers, timers, aliases
wizscript <player> scripts    Show scripts only
wizscript <player> stats      Show session automation statistics
```

### Session Statistics (stored per session, visible to staff)

```
Automation stats for Thorin (session: 3h 24m)
  Total commands:     1,842
  Manual commands:    412  (22%)
  Automated commands: 1,430 (78%)
    From aliases:     310
    From triggers:    890
    From timers:      180
    From scripts:     50
  Script fires:       212
  Rate limit hits:    0
  AFK suspension:     2 events (8m 15s total)
  Human-present ratio: 96.0%
```

High automation ratios combined with low human-present ratio and perfectly regular timing
are signals admins can act on. The stats are not automated enforcement — they're diagnostic
tools for human review.

### Bot Score (Heuristic)

A non-binding `bot_score` (0-100) is derivable from session stats:

- Ratio of automated to manual commands (weighted heavily)
- Variance in inter-command timing (real humans have irregular timing)
- AFK suspension events and duration
- Scripts firing during near-absent periods

A bot score of 80+ might trigger a soft alert to online staff. The score is advisory only —
staff still review and decide.

---

## Implementation Phases

### Phase 1 — Lua embedding and sandbox (medium risk)
- Add PUC Lua 5.4 as a git submodule in `.deps/`
- Update `Makefile` and `CMakeLists.txt`
- Create `src/lua_sandbox.c` / `src/lua_sandbox.h`
- Implement per-descriptor Lua state lifecycle (create on connect, destroy on disconnect)
- Implement instruction limit (debug hook), memory limit (custom allocator)
- Implement restricted environment (blocked libraries removed from state)
- Implement `S` API: `S.ch.*`, `S.send()`, `S.echo()`, `S.var.*`, `S.event`, utilities
- Script storage in `PC_DATA` and JSON (`automation.scripts`)
- `do_script` command family (list, add, delete, enable, disable, run)

### Phase 2 — Script editor (low risk)
- Multi-line in-game editor for script source
- Compile-check on save (`.test`, `.save`)
- WebSocket `SCRIPT_UPLOAD` message type

### Phase 3 — Integration with automation system (low risk)
- `trigger add <name> <pattern> script <script_name>`
- `timer add <name> <interval> script <script_name>`
- `alias <name> script <script_name>`
- `S.event` population for trigger/timer-invoked scripts

### Phase 4 — Staff tools (low risk)
- `wizscript` command family
- Per-session automation statistics tracking
- Bot score heuristic

---

## Arms Race Mitigations

### The Problem

Even with server-side-only automation, two arms race dynamics exist:

**Player vs player (PvP)**: A script that fires a flee trigger in <1ms beats a human who
takes 300-500ms to read output and type a response. If Player A scripts their combat and
Player B doesn't, Player A wins regardless of skill. Player B is then incentivized to
script defensively. This escalates until PvP is effectively script vs script.

**Players vs content**: Staff designs an encounter. Players script around it using trigger
patterns in the encounter's output. Staff patches the output text. Players update the
triggers. This is a losing cycle for content design.

### Why Server-Side-Only Helps

The server-side-only policy changes the character of both arms races without eliminating
them. Because all scripts are visible to staff:

- When a script trivializes content, staff can see exactly what pattern it exploits.
- Staff can tune the mechanical limits (reaction floor, rate caps) targeting observed abuse
  without guessing.
- The arms race happens within a controlled, observable surface instead of in invisible
  client-side automation.

This doesn't prevent the arms race. It makes it manageable.

### Mechanical Mitigations

**1. Reaction time floor (combat)**

When output is sent to a player during combat, a minimum delay applies before any response
command is accepted — from any source, including scripts. Tentative default: 100ms.

A human reads and responds in 300-500ms. A script would respond in <1ms without this floor.
The floor doesn't close the gap completely but makes it significantly smaller and configurable.

The floor applies only during combat (`position == fighting`). Outside combat, triggers and
aliases respond at normal speed (speedwalking, navigation, crafting triggers are unaffected).

The floor value is a game setting (`combat_trigger_delay_ms`). It can be tuned without a
code change.

**2. Round-gated combat commands**

During combat, the number of player-initiated commands accepted per round is capped at the
same level regardless of source (manual, alias, trigger, script). This prevents scripts
from "spending" multiple command slots in a single round that a human could not.

The round gate is separate from the global throughput cap (60 commands/minute). A round
gate of 2 commands/round means no one — human or script — can issue more than 2 commands
per combat round. Scripts cannot achieve a higher effective action economy.

**3. Variable output in content design**

This is the most durable content-side mitigation. Encounters whose significant output lines
are variable, contextual, or non-templatable resist pattern-matching triggers at the design
level.

Principles for anti-scripting content design:
- Boss ability announcements include variable text, character-name insertion, or
  contextually-derived phrasing that changes each encounter.
- Critical decision points present information in formats that require comprehension, not
  matching (e.g., riddle prompts, contextual warnings, multi-line state descriptions).
- High-value actions (rare node collectibles, legendary fish) can require non-template input
  (a specific contextual response) at low frequency to confirm presence.

This is a design guideline, not a system enforcement. It does not affect normal content;
it informs how high-stakes content is authored.

**4. API surface limits**

The Lua API (`S.*`) intentionally exposes player character state but not external state:

- No enemy HP, position, or affects
- No room occupant lists
- No NPC action or state data
- No other player's state

Scripts that react to parsed output text can still try to extract this information from
what the server sends. The reaction time floor and variable output principle are the
mitigations for that. The API boundary prevents scripts from having *direct* access to
game state they'd need to optimally automate against.

**5. Staff audit as the primary PvP control**

For PvP specifically, scripted automation is a policy enforcement problem as much as a
mechanical one. Staff can review scripts of players who are winning suspiciously
consistently, or who report being beaten by inhuman reaction times. The `wizscript` audit
tools exist for this purpose.

The mechanical controls (reaction floor, round gate) set a baseline fairness level. Staff
review handles edge cases and graduated enforcement.

### What This Does Not Solve

- Scripts that are sophisticated but legitimate will still outperform unscripted play. This
  is accepted. The goal is not to make scripting ineffective — it's to prevent unattended
  automation and to ensure the arms race stays within observable bounds.
- Players will attempt to extract combat state from output text and script against it. The
  reaction time floor and variable output mitigate this; they don't eliminate it.
- The "right" values for reaction floor, round gate, and human-present timeout require
  empirical tuning against real player behavior. The design provides the knobs; the game
  operation tunes them.

---

## Open Questions

1. **Coroutines**: Coroutines are excluded due to instruction-limit bypass risk. Is there a
   safe way to support them? (e.g., limit coroutine resume count per invocation.) They'd
   enable cleaner stateful script patterns.

2. **Shared script library**: Should there be a server-provided `S.lib` table of common
   helpers (e.g., string patterns for common output formats, HP threshold helpers)? This
   would reduce boilerplate but creates a maintenance surface.

3. **Script import**: Should scripts be able to call other scripts via `S.run('name')`?
   Enables composition but risks creating complex dependency graphs. A call depth limit (e.g.
   3) would make this safe.

4. **Room data access**: `S.ch.room_name` is read-only text. Should room exits or room flags
   be exposed? Useful for navigation scripts (auto-flee toward exits), but enables more
   aggressive automation of movement.

5. **Affect details**: `S.ch.affects` is an array of affect names. Should affect duration
   (in ticks) be exposed? Useful for rebuff timers, but enables precise timing automation.

6. **Human-present threshold**: Is 5 minutes the right default? Too long = bots check in
   rarely and still run; too short = legitimate players who AFK briefly lose automation.
   Should this be per-player configurable (within staff-set bounds)?

7. **Persistent Lua state**: Currently each script invocation is stateless (fresh environment
   each call). If scripts need to track state across calls (e.g., combo step counters), they
   must use `S.var`. Is this sufficient, or should per-script persistent Lua globals be
   supported? Risk: memory leak if scripts accumulate large state.

---

## See Also

- `PLAN_CLIENT_AUTOMATION.md` — Basic automation system this builds upon
- `PLAN_WEBSOCKET_SESSION_RESUME.md` — WebSocket layer (script upload via WebSocket)
- `src/scripts.c`, `src/scripts.h` — Existing game scripting engine (separate; not Lua)
