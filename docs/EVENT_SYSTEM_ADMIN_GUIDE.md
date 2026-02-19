# Event System Admin Guide

This guide is for staff/admin operators using `evtedit` and `event` in-game.
It focuses on practical setup, runtime control, scheduling, and phase operations.

---

## 1) Core Model

The event system has two layers:

- **Definition layer** (`evtedit`): persistent template saved to `data/system/events.json`
- **Runtime layer** (`event`): active instances created from a definition

Think of `evtedit` as design-time and `event` as live operations.

---

## 2) Quick Start (Minimal Working Event)

1. Create and enter an event:

   ```
   evtedit create dragonhunt
   ```

2. Set baseline identity/schedule:

   ```
   type collection
   scope global
   schedule manual
   duration 60
   goal 100
   announce The Dragon Hunt has begun!
   endmsg The Dragon Hunt is complete.
   enabled on
   save
   ```

3. Start it live:

   ```
   event start dragonhunt
   ```

4. Verify runtime:

   ```
   event status
   event info dragonhunt
   ```

---

## 3) `evtedit` Command Surface (Definition Editing)

## Entry and selection

```
evtedit list
evtedit create <name>
evtedit <uid|name>
evtedit save
evtedit reload
```

## Core fields

```
name <text>
type <collection|invasion|boss|war-ffa|war-genocide|war-jihad|worldstate|custom>
enabled [on|off]
scope <global|area|region|zones|battlefield>
flags <flag>
```

## Schedule and eligibility fields

```
schedule <manual|recurring|calendar|worldcondition|triggered>
schedule list
schedule at <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>
schedule clear
interval <0-32767>          (minutes)
variance <0-32767>          (minutes; used by recurring)
duration <0-32767>          (minutes)
cooldown <0-32767>          (minutes)
minlevel <0-32767>
maxlevel <0-32767>
minplayers <0-32767>
maxplayers <0-32767>
goal <0-32767>
leaderrequired [on|off]     (invasion)
```

## Messaging/news fields

```
description <text>
announce <text>
endmsg <text>
joinmsg <text>
title <text>
summary <text>
newsslug <text>
newsannounce <text>
newsbody <text>
themetags <text>
```

## Brackets/progress/phases

```
spawnbrackets <spec>
collectionbrackets <spec>
bracketmode <auto_by_level|open|manual>
progressagg <shared|total|per_bracket_any|per_bracket|per_bracket_all_required>
phaseplan list
phaseplan clear
phaseplan add <name> [minutes] [scriptvnum]
phaseplan insert <index> <name> [minutes] [scriptvnum]
phaseplan set <index> <name> [minutes] [scriptvnum]
phaseplan name <index> <name>
phaseplan minutes <index> <minutes>
phaseplan script <index> <scriptvnum|0>
phaseplan remove <index>
rewardsuccess <scriptvnum|0>
rewardfail <scriptvnum|0>
comments <text>
```

---

## 4) Runtime `event` Command Surface

Available to players/staff (with staff-only restrictions for control actions):

```
event list
event info <uid|name>
event news <uid|name>
event status
event join [uid|name]
event leave [uid|name]
event enabled
event enable|disable
event start <uid|name>
event stop <uid|name>
event schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>
event tick
```

Notes:

- `start`, `stop`, `schedule`, `tick`, `enable`, `disable` require staff rank (`STAFF_CREATOR` or higher).
- Passive/worldstate events cannot be joined.

---

## 5) Scheduling: How It Actually Works

### Schedule modes

- `manual`: never auto-start; use `event start` or `event schedule`
- `recurring`: auto-start every `interval` minutes (with `variance` jitter)
- `calendar`: start at scheduled calendar time, then repeat every `interval` minutes
- `worldcondition`: currently definition-only (no automatic evaluator yet)
- `triggered`: currently definition-only (no autonomous trigger engine yet)

### Setting an absolute/relative start time

You set actual clock time with the runtime command, not in `evtedit`:

```
event schedule dragonhunt +30m
event schedule dragonhunt +2h
event schedule dragonhunt 2026-03-01 20:00
```

This updates the definition’s next scheduled start. You can inspect via:

```
event info dragonhunt
```

Look for:

- `Scheduled:` (one-shot pending start)
- `Next Auto:` (next repeating window)

You can also set an index-level schedule anchor directly in `evtedit`:

```
schedule at +30m
schedule at 2026-03-01 20:00
schedule clear
```

This is useful when defining events up front so ops can later just run `event start <name>` or enable runtime without re-entering schedule data.

### Recurring vs calendar

- **Recurring** computes next run from current runtime cadence (`interval ± variance`).
- **Calendar** uses the explicit scheduled anchor time, then advances by fixed `interval` steps.

---

## 6) Brackets and Aggregation

Bracket specs must be ordered and non-overlapping.

Valid token forms:

- `N-M`
- `N+`
- `N`

Examples:

```
spawnbrackets 1-50,51-90,91+
collectionbrackets 1-30;31-60;61+
```

Aggregation options:

- `shared` / `total`: pooled progress
- `per_bracket_any`: any bracket can satisfy goal
- `per_bracket` (alias behavior): per-bracket-any style evaluation
- `per_bracket_all_required`: every bracket must meet goal

---

## 7) Phase Plans (What They Are + How to Use)

`phaseplan` controls named runtime phases and optional timed/script transitions.
You build it with indexed subcommands rather than raw comma strings.

Example build:

```
phaseplan clear
phaseplan add prep 10 1200
phaseplan add battle 30
phaseplan add boss 0 1201
phaseplan list
```

Interpretation:

- `prep` at step 1: transition after 10 minutes, run script 1200 on entry
- `battle` at step 2: transition after 30 minutes
- `boss` at step 3: run script 1201, no timed transition unless a later step defines one

### How phases advance

- Event start tries to apply phase step 0.
- If no valid `phaseplan`, phase defaults to `active`.
- Timed transitions use each step’s `@minutes` value.
- Scripts (if present) execute when entering a phase step.

### Manual phase control

There is currently no direct player/staff `event` CLI subcommand for phase changes.
Manual phase control is available through scripts via `scriptcmd_event`:

```
event phase <event> next
event phase <event> set <phase_name>
```

Also available in scripts:

```
event progress <event> addkills <n>
event progress <event> additems <n>
event progress <event> setgoal <n>
event complete <event> [reason]
event fail <event> [reason]
```

---

## 8) Recommended Admin Workflows

### A) Fully manual event ops

Use when running GM-led events.

1. `schedule manual`
2. Keep `interval/variance` ignored or neutral
3. Launch with `event start <name>`
4. Close with `event stop <name>` (or natural completion)

### B) “Run tonight at 8pm, then every 2 hours”

1. `schedule calendar`
2. `interval 120`
3. `event schedule <name> YYYY-MM-DD 20:00`
4. Verify with `event info <name>`

### C) Rolling recurring event with jitter

1. `schedule recurring`
2. `interval 180`
3. `variance 30`
4. Verify periodic starts in `event status` and `event info`

---

## 9) Troubleshooting

### “I set `schedule calendar` and the display looked weird before.”

The schedule renderer now resolves event types/schedules by exact type name, not flag-style combinations.
If you still see stale display output, reload editor state:

```
evtedit reload
show
```

### “I can’t find where to set a date/time in EVTEdit.”

Expected: date/time is set via runtime command:

```
event schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>
```

### “worldcondition/triggered don’t auto-start.”

Current behavior: these schedule types are stored, but automatic evaluators are deferred/not wired yet.

### “My phase plan is rejected.”

Use subcommands and inspect with:

```
phaseplan list
```

If needed, reset and rebuild:

```
phaseplan clear
phaseplan add prep 10
phaseplan add battle 30
phaseplan add boss 0 1202
```

### “Players can’t join event.”

Check:

- Event/system enabled (`enabled on`, `event enabled`)
- Not `passive` and not `worldstate`
- Level/min/max player limits
- `exclusive_player` conflicts
- Bracket assignment constraints when `bracketmode auto_by_level`

---

## 10) Useful Verification Commands

After edits:

```
evtedit save
event list
event info <uid|name>
event news <uid|name>
event status
```

When testing scheduler changes quickly:

```
event schedule <uid|name> +1m
event tick
event status
```
