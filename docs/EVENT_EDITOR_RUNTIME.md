# Event Editor Runtime Reference

This document captures the **implemented** behavior of the event editor/runtime as of the current migration pass.

## Scope

Runtime and editor logic lives in:
- `src/editors/events/evtedit.c`
- public API declarations in `src/event_types.h`

Script-side integration exists via:
- `src/script_commands.c`
- `src/script_ifc.c`
- `src/script_expand.c`
- `src/script_const.c`

## Event Definition Surface (EVTEdit)

Editable fields include:
- Identity/meta: name, title, summary, type, scope, enabled, flags, comments
- Schedule: schedule type, interval, variance, duration, cooldown
- Eligibility: min/max level, min/max players
- Completion controls: goal, leaderrequired
- Messaging/news: description, announce, endmsg, joinmsg, newsslug, newsannounce, newsbody, themetags
- Brackets/progress: spawnbrackets, collectionbrackets, bracketmode, progressagg

## Validation Rules (Current)

### Bracket spec format
`spawnbrackets` and `collectionbrackets` are validated as ordered, non-overlapping ranges.

Accepted token forms:
- `N-M`
- `N+`
- `N`

Examples:
- `1-50, 51-90, 91+`
- `1-30;31-60;61+`

### Bracket mode
Allowed values:
- `auto_by_level`
- `open`
- `manual`

### Progress aggregation
Allowed values:
- `total`
- `per_bracket`
- `per_bracket_all_required`

## Runtime Scheduling

### Implemented schedule behaviors
- `manual`: explicit starts only
- `recurring`: auto-start from `next_auto_time` based on interval (+variance)
- `calendar`: starts from `scheduled_time`, then repeats using `sched_interval` minutes

When `event schedule <event> ...` is used, `scheduled_time` is set and `next_auto_time` is cleared to ensure fresh anchoring.

### Deferred schedule behaviors
- `worldcondition`
- `triggered`

These are currently definition values only; no autonomous condition evaluator has been wired in runtime yet.

## Runtime Progress & Completion

Implemented progression paths:
- Collection: turn-ins (`event_progress_record_collection_turnin`)
- Invasion: NPC kills + optional leader phase and leader completion
- War variants (`war-ffa`, `war-genocide`, `war-jihad`): player kill score progression
- Boss: NPC kill progression with goal-based completion (default goal fallback = 1)

Bracket-aware aggregation is applied when configured (`per_bracket*`).

## Participation Rules

Players cannot join:
- passive events (flag `passive`)
- `worldstate` events

These are status/runtime-driven events, not participant-driven contests.

## Runtime/Event Commands

### Player/staff command (`event`)
Current subcommands:
- `list`, `info`, `news`, `status`
- `join`, `leave`
- `enabled`, `enable`, `disable`
- `start`, `stop`, `schedule`, `tick`

### Script command surface (`event` in scripts)
Supported control operations include:
- provenance operations (`set`, `clear`, `inherit`, `copy`)
- progress/phase/completion operations (`progress`, `phase`, `complete`, `fail`)

## Script Read Surface

### Ifchecks
- `eventsourceuid`, `eventsourceinstance`, `eventsourcebracket`, `haseventsource`
- `eventbracket`
- `eventactive`, `eventkills`, `eventitems`, `eventgoal`, `eventphase`

### Entity expansions
Mobile/object fields include event provenance and runtime snapshots:
- source uid/instance/bracket
- active flag
- kills/items/goal
- phase (`active`/`leader` where applicable)

## Known Deferred Items

These are intentionally left for a later phase:
- Automatic evaluators for `worldcondition` and `triggered` schedule modes
- Scope-driven enforcement logic beyond current metadata/display usage
- Rich custom completion policies for `custom`/`worldstate` definitions beyond script/manual control
- Reward policy integration and payout orchestration from runtime outcomes

## Operator Notes

Use `event info` and `event status` as the primary runtime inspection tools.

Recommended bracket/progress defaults for most competitive events:
- `bracketmode auto_by_level`
- `progressagg total` for shared progress
- `progressagg per_bracket_all_required` for cohort parity
