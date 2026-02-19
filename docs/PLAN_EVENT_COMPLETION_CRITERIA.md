# PLAN_event_completion_criteria

## Goal
Define robust, event-runtime-owned completion criteria for three event families:
- Invasion
- Collection
- War

This plan assumes we keep legacy command shims, while moving progression and completion ownership into the `evtedit` runtime.

## Current Runtime Snapshot
As of now:
- Event instances track participant membership and timer-driven lifecycle.
- Progress counters exist for:
  - kill totals (`progress_kills`)
  - collection turn-ins (`progress_items`)
  - per-player contributions (`kills`, `items_turned`)
- Invasion has a basic phase path:
  - kill threshold reached -> leader phase enabled
  - leader kill can end event
- Collection turn-ins can be fed by `deposit`.

Still needed:
- Rich per-event completion configuration.
- Winner/tie-break policy.
- Reward policy binding to completion outcomes.
- Script-visible progress/phase controls.

## Current Pause Snapshot (2026-02-19)

This plan remains active, but implementation is intentionally paused at the following boundary:

### Implemented
- Scope anchor/floating behavior wired in event runtime.
- Structured event roster in `evtedit` for NPC/object entries.
- Roster entries support:
  - count/chance
  - min/max level windows
  - boss designation (NPC only)
  - phase targeting (`any` or named phase)
- Runtime roster spawning on phase entry (including default `active` start path).
- Boss completion checks can require roster-designated phase boss entries when configured.

### Deferred from this point
- `eprogs` lifecycle hooks (`on_start`, `on_phase_change`, `on_tick`, etc.).
- Advanced placement/filtering modes (for example every-room or sector-filtered spawn behavior).
- Full non-area scope semantics (`region`, `zones`, `battlefield`).
- Expanded reward/winner policy orchestration and tie-break strategies.

### Recommended resume order
1. Add roster placement/filter controls.
2. Implement initial `eprogs` execution model.
3. Complete completion policy matrix (winner/tie-break/reward timing).

## Design Principles
1. Event runtime is source-of-truth for progress/completion.
2. Completion logic must be deterministic and visible in `event info`/`event status`.
3. Criteria should be mostly data-driven, with script overrides for special cases.
4. Defaults should be builder-friendly (no scripting required for common setups).

## Event Family Options

### Invasion
Primary model:
1. **Phase 1 (mob clear):** count qualifying invasion mob kills.
2. **Phase 2 (leader):** spawn/enable leader when kill target is reached.
3. **Completion:** event ends on leader death (or timeout/cancel fallback).

Config knobs:
- `kill_goal` (absolute or level-scaled)
- `leader_required` (bool)
- `leader_timeout_minutes` (optional)
- `allow_phase_regress` (default false)

Failure modes:
- Timeout before leader death.
- Optional auto-cancel when no participants remain.

### Collection
Primary model:
1. Track accepted turn-ins globally and per participant.
2. End when goal is reached or timer expires.
3. Rank participants by contribution for reward policy.

Config knobs:
- `turnin_goal`
- `scoring_mode` (`items`, `weighted_items`, `qp_weighted`)
- `min_contribution`
- `winner_policy` (`top1`, `top3`, `all_meet_threshold`)

Failure modes:
- Timeout with partial progress (policy-driven: partial rewards vs no rewards).

### War
Possible completion models:
1. **Last team standing** (genocide/jihad style).
2. **Kill score race** (first to N kills).
3. **Timed score** (highest score at timeout).

Config knobs:
- `war_mode` (`elimination`, `kill_target`, `timed_score`)
- `kill_goal` (for race mode)
- `respawn_enabled`
- `friendly_fire` policy
- `team_assignment` policy

Failure modes:
- Team imbalance.
- Draw conditions (tie-break by deaths, damage, or sudden death timer).

## Multi-Level Spawn Brackets (Invasion + Collection)

### Why
Support low/mid/high level cohorts in one event definition, especially for seasonal or holiday events.

### Invasion Brackets
Add optional `spawn_brackets[]` with per-bracket:
- `min_level`
- `max_level`
- `mob_pool[]`
- `leader_pool[]` (optional)
- `kill_goal` override (optional)

Selection modes:
- `auto_by_level` (participant level chooses bracket)
- `staff_forced` (staff sets bracket at start)
- `all_active` (multiple brackets run concurrently)

Progress aggregation:
- `shared` (single global completion objective)
- `per_bracket_all_required`
- `per_bracket_any`

### Collection Brackets
Add optional `collection_brackets[]` with per-bracket:
- `min_level`
- `max_level`
- `accepted_items[]`
- `item_weights[]` or weighted table
- `turnin_goal` override (optional)

Routing modes:
- `match_participant_level`
- `match_item_bracket`
- `global_pool`

## Player-Facing Metadata + News
Add optional metadata so events can carry player-visible copy without code edits.

Suggested fields:
- `display_title`
- `short_summary`
- `long_description`
- `news_slug`
- `news_announcement`
- `news_body`
- `theme_tags[]` (e.g. `holiday`, `halloween`, `anniversary`)
- `starts_at_text`
- `ends_at_text`

Behavior notes:
- All fields optional; runtime works with empty defaults.
- If `news_slug` is set, tooling may upsert news on start/end.
- Add per-event toggle for suppressing auto-news publication.

## Script Command Integration (Recommended)
Script commands should allow special events to override runtime defaults.

Candidate commands:
- `event progress <event> addkills <n> [player]`
- `event progress <event> additems <n> [player]`
- `event progress <event> setgoal <n>`
- `event phase <event> set <name>`
- `event complete <event> [reason]`
- `event fail <event> [reason]`

Read-side helpers:
- `%event_progress_kills%`
- `%event_progress_items%`
- `%event_goal%`
- `%event_phase%`
- `%event_participant_kills%`
- `%event_participant_items%`

## Proposed Data Additions (Definition Level)
Optional JSON/runtime fields:
- `completion_mode`
- `kill_goal`
- `turnin_goal`
- `leader_required`
- `leader_mob` / `leader_template`
- `winner_policy`
- `min_contribution`
- `phase_on_goal`
- `spawn_brackets[]`
- `collection_brackets[]`
- `display_title`
- `short_summary`
- `long_description`
- `news_slug`
- `news_announcement`
- `news_body`
- `theme_tags[]`

All new fields must have safe defaults for backward compatibility.

## Phased Delivery Plan

### Phase 1 (in progress)
- Timer lifecycle + runtime progress visibility.
- Invasion leader-phase transition and completion path.
- Collection turn-in tracking (global + per participant).

### Phase 2
- Add completion config fields to JSON + OLC.
- Generic completion evaluator in runtime tick/hook paths.

### Phase 3
- Add bracket-aware spawn/target selection for invasion and collection.
- Add player-facing copy fields and optional news hooks.

### Phase 4
- Add script command surface + variable expansion.
- Add reward resolution policies by contribution/rank.

### Phase 5
- Retire remaining legacy war/gq/invasion runtime dependencies after parity.

## Testing Strategy
- Unit tests:
  - threshold reached
  - timeout without threshold
  - leader phase transition + completion
  - bracket selection and aggregation modes
- Integration tests:
  - collection deposit increments bracketed progress
  - war kill updates score and triggers configured completion
  - invasion leader death ends event after phase transition
  - metadata fields display correctly in player-facing commands
- Regression checks:
  - legacy command shims continue routing correctly
  - status/info output remains accurate and readable

## Open Questions
1. Should leader spawn be default runtime behavior or script-only hook?
2. Should non-participants be allowed to contribute to collection goals?
3. For war, should auto team balancing be enabled by default?
4. Should completion rewards be immediate or deferred to tick boundary?
5. Should news publishing be synchronous, queued, or manual-approval?
