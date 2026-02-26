# WORKLOG_event_editor_runtime

## Summary

This worklog records the event editor/runtime closure pass focused on script support, bracket-aware behavior, runtime control surfaces, and documentation.

## Completed Changes

### Runtime/event core (`src/editors/events/evtedit.c`)
- Added/extended runtime APIs for script integration:
  - progress adjust/set goal/set phase/finish
  - source-based progress/phase queries
- Added bracket-aware spawn provenance and participant matching for invasion credit paths.
- Added boss event completion behavior using NPC kill progression.
- Added calendar schedule cadence behavior after scheduled anchor start.
- Added participation guardrails for passive/worldstate event types.
- Improved command help output and status/info reporting (including boss progress).

### Event API (`src/event_types.h`)
- Added declarations for runtime control and source-based runtime query helpers.

### Script integration
- `src/script_commands.c`
  - Expanded `event` command control surface (`progress`, `phase`, `complete`, `fail`) while preserving provenance controls.
- `src/scripts.c`
  - Propagated event source + bracket metadata through script-driven spawns.
- `src/script_ifc.c`, `src/script_const.c`, `src/scripts.h`
  - Added ifchecks and declarations for event runtime/provenance state.
- `src/script_expand.c`, `src/script_const.c`, `src/scripts.h`
  - Added mobile/object expansion fields for event runtime snapshots and provenance.

### Editor input validation
- `spawnbrackets` and `collectionbrackets` now validate ordered/non-overlapping ranges.
- `bracketmode` now validates allowed values:
  - `auto_by_level`, `open`, `manual`
- `progressagg` now validates allowed values:
  - `total`, `per_bracket`, `per_bracket_all_required`

### Roster + phase runtime slice
- Added structured event roster support in `evtedit`:
  - NPC/object entries with `vnum`, `count`, `chance`, `min_level`, `max_level`
  - NPC boss designation
  - phase targeting (`any` or named phase)
- Added roster JSON persistence (`events.json` save/load).
- Added runtime roster spawn execution at phase entry (including default start/active phase).
- Added boss-aware kill completion filtering using roster-designated boss entries when present.

## Build/Validation

- Debug builds completed successfully after integration patches.
- Unit test execution was intermittently skipped by session flow in some runs; compile-level validation is green.

## Remaining Deferred Items

- Autonomous evaluators for `worldcondition` and `triggered` schedule modes.
- Deeper scope enforcement semantics in runtime eligibility/dispatch.
- Full reward policy orchestration tied to completion outcomes.
- Rich custom/worldstate completion policy presets beyond script/manual controls.
- `eprogs` lifecycle hook system and scoped map mutation filters.
- Advanced roster placement/filter modes (every-room/sector-filtered behavior).

## Pause/Handoff Snapshot

Work is paused with runtime-stable roster + phase integration in place.

Recommended resume order:
1. Add placement/filter controls for roster entries.
2. Implement first `eprogs` runtime hooks.
3. Align scope semantics for non-area scopes (`region`, `zones`, `battlefield`).

## Documentation Added

- `src/docs/guides/EVENT_EDITOR_RUNTIME.md` (implementation reference)
- `src/docs/WORKLOG_EVENT_EDITOR_RUNTIME.md` (this log)
