# Plan: Event Progs (`eprogs`) and System Progs (`sprogs`)

**Status:** In Progress (partial foundations present; code-verified 2026-02-26)

## Code Reality Check (2026-02-26)

Implemented now:
- Event definitions already persist and reload `progs` via area JSON (`json_area_serialize_event` / `json_area_deserialize_event` + `json_area_deserialize_progs(..., PRG_EPROG)`).
- `evtedit` already supports event-prog attachment/edit operations (`addeprog` / `deleprog`) and EPROG trigger validation.
- EPROG script type/editor plumbing exists (`PRG_EPROG`, `epedit`/script index support).

Still missing versus this plan:
- No separate `sprog` registry implementation and no `spedit` command surface.
- No dedicated `on_start`/`on_phase_change`/`on_tick`/`on_complete`/`on_fail`/`on_stop` eprog contract as specified here.
- Current event lifecycle scripting primarily uses explicit phase/reward script fields in runtime, not the trigger-driven eprog model described below.

### Strict phase status

- Data model + persistence: **Partially complete** (eprog persistence exists; sprog model missing)
- `evtedit eprog` CRUD surface: **Partially complete** (add/delete exists; full command model in this plan not fully present)
- Runtime trigger dispatch contract: **Not started (per this design)**
- `sprog` registry + system dispatch points: **Not started**


## Goal

Add script surfaces for:
- **`eprogs`**: event-definition lifecycle scripting where `$(self)` is the event.
- **`sprogs`**: system-level scripting for global orchestration, with optional area/zone placement.

Primary use cases:
- Event-local world mutation (spawn effects, room overlays, scoped encounters).
- Global orchestration hooks (startup/tick/shutdown/admin workflows).
- Builder ownership by area/zone instead of one global script pool.

## Data Model

### `eprogs` on `EVENT_INDEX_DATA`
Each entry:
- `name` (optional)
- `trigger` (`on_start`, `on_phase_change`, `on_tick`, `on_complete`, `on_fail`, `on_stop`)
- `script` (`WNUM` for script index)
- `interval_seconds` (`on_tick` only; `0` means every event runtime tick)
- `scope_mode` (`event_scope`, `global`)
- `filters` (optional structured constraints)
- `enabled`

### `sprogs` registry
Each entry:
- `name`
- `trigger` (`on_boot`, `on_shutdown`, `on_tick`, `on_admin`, etc.)
- `script` (`WNUM`)
- `placement` (`global`, `area`, `zone`)
- `interval_seconds` (for ticked hooks)
- `enabled`

## Persistence

- `eprogs` are serialized inside each event definition in area JSON (`json_area.c`).
- `sprogs` can be persisted either:
  - globally in game settings JSON, or
  - per area/zone in area JSON arrays for builder-local ownership.

## Runtime Contract

### `eprogs`
Dispatch points:
- `on_start`: after event instance creation and scope resolution
- `on_phase_change`: immediately after phase transition
- `on_tick`: each runtime tick per interval
- `on_complete`: before teardown/reward script dispatch
- `on_fail`: before teardown on failure
- `on_stop`: manual/system cancellation path

Context passed to scripts includes:
- event definition WNUM
- instance id
- phase name/index
- progress counters/goals
- success/failure reason where applicable

### `sprogs`
Dispatch from system runtime hooks independent of one event definition.

## OLC/Command Surface

### `evtedit` (`eprogs`)
- `eprog list`
- `eprog add <trigger> <script_wnum> [name]`
- `eprog set <index> trigger|script|interval|scope|enabled <value>`
- `eprog filter <index> <expr>`
- `eprog remove <index>`
- `eprog clear`

### `spedit` (`sprogs`)
- `sprog list`
- `sprog add <trigger> <script_wnum> [placement] [name]`
- `sprog set <index> ...`
- `sprog remove <index>`
- `sprog clear`

## Safety / Failure Behavior

- Missing script index: warn and skip.
- Script runtime error: log and continue event/system loop.
- Scope enforcement for `event_scope`: no out-of-scope mutation.

## Minimal Delivery Sequence

1. Add data structs and JSON serialization/deserialization.
2. Add `evtedit eprog` CRUD.
3. Wire `eprog` dispatch in event runtime lifecycle points.
4. Add `sprog` registry + persistence.
5. Add system dispatch points.
6. Build and smoke test.

## Out of Scope (initial)

- Full declarative world-mutation DSL.
- Automatic rollback of all scripted mutations.
- New trigger classes beyond lifecycle/system hooks listed above.
