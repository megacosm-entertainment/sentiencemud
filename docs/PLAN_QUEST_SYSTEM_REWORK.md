# Quest System Rework Plan

## Goals

Replace the legacy single-active autoquest flow with a modern quest platform that supports:

- Multiple active quests per player at once
- Authored, branchable narrative quests
- Repeatable mission-style content (FF14 levequest style)
- Allowance accumulation over time for mission starts
- Player quest log (active, completed, objective details)
- Script-driven objective and reward logic
- Optional points/currency/reputation/token/script rewards

Legacy autoquest and its command UX will be retired.

## Delivery Priority

Foundation-first scope for this rework was **character-owned runs only**.

- Required for initial ship: mission allowance economy, multi-active run runtime, quest/mission definitions, objective/reward pipeline, persistence migration, and player command/runtime cleanup.
- Current incremental extension (active): scoped runtime semantics for `target_scope=group` and `target_scope=church` are being implemented in controlled slices without blocking foundation stability.

## Product Model

### Ownership Model Decision

Quest definitions are strictly area-owned.

- Every quest/mission template belongs to exactly one area.
- There is no global quest-definition scope in data model or editor UX.
- World-spanning or global-style quests should be authored in the system area.
- Quest objectives may still reference targets in other areas.

### 1) Two quest categories

- Full Quests
  - Hand-authored progression content
  - Usually finite (one-time or controlled repeatability)
  - Supports branching and stage transitions
  - Persist completion history

- Missions
  - Repeatable, template-driven tasks (levequest style)
  - Governed by per-player allowance economy
  - Faster objective loops, simpler stage structures
  - Retains autoquest-style target generation behavior inside templates
  - Can share objective/reward pipeline with Full Quests

### 2) Shared runtime model

All player quest participation is represented as Quest Runs:

- quest_run_id
- target_scope (foundation: character)
- owner reference (foundation: character id)
- initiator character (who accepted/started the run)
- definition reference (quest or mission template)
- current stage/state
- per-objective progress
- branch flags/variables
- started_at, completed_at, expires_at (optional)
- status: active/completed/failed/abandoned

This removes the single pointer model currently represented by ch->quest and countdown/nextquest coupling.

### 3) Target Scope Model

Quest definitions must declare a target scope policy:

- character
  - Run is owned and progressed by one player character.
  - Legacy and default behavior for migrated content.

- group (deferred stretch goal)
  - Run is shared by a party/group.
  - Objective contribution and reward distribution are group-aware.
  - Implementation aligns with the group refactor and is intentionally post-foundation.

- church (deferred stretch goal)
  - Run is shared by church membership context.
  - Eligibility, visibility, and progression derive from church membership and rank/permissions.
  - Implementation is intentionally post-foundation.

Default for existing content: character.

## Core Data Structures

Introduce these top-level concepts:

- QUEST_DEF
  - id/wnum, owning area, category (full|mission), metadata, repeat policy
  - target_scope (character|group|church)
  - scope_permissions / membership constraints (for group/church)
  - stage graph entrypoint
  - acceptance requirements

- QUEST_STAGE_DEF
  - stage id
  - objective set
  - transition rules
  - optional scripted enter/exit hooks

- QUEST_TARGET_SELECTOR_DEF (mission-oriented)
  - selector mode: specific | weighted_pool | random_by_rules | script_generated
  - supports explicit fixed targets (specific mobs/objects/rooms)
  - supports fully random generation constrained by tags/ranges/areas/rules
  - supports mixed mode (fixed + random picks in the same mission template)
  - stores generated selections on the Quest Run for deterministic progress tracking

- QUEST_OBJECTIVE_DEF
  - objective type (kill, collect, talk, travel, custom script, etc)
  - target descriptor (widevnum/tag/filter)
  - quantity/threshold
  - completion rule mode (all/any/custom)

- QUEST_REWARD_DEF
  - reward entries (points, currencies, reputation deltas, token grants, script callback, item grants)
  - conditional reward branches if needed

- QUEST_RUN
  - active runtime instance bound to target owner (character/group/church)
  - scope_owner_id + scope_owner_type
  - initiated_by_character_id
  - supports attached runtime tokens for event orchestration

- QUEST_LEDGER
  - lightweight history for completed major quests
  - indexed for quest log presentation

- QUEST_TOKEN_BINDING_DEF
  - references existing TOKEN_INDEX/TOKEN runtime system (no parallel token subsystem)
  - target attachment: player, npc, room, or object (using existing token attach semantics)
  - lifecycle policy: on_accept, on_stage_enter, on_complete, on_fail, on_abandon
  - supports initializing existing token ints/flags/script variables from quest context
  - optional auto-cleanup policy to prevent stale runtime tokens

- MISSION_ALLOWANCE_STATE
  - current_allowances
  - max_allowances
  - regen_period_seconds
  - last_regen_ts
  - optional category-specific pools

## Allowance System (Missions)

Per-player allowance bank:

- Regenerates by wall-clock interval while offline or online
- Capped at configurable maximum
- Mission acceptance consumes allowances
- Different mission templates may consume variable allowance cost

Suggested baseline behavior:

- allowance_max default 100
- allowance_regen every N minutes (configurable)
- on login + periodic update: reconcile elapsed time and add allowances

This can replace nextquest cooldown behavior for mission cadence.

## Mission Template Generation Modes

Mission templates must support both legacy autoquest patterns:

- Targeted mode
  - Template chooses exact entities/rooms/items by explicit references
  - Equivalent to current hand-picked autoquest outcomes

- Fully random mode
  - Template rolls objectives from constrained pools/rules at acceptance time
  - Equivalent to current random autoquest generation behavior

- Hybrid mode
  - Some objectives fixed, others generated randomly

Generated objective choices are persisted into each Quest Run so scripts and
player logs always reference stable targets after acceptance.

## Scripting Integration

Existing quest-related script commands should be migrated into a v2 set that targets Quest Runs and definitions rather than implicit single-quest state.

Existing script-driven questor content patterns (for example, questor mob scripts
that currently build quest parts via triggers) should be supported through
template/script generation hooks rather than removed.

Core mission/quest flow should remain fully functional without requiring script
handlers; scripting is an optional customization layer.

### Keep conceptually

- quest acceptance/cancel/complete actions
- quest part/objective creation and tracking
- quest scroll generation equivalent
- template-time scripted objective generation hooks

### Default vs Optional Execution

- Default (required)
  - Data-defined objective selection and transitions
  - Data-defined reward application
  - Log/progress updates driven by core runtime

- Optional (script customization)
  - Override/augment objective generation
  - Custom transition rules beyond default graph logic
  - Custom reward side-effects and world-state hooks

### Rework API surface

- quest start <definition> [scope options]
- quest advance <run> <stage or transition>
- quest objective update <run> <objective> <delta|set>
- quest complete <run>
- quest fail <run>
- mission start <definition>
- mission generate-objectives <run|template-context>
- mission allowance get|add|set|consume

These commands are optional integration points; templates should not require
script implementations unless advanced behavior is desired.

Scope options for foundation:

- target_scope=character (default)

Deferred after foundation ship:

- target_scope=group
- target_scope=church

### Reward handling

Support both:

- Declarative rewards in QUEST_REWARD_DEF
- Script callback reward pipelines for complex behavior

Reward actions to support initially:

- Generic points bank (renameable label)
- Currencies (shop currency system integration)
- Reputation changes
- Token grants
- Item grants
- Script event/callback invocation

Token grants must support attaching quest-scoped runtime tokens directly to the
player so token scripts can drive dynamic world behavior.

Implementation note: reuse the existing generic token runtime as-is (tprogs,
token integer fields, flags, and arbitrary script vars), with quest runtime only
managing when/where token instances are attached and cleaned up.

## Token-Orchestrated Quest Events

The quest runtime must support token-driven orchestration patterns, including:

- Attach token to player on acceptance/stage transition
- Attach token to npc/room/object when template requires world-anchored behavior
- Token scripts reacting to player context (room/area/wilds/location checks)
- Spawn orchestration (for example, levequest encounter spawns near objective areas)
- Stage-local token behavior and deterministic cleanup on stage completion/failure

This enables levequest-style behavior where progression state is carried by
runtime quest tokens and world events are triggered contextually.

## Player UX

Replace current quest command behavior with log-centric UX:

- quest log
  - Active quests/missions
  - Stage/objective progress
  - Time-limited indicators
  - Focus marker for the currently selected quest run
  - Scope indicator (character/group/church)

- quest focus <name|idx>
  - Selects one active quest run as the focused run
  - `quest info` and other summary-style quest commands default to focused run output
  - If no focus is set, default to most recently updated active run

- quest history
  - Completed full quests
  - Optional mission completion counters

- quest details <run or id>
  - Full objective list
  - Branch context summary
  - Reward preview (if visible)

- mission board / mission list
  - Available missions filtered by eligibility
  - allowance cost shown
  - current allowances shown
  - scope eligibility shown (character/group/church)

## Editor Requirements

Add editor support for quest definitions and mission templates.

### qedit (new)

- metadata
- owning area (required; inherited from editor context)
- category (full/mission)
- target_scope (character|group|church)
- scope permissions/requirements (especially for group/church)
- repeat policy
- acceptance requirements
- stage graph editing
- objective editing
- transition rules
- reward table
- script hook assignment

### mission templates

Either:

- mission as category inside qedit (preferred), or
- dedicated medit2 if separation is required

Preferred: one definition system with category flag to reduce duplicated code.

### Ownership Enforcement Checklist

- qedit creation requires an in-context area and stamps quest ownership to that area.
- quest save/load validates owning area exists; invalid ownership fails validation and logs error.
- quest list/search defaults to current area scope in builder workflows.
- cross-area objective references are allowed and validated as references, not ownership transfer.
- system-level/world quests are authored in the system area by policy.

Implementation note (current incremental rollout):

- authored template-backed quests set `quest_index_vnum` + `run_id`
- generated mission/leve quests may run with `quest_index_vnum = 0` and a valid `run_id`
- once generated missions are backed by stored templates, they should set `quest_index_vnum`

## Migration and Cleanup Strategy

User constraint accepted: only minimal compatibility concerns are required.

### Required migration concerns

1. Legacy in-memory/persisted quest state cleanup
   - Remove/ignore old ch->quest payload on load
   - Clear old quest parts if present in character data
   - Stop writing legacy quest object payload in new saves once cutover is complete

2. Legacy points naming migration
   - If renaming questpoints to generic points (or splitting mission/full pools), run one-time load-time migration:
     - old questpoints -> new points bank(s)
     - preserve value exactly

3. Questor NPC data
  - Existing questor metadata and script attachments can remain when reused as mission template generators
   - Otherwise deprecate questor-specific autoquest fields in a later cleanup phase

4. Target scope migration
  - Existing quests default to target_scope=character.
  - Group/church scoped runs are additive and introduced only for newly authored templates unless explicitly migrated.

### Hard cut behavior

- Disable autoquest request generation path
- Keep legacy fields readable during transition window
- Convert and persist into new schema immediately after first successful load/save cycle

## Persistence

Store new system in character JSON under dedicated blocks:

- quest_runtime
  - active_runs: []
    - each run stores target_scope + scope owner reference
  - mission_allowances: {}
  - points_banks: {}

- quest_history
  - completed_full_quests: []
  - mission_stats: {}

Keep legacy fields read-only during transition and stop writing them after migration flag is set.

## Execution Plan (Phased)

### Phase 0: Foundations

- Introduce new quest runtime structs and alloc/free lifecycle
- Add points bank abstraction (including legacy questpoints mapping)
- Add mission allowance state and regen logic
- Add load/save JSON schema blocks (read legacy too)

### Phase 1: Runtime Engine

- Implement Quest Definition + Stage + Objective execution engine
- Implement multi-active-run management per player
- Implement target_scope ownership plumbing for **character runs**
- Implement objective progress APIs and completion transitions
- Implement mission target selector engine (specific/random/hybrid/script-generated)
- Implement quest-token binding lifecycle on top of existing token system (attach/update/cleanup)

### Phase 2: Script API v2

- Add/bridge script commands for run-based quest operations
- Add reward execution pipeline (declarative + scripted)
- Add trigger events for stage start/complete/fail/cancel
- Add template-time generation hooks to preserve current autoquest script workflows
- Add explicit APIs for quest-token attach/remove and run-token lookup in scripts

### Phase 3: Player Commands

- Implement quest log/history/details and mission list commands
- Surface character-scope ownership in command UX
- Integrate allowance display/consumption
- Remove legacy single-quest UX paths

### Post-Foundation Stretch

- Complete church/group lifecycle hooks end-to-end (join/leave/disband/deletion)
- Expand command UX and eligibility rules for non-character scopes
- Integrate group sync/catch-up flows for cooperative progression

### Phase 4: Editor

- Add qedit for definitions, stages, objectives, rewards, branches
- Add validation tooling (graph validity, unreachable stages, missing rewards)

### Phase 5: Retirement

- Disable legacy autoquest generation
- Mark legacy quest fields deprecated, then remove
- Keep temporary read migration for one release window, then remove

## Progress Checkpoint (Current)

- Implemented: runtime identifiers (`run_id`, focused run, next run id), widevnum template linkage (`quest_index_auid` + `quest_index_vnum`), and login cleanup for legacy quest payloads.
- Implemented: script-side run linkage controls (`questgenerate` optional template link and `questsetindex`) with widevnum validation.
- Implemented: migration-safe runtime normalization (stale template links auto-cleared with quest logging).
- Implemented: single-run façade helpers (`quest_runtime_get_run_by_id`, `quest_runtime_get_focused_run`) to prepare command flow for multi-run containers.
- Implemented: `quest_runtime.active_runs` persistence path (single-entry-backed, legacy `quest` fallback retained during transition).
- Implemented: list-aware run helpers (`get by id/index`, active run count) and quest command routing updates for `quest log`/`quest focus` against run-list semantics.
- Implemented: focused-run custom objective completion path for `quest commence` (run-pointer based).
- Implemented: focused-run mutation paths for `quest cancel` and `quest complete`, including safe run detach from active-run list while preserving focus consistency.
- Implemented: quest access flow switched from legacy cooldown gating (`nextquest`) to mission allowance gating in `quest request/time/update` paths.
- Implemented: persistence decoupling for legacy cooldown (`nextquest`) so save paths no longer emit it and load paths import it as one-way allowance timing compatibility.
- Implemented: script-facing `nextquest` timer setter/getter now aliases to allowance delay semantics, preventing scripts from reintroducing legacy cooldown state.
- Implemented: script-facing `quest` timer setter/getter now routes through `quest_runtime` countdown expiry fields (with compatibility sync), reducing direct `countdown` coupling.
- Implemented: request/generate/scroll flow now uses explicit active-run pointers (`focused` fallback) instead of direct head-run mutation (`ch->quest`), including script `questscroll` integration.
- Implemented: initial target-scope scaffolding on quest runs (`target_scope`, owner ids/uid) with default-to-character normalization and JSON persistence/load compatibility.
- Implemented: command-level scope gating in `quest` flow with ownership checks for character scope and explicit not-yet-enabled gates for group/church scopes; added `quest request [character|group|church]` parsing.
- Implemented: initial multi-active request behavior by allowing additional `quest request` runs, preserving existing run list on new request insertion, and auto-focusing newly requested runs.
- Implemented: concurrent-run command UX improvements (`quest time` shows focused run + other active run count; `quest cancel`/`quest complete` accept optional run selector by index or run id).
- Implemented: internal target-scope owner normalization/matching helpers (character/group/church ownership seeding + access matching) to prepare seamless activation of group/church run ownership once supporting systems land.
- Implemented: scope-consistent request seeding via shared owner helper and surfaced focused-run scope in `quest focus`/`quest info`/`quest time` output for clearer multi-scope runtime visibility.
- Implemented: removed additional legacy head-run fallbacks in quest generation/custom-task helpers and gated objective-progress checks to player-accessible runs only.
- Implemented: request-flow runtime expiry initialization (`QUEST_EXPIRY_COUNTDOWN`) and centralized expiry reset on final-run detach/timeout to reduce legacy `countdown`/expiry state drift.
- Implemented: `quest time`/`quest_update` countdown handling now prefers runtime expiry fields, with `countdown` treated as compatibility input/fallback rather than primary timer state.
- Decision: group/church quest scopes are explicitly de-scoped from foundation delivery and tracked as post-foundation stretch goals.
- Implemented: player `quest request` flow now enforces character-scope foundation behavior (`quest request [character]`), with non-character scope requests explicitly rejected.
- Implemented: foundational quest-definition scaffolding types (`QUEST_DEF`/stage/objective/reward + objective runtime state) and allocator/free lifecycle helpers in core runtime for upcoming qedit/runtime engine work.
- Implemented: quest index v2 registry API with explicit area-scoped storage (`AREA_DATA->quest_index_v2_hash`) and area+vnum lookup helpers to align quest templates with zone-owned indexing.
- Implemented: quest run v2 structural fields on `QUEST_DATA` (index linkage, stage id, run status/timestamps, objective-state list) plus runtime helpers for index binding, stage activation, and objective-state lifecycle.
- Next on track (Phase 1): continue replacing direct `ch->quest` assumptions in remaining runtime/update flows with true active-runs container semantics while preserving gameplay behavior.
- Implemented: centralized qprog lifecycle dispatch through shared script-bank trigger walker (`p_lifecycle_bank_trigger`) instead of quest-local trigger loops.
- Implemented: qprog lifecycle actor/enactor context is now threaded correctly for command-driven quest events (manual focus/request/grant).
- Implemented: group/church scope gating now validates actual membership (group/church) rather than hard-blocking non-character scopes.
- Implemented: group scope owner identity now uses stable `group_data.id` (replacing leader-UID fallback).
- Implemented: scoped objective propagation for non-character scopes so relevant progress events can fan out across members in the same scope.
- Implemented: quest v2 definition flags (`flags`) with first policy flag `group_snapshot`, allowing per-quest control of group scope-loss behavior.
- Implemented: group context loss handling (`group_remove_member` and `group_disband`) now routes through quest policy (snapshot-to-character or purge for strict group-only quests).
- Implemented: church scope-loss behavior now purges church-scoped runs (member removal/extract/normalization), preserving strict church-only semantics.
- Implemented: admin church deletion path (`church delete`) now saves deleted state then runs full extraction teardown, ensuring online members are detached and church-scoped runs are purged.
- Implemented: church-scoped acceptance permission (`accept_quests`) and church quest metadata persistence (`mission_allowance_bonus`, `board_last_refresh`, `available_missions`).

## Scope Runtime Workplan (Consolidated)

### Completed

- Group scope ownership + matching based on `GROUP_DATA.id`.
- Scope-aware owner resolution for scripts/runtime (character/group/church).
- Group disband/leave scope-loss behavior is policy-driven per quest (`group_snapshot` flag).
- Church scope-loss behavior is strict purge for church-scoped runs.
- Shared trigger dispatch path for qprogs and corrected enactor context wiring.
- Quest v2 flag plumbing (data model, qedit, runtime check path, area JSON persistence).

### In Progress

- Church lifecycle transition hooks (remaining edge paths: bulk church deletion/disband workflows and regression sweep).
- Full validation pass for scoped propagation across kill/collect/travel/custom events.

### Planned Next

- Add automatic cooperative sync for members in the same group scope (default behavior).
- Keep `quest sync` as an explicit/manual override command for edge-case recovery and debug.
- Sync eligibility: same quest template (`auid#vnum`) and scope compatibility.
- Catch-up semantics: promote lower-progress run to higher checkpoint (never downgrade).
- First-pass checkpoint model: stage index + objective completion set within stage.
- Move synced members to a single group-scoped canonical progression reference.
- Auto-sync trigger points (first pass): on group member join/leave transitions, run focus changes, and objective progression events.
- Safety guard: never perform automatic sync when either run is failed/abandoned/completed; only active runs participate.

## Audit + Refocus (2026-02-21)

This section supersedes older foundation-only notes below and reflects current shipped behavior.

### qedit / qpedit audit

- Implemented in `qedit` (authoring):
  - metadata (`name/summary/description`, `mode`, `category`, `scope`, `flags`, `repeat`, `allowance/cost`, `entry`, `enabled`, seed policy)
  - script linkage (`addqprog`, `delqprog`, index vars)
  - stage/objective/reward authoring with target/destination/token refs and objective pools
  - tabbed display (`General/Flow/Rewards/Scripts/Notes`) with visibility rules for objective field relevance
- Implemented in `qpedit` (script authoring):
  - unified script editor (`General/Logs/Uses`) for `PRG_QPROG`
  - compile path uses quest IFC (`IFC_Q`) and persists compile/runtime logs
  - Uses tab now includes quest-bank references from `QUEST_INDEX_V2_DATA->progs`
- Remaining editor gaps vs plan:
  - no expanded quest policy flag set beyond initial `group_snapshot` (additional flags expected as runtime semantics evolve)
  - no dedicated scope-permission authoring (group/church constraints beyond simple scope + flags)
  - no explicit acceptance-requirements editor yet
  - no graph validation tooling pass (unreachable stages, broken transitions, reward coverage)

### Scripting/runtime audit

- Implemented:
  - qprog lifecycle dispatch now uses shared bank walker (`p_lifecycle_bank_trigger`)
  - trigger-type resolution and matching are stable for quest lifecycle events
  - numeric trigger phrases support lifecycle usage (`manual/request/grant` can act as percent chance when numeric)
  - boot compile path for qprogs is corrected to `IFC_Q`
  - actor/enactor context is threaded for command-driven lifecycle events (`focus/request/grant`)
- Remaining runtime/scripting gaps:
  - church edge lifecycle coverage beyond member removal (bulk deletion/disband workflows)
  - cooperative auto-sync/catch-up not implemented yet (planned below)

### Player-facing audit (`do_quest`)

- Implemented and actively used:
  - `quest log`, `quest list`, `quest focus`, `quest info`, `quest time`, `quest commence`, `quest request`, `quest cancel`, `quest complete`, `quest grant`
  - multi-run semantics, focused-run behavior, run selection by index/id/name, and scope display in output
  - mission allowance gating integrated in request/time flow
- Still missing relative to target UX:
  - dedicated `quest history` command
  - dedicated `quest details <run>` command separate from current `quest info`
  - explicit player-facing sync command (`quest sync`) for manual recovery/debug

## Risks and Mitigations

- Risk: state corruption during migration
  - Mitigation: one-way migration flag + conservative load fallback + logging

- Risk: script content churn
  - Mitigation: provide bridge commands and compatibility wrappers for one cycle

- Risk: data model over-complexity
  - Mitigation: start with minimal objective types and expand incrementally

## Immediate Next Slice

1. Church lifecycle completion + verification
  - Finish remaining bulk church deletion/disband edge hooks and regression-check scope cleanup behavior.
2. Cooperative sync baseline
  - Implement automatic same-template catch-up for active group-scoped runs (never downgrade progress).
  - Add `quest sync` as manual override/debug path.
3. Editor safety + policy expansion runway
  - Add `qedit` validation checks for stage graph integrity and objective/reward consistency.
  - Keep quest policy extensions flag-based (additive `quest_v2.flags`) as new semantics are introduced.
4. Player UX completion
  - Add `quest history` and `quest details` to match planned log-centric UX without replacing current stable commands.

## Runtime + Authoring Checkpoint (2026-02-21)

### Implemented this cycle

- Quest v2 runtime objective progression and stage transitions are active (`all`/`any` completion and auto-advance).
- Event hooks for kill/collect/travel/custom now feed v2 objective progress updates.
- Unified quest mode scaffolding is in place (`narrative|template|hybrid`) with stage source (`static|generated`).
- Deterministic seed plumbing is in place with index-level seed policy (`auto|fixed`) and fixed seed override support.
- Commence lifecycle split is in place: generation metadata is tracked separately from stage commence state.
- Stage-level `auto_commence` behavior is implemented for immediate stage start when desired.
- `qedit` has been converted to quest-index authoring and supports index metadata plus stage/objective authoring.

### Current boundaries

- Character scope remains the baseline and most battle-tested path.
- Group/church scope runtime is partially implemented (ownership, propagation, normalization, lifecycle hooks), with group scope-loss now policy-driven via quest flags and church scope-loss strict by default.
- Generated-stage resolver and commence side effects are partially scaffolded and still require a focused completion slice.

### Next Slice (active)

1. Complete church lifecycle edge hooks and regression-check scoped normalization paths.
2. Implement first-pass automatic group catch-up sync + `quest sync` manual fallback.
3. Add `qedit` validation utilities (graph + objective/reward sanity).
4. Finish player UX parity with `quest history` and `quest details`.
