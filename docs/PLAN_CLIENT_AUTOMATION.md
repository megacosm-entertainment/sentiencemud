# Plan: Server-Side Client Automation

**Date:** March 15, 2026
**Status:** Design Draft

---

## Policy: Server-Side Automation Only

**All player automation must be server-side.** Client-side automation (Mudlet scripts,
MUSHclient triggers, TinTin++ aliases, etc.) is not a supported or sanctioned tool.

This is the foundational policy from which everything else follows:

- Staff has complete visibility into every automation tool every player has.
- Rate limits, human-present checks, and capability bounds are enforced by the server —
  not by the honor system.
- No client has a capability advantage, because the server exposes the only sanctioned
  automation surface.
- The arms race between players, and between players and content, is **contained within
  a staff-observable, server-enforced boundary**.

A Mudlet player's local Lua has no enforced limits and is invisible to staff. Server-side
Lua has hard limits on every axis and every script is readable by admins. The policy choice
to require server-side-only is what makes the anti-bot and anti-arms-race controls actually
work. Client-side enforcement is unverifiable; server-side enforcement is not.

**Tightening from this baseline**: The server provides the capability surface; separate
policy decisions (and tuning of the mechanical limits below) determine how much of that
surface is available to players and under what conditions.

---

## Problem Statement

Dedicated MUD clients (Mudlet, MUSHclient, TinTin++, Zmud) give their users substantial
mechanical advantages over players on browser, mobile, or basic telnet clients:

| Feature | Dedicated client | Basic client (no server support) |
|---|---|---|
| Aliases with arg substitution | Yes | No |
| Multi-command sequences | Yes | No |
| Speedwalking / named paths | Yes | No |
| Output triggers → commands | Yes | No |
| Timers | Yes | No |
| Session variables | Yes | No |

The server should provide all of these at parity, ensuring no client has a mechanical
advantage. Players who prefer richer clients can still use them — they just aren't required
for competitive participation.

The existing `alias.c` system provides 80 simple substitution aliases per character. This
plan extends it and adds the missing features.

---

## Feature Set

### 1. Enhanced Aliases (upgrade existing)

**Current**: First word matched, substitution replaces it, remaining args appended.

**Upgraded**:
- **Argument substitution**: `%1`, `%2`, ..., `%*` (all remaining args) in alias bodies.
- **Multi-command body**: `;` separator in alias body expands to sequential commands.
- **Variable references**: `$varname` in alias body expands from the variable store (see §5).

```
alias k    kill %1
alias kk   kill %1;bash %1;trip %1
alias kt   kill $target
alias home path go home
```

The alias cap increases from 80 to 200 to accommodate the richer use cases.

### 2. Multi-Command Input

Any raw input containing `;` is split and each segment is queued as a separate command.
This applies even without an alias — it's a base input layer feature.

```
> n;n;e;say hello
```

Queued commands execute at normal command tick rate — not instantaneously. A 200-command
depth limit on the input queue prevents abuse. Characters in combat or special states (e.g.,
crafting synthesis) cannot use movement commands from the queue while those states block
normal movement.

### 3. Speedwalking

**Inline numeric repeat**: A token of the form `[0-9]+[nsewud]` in a multi-command sequence
(or standalone) expands to N repetitions of that direction.

```
> 5n;2e;3s
Equivalent to: n;n;n;n;n;e;e;s;s;s
```

This is handled in the pre-processor before alias expansion.

**Named paths**: Stored sequences of movement, recalled by name.

```
path record home      Start recording movement
... (walk around) ...
path stop             Save the recorded path as "home"

path go home          Execute the stored path
path list             Show all saved paths
path save market 3n;2e;n   Define a path directly
path delete home      Remove a path
path show home        Display the direction sequence
```

Named paths are character-persisted (up to 30 paths). Executing a path queues all its
commands into the normal command queue — they inherit the same rate limiting.

**Path record mode**: When recording, movement commands are captured to a buffer in addition
to being executed normally. `path stop` finalizes and names the buffer. Recording mode is
stored on `DESCRIPTOR_DATA` (session state, not persisted).

### 4. Triggers

A trigger watches for a substring or glob pattern in output sent to the player and fires
one command in response.

```
trigger add hungry    "You are hungry"      eat bread
trigger add lowerhp   "* desperately wounded *"   recall
trigger add lowmana   "Your mana drops to *%"     rest

trigger list
trigger delete hungry
trigger enable hungry
trigger disable hungry
trigger show hungry
```

**Pattern types**:
- **Substring**: literal text match anywhere in the output line (default).
- **Glob**: `*` matches any sequence of characters within a line.
- No regular expressions — too complex to sandbox safely.

**Rate limiting**:
- Each trigger has a per-trigger cooldown: minimum 500ms between fires of the same trigger.
- Player-wide trigger rate cap: 5 trigger fires per second across all triggers.
- If the cap is hit, pending triggers are discarded (not queued).

**Loop prevention**:
- Commands sent by a trigger execute with a `from_trigger` flag on the descriptor.
- When `from_trigger` is set, trigger matching is skipped for that command's output.
  This breaks trigger → command → output → trigger → ... chains at one level.
- A trigger whose response is another trigger-fire-eligible command does not cascade.

**Limits**: Maximum 20 triggers per character.

**Persistence**: Triggers are saved to the character JSON.

### 5. Session Variables

A simple string key-value store per character. Variable references (`$name`) expand inside
alias bodies, trigger responses, timer commands, and path definitions.

```
var set target goblin
var set home_room "The Crossroads"

alias k    kill $target
alias kk   kill $target;bash $target;trip $target
```

**Built-in read-only variables** (populated automatically by the server):

| Variable | Set when |
|---|---|
| `$target` | You attack a character; updated each time |
| `$last_tell` | A character sends you a tell |
| `$last_say` | Last character who said something in your room |

Built-in variables can be overridden by the player with `var set`. They reset to automatic
behavior when deleted with `var delete`.

**Commands**:
```
var                       List all variables
var set <name> <value>    Set a variable
var get <name>            Show a variable's value
var delete <name>         Remove a variable
var clear                 Remove all player-defined variables
```

**Limits**: Maximum 50 variables per character (built-ins do not count against the cap).

**Persistence**: Variables are saved to the character JSON.

### 6. Command Timers

A timer fires a command at a repeating interval.

```
timer add rebuff   120  cast 'refresh'
timer add food     900  eat bread
timer list
timer delete rebuff
timer enable rebuff
timer disable rebuff
timer show rebuff
```

**Interval**: Specified in seconds. Minimum interval: **30 seconds**. This allows buff
maintenance and sustain commands but prevents tight automation loops.

**Human-present check**: Timers are paused for a character if no player input has been
received in the last **5 minutes**. Timers resume when the player sends any input. This
prevents overnight automation.

The human-present threshold is a configurable game setting (`timer_afk_seconds`, default 300).

**Command restrictions**: Timer commands go through the normal `interpret()` path and are
subject to all standard command restrictions (position, level, cooldowns, states). A timer
cannot bypass anything the player couldn't do manually.

**Limits**: Maximum 10 timers per character.

**Persistence**: Timers are **not** persisted — they are session-only and must be
re-established on login. This is intentional: persistent timers would be nearly
indistinguishable from bots left running permanently.

---

## Architecture

### Input Pre-Processor Pipeline

All player input passes through a pre-processor chain before reaching `interpret()`.
The chain runs in this order:

1. **Multi-command split**: If input contains `;`, split into segments and enqueue each.
   Return after enqueuing — the first segment is taken for immediate processing.
2. **Speedwalk expansion**: Check for `[0-9]+[nsewud]` tokens and expand to repeated
   direction tokens, inserting into the current input.
3. **Variable expansion**: Replace `$varname` references with stored values.
4. **Alias substitution**: Match first word against alias list, substitute with argument
   expansion (`%1`, `%2`, `%*`). Multi-command alias bodies are split back into the queue.

The pre-processor lives in a new `src/client_automation.c` (see §Data Structures).

### Command Queue

`DESCRIPTOR_DATA` gains a `cmd_queue` field: a simple LLIST of strings. The main game
loop drains one item per tick per descriptor (same as today for single commands). The queue
has a maximum depth of 200 items — if exceeded, further queue additions are dropped and the
player receives a warning.

### Trigger Engine

After output is assembled for a descriptor (the buffer passed to `write_to_buffer()` or
equivalent), the trigger engine scans each line against the player's trigger list. Matches
are rate-checked, then the response command is prepended to the descriptor's `cmd_queue`
with `from_trigger = true` on the descriptor flag.

`from_trigger` is a boolean on `DESCRIPTOR_DATA`. It is cleared after each command that
was dispatched with it set. While set, trigger matching is skipped for that command's
output.

### Timer Loop

`update_timers()` is called once per second from the main game pulse (or a 1-second tick
bucket). It iterates each connected descriptor's timer list, checks human-present timestamp,
checks `next_fire`, and enqueues any due timers' commands.

Timer commands are enqueued with a `from_timer = true` flag, which prevents timer-fired
commands from triggering the human-present timestamp update (so a timer alone cannot keep
the human-present window alive).

---

## Data Structures

### New: `src/client_automation.c` / `src/client_automation.h`

Houses the pre-processor pipeline, trigger engine, and timer update function.

```c
/* Trigger */
typedef struct trigger_data TRIGGER_DATA;
struct trigger_data {
    TRIGGER_DATA *next;
    bool valid;
    char *name;             /* Identifier */
    char *pattern;          /* Text pattern (substring or glob) */
    bool glob;              /* True if pattern uses * wildcards */
    bool case_sensitive;    /* Default: false */
    char *response;         /* Command to fire */
    bool enabled;
    long fire_count;        /* Lifetime fire count */
    time_t last_fire;       /* For per-trigger rate limiting */
};

/* Timer */
typedef struct timer_data TIMER_DATA;
struct timer_data {
    TIMER_DATA *next;
    bool valid;
    char *name;             /* Identifier */
    char *command;          /* Command to execute */
    int interval;           /* Seconds between fires */
    time_t next_fire;
    bool enabled;
};

/* Named path */
typedef struct path_data PATH_DATA;
struct path_data {
    PATH_DATA *next;
    bool valid;
    char *name;             /* Identifier */
    char *directions;       /* `;`-separated direction sequence */
};

/* Variable */
typedef struct var_data VAR_DATA;
struct var_data {
    VAR_DATA *next;
    bool valid;
    char *name;
    char *value;
};
```

### Modifications to `PC_DATA` (merc.h)

```c
struct pc_data {
    /* ... existing fields ... */

    /* Extended alias (upgrade from fixed array) */
    /* Keep existing alias[MAX_ALIAS] / alias_sub[MAX_ALIAS] for migration,
       migrate to LLIST when class/job JSON persistence refactor lands */

    /* New automation fields */
    LLIST *triggers;        /* TRIGGER_DATA list */
    LLIST *paths;           /* PATH_DATA list */
    LLIST *variables;       /* VAR_DATA list (persisted) */
    /* Timers are on DESCRIPTOR_DATA — session-only */
};
```

### Modifications to `DESCRIPTOR_DATA` (merc.h)

```c
struct descriptor_data {
    /* ... existing fields ... */
    LLIST *cmd_queue;           /* Queued multi-command segments */
    LLIST *timers;              /* TIMER_DATA list — session-only */
    bool from_trigger;          /* Current command was fired by a trigger */
    bool from_timer;            /* Current command was fired by a timer */
    time_t last_input_time;     /* For human-present check */
    bool recording_path;        /* In path record mode */
    LLIST *path_record_buf;     /* Accumulated directions during record */
};
```

---

## Commands

### Aliases (upgraded)
```
alias                          List all aliases
alias <name>                   Show alias expansion
alias <name> <expansion>       Create or redefine alias
unalias <name>                 Remove alias
unalias *                      Remove all aliases
```

### Speedwalk / Paths
```
path                           List all named paths
path list                      Same
path save <name> <directions>  Define a path directly
path record <name>             Start recording movement
path stop                      Stop recording, save with given name
path go <name>                 Execute a named path
path show <name>               Display the direction sequence
path delete <name>             Remove a path
```

### Triggers
```
trigger                        List all triggers
trigger list                   Same
trigger add <name> <pattern> <command>   Add a trigger
trigger delete <name>          Remove a trigger
trigger enable <name>          Enable a trigger
trigger disable <name>         Disable a trigger
trigger show <name>            Show trigger details including fire count
trigger clear                  Remove all triggers
```

### Variables
```
var                            List all variables
var set <name> <value>         Set a variable
var get <name>                 Show a variable
var delete <name>              Remove a variable
var clear                      Remove all player-defined variables
```

### Timers
```
timer                          List all timers
timer list                     Same
timer add <name> <seconds> <command>   Add a timer
timer delete <name>            Remove a timer
timer enable <name>            Enable a timer
timer disable <name>           Disable a timer
timer show <name>              Show timer details
timer clear                    Remove all timers
```

---

## Persistence (JSON)

**Persisted in character JSON** (new section):

```json
{
  "automation": {
    "triggers": [
      {
        "name": "hungry",
        "pattern": "You are hungry",
        "response": "eat bread",
        "glob": false,
        "case_sensitive": false,
        "enabled": true
      }
    ],
    "paths": [
      {
        "name": "home",
        "directions": "3n;2e;n"
      }
    ],
    "variables": [
      { "name": "target", "value": "goblin" }
    ]
  }
}
```

**Not persisted** (session-only):
- Timers — must be re-established each session
- Command queue (`cmd_queue`)
- Path recording state

---

## Limits Summary

| Feature | Limit | Notes |
|---|---|---|
| Aliases | 200 | Up from 80 |
| Triggers | 20 | |
| Trigger rate | 5/second (player) | Per-trigger minimum: 500ms |
| Named paths | 30 | |
| Variables | 50 | Built-ins not counted |
| Timers | 10 | |
| Timer minimum interval | 30 seconds | |
| Timer AFK pause | 5 minutes no input | Configurable game setting |
| Command queue depth | 200 | |

---

## Implementation Phases

### Phase 1 — Input pipeline and alias upgrade (low risk)
- Add `;` multi-command split and command queue to `DESCRIPTOR_DATA`
- Add speedwalk numeric repeat expansion
- Upgrade `substitute_alias()` for `%1`/`%2`/`%*` and `;` in bodies
- Update `do_alias` cap to 200

### Phase 2 — Variables and paths (low risk)
- Add `VAR_DATA` to `PC_DATA` and JSON persistence
- Add variable expansion (`$name`) to pre-processor
- Add `PATH_DATA` to `PC_DATA` and JSON persistence
- Implement `do_path` and path record mode on descriptor

### Phase 3 — Triggers (medium risk)
- Add `TRIGGER_DATA` to `PC_DATA` and JSON persistence
- Add trigger engine to output pipeline
- Add `from_trigger` flag and loop prevention
- Implement rate limiting
- Implement `do_trigger`

### Phase 4 — Timers (medium risk)
- Add `TIMER_DATA` to `DESCRIPTOR_DATA` (session-only)
- Implement `update_timers()` called from main pulse
- Add human-present check and `last_input_time` tracking
- Implement `do_timer`

---

## Open Questions

1. **Trigger pattern matching scope**: Should triggers only match against the first line of
   multi-line output, or scan all lines sent in one write? Scanning all lines is more useful
   but slightly more expensive.

2. **Trigger pattern for colored output**: Output includes ANSI color codes. Should patterns
   match against the raw (colored) string, or against a stripped version? Stripped is almost
   certainly the right choice — players can't predict exact color codes.

3. **Variable expansion in path directions**: Should `$varname` expand inside path direction
   strings? This enables dynamic paths but adds complexity.

4. **`path record` and combat**: If a player starts a path record and gets into combat, what
   happens? Probably just record anyway — let the player decide if the path is useful.

5. **Alias vs trigger for `$target`**: Built-in `$target` auto-updates on attack. Should
   `$last` (last target killed) also be tracked as a built-in? Useful for loot commands.

6. **Timer persistence reconsideration**: A middle path — save timer definitions but default
   them to disabled on login. Players must manually re-enable, which preserves human-intent
   requirement without forcing full re-definition.

7. **Staff visibility**: Should staff be able to see a player's triggers/timers/aliases for
   abuse review? Probably yes — a `spy` or `debug` command showing automation state.

---

## Human-Present Gating

All automation in this system — aliases, multi-command queues, triggers, timers — is subject
to the same human-present check described in `PLAN_PLAYER_LUA_SCRIPTING.md`. Only real
keystrokes from the network descriptor update `last_input_time`. When no real input has
been received for `automation_afk_seconds` (default: 300), all automated execution suspends.

See `PLAN_PLAYER_LUA_SCRIPTING.md` for the full human-present, throughput cap, and staff
visibility architecture that governs both this system and Lua scripting.

---

## See Also

- `alias.c` — Existing alias implementation (upgraded by Phase 1)
- `PLAN_PLAYER_LUA_SCRIPTING.md` — Lua scripting environment, sandbox, anti-bot controls
- `PLAN_command_enhancements.md` — CMD_DATA-level aliasing (separate concern; staff-side)
- `PLAN_pubsub_communication.md` — Channel system (triggers can react to channel output)
