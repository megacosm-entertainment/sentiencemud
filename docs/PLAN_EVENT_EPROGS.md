# PLAN_event_eprogs

## Goal
Add definition-level `eprogs` (event program hooks) so events can run scoped world mutations without hardcoding behavior in event runtime C paths.

Primary target use case:
- Area/zone scoped procedural effects (example: spawn a mist object in every room matching sector filters), enabling Reckoning-style events that stay local to event scope.

## Scope (Minimal Viable)
- Data model only + execution points, not a full script language.
- `evtedit` supports adding/removing/listing hook entries per event definition.
- Runtime executes configured hook scripts at defined lifecycle moments.

## Proposed Definition Data
`eprogs[]` on each event definition, each entry:
- `name` (optional label)
- `trigger` (`on_start`, `on_phase_change`, `on_tick`, `on_complete`, `on_fail`, `on_stop`)
- `script_vnum` (area prog)
- `interval_seconds` (for `on_tick` only; `0` = every event tick)
- `scope_mode` (`event_scope`, `global`)
- `filters` (optional structured payload for sectors/flags/room ranges)
- `enabled` (bool)

## Runtime Contract
- Runtime passes event context to scripts:
  - definition uid, instance id, current phase, scope info, success/fail reason where relevant.
- `event_scope` mode constrains room iteration/mutation to resolved runtime scope anchor.
- Fail-safe behavior:
  - missing script = skip with warning log
  - script error = do not crash event runtime loop

## OLC Command Surface (Proposed)
- `eprog list`
- `eprog add <trigger> <scriptvnum> [name]`
- `eprog set <index> trigger|script|interval|scope|enabled <value>`
- `eprog filter <index> <filter-expr>`
- `eprog remove <index>`
- `eprog clear`

## Phase Order Integration
- `on_start` after instance creation/scope resolution.
- `on_phase_change` immediately after phase transition application.
- `on_tick` in runtime tick for active instances.
- `on_complete`/`on_fail` before teardown reward script dispatch.
- `on_stop` on manual/system cancellation path.

## Constraints
- Backward compatible JSON load/save when `eprogs` missing.
- No requirement for scripts to exist to keep event definition valid.
- Keep scheduler semantics unchanged.

## Delivery Steps
1. Add `eprogs` definition structs + JSON persistence.
2. Add `evtedit` commands for CRUD.
3. Wire runtime trigger execution points.
4. Add scope-constrained room iteration helpers for common map effects.
5. Document operator usage and script examples.

## Out of Scope (This Phase)
- Full declarative DSL for room/object mutation.
- New trigger types beyond lifecycle set above.
- Automatic rollback/undo of all world mutations.
