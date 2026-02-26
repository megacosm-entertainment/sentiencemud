# Docs Portfolio Review (2026-02-18)

## Scope

Reviewed the full `src/docs/` tree (116 markdown docs):
- 60 top-level docs
- 46 docs in `docs/done/`
- 5 docs in `docs/testing/`
- 5 docs in `docs/widevnums/`

This review consolidates status across roadmap, plans, worklogs, and completed records.

---

## What Is Clearly Completed

### Core migrations and foundations
- Widevnum migration is documented as complete in `docs/widevnums/WIDEVNUM_REMAINING_WORK.md` (Phases 1-8 complete; polish deferred).
- Post-migration cleanup is complete: script parsing/context hardening and JSON shop stock cross-area serialization/fixup (`0#<vnum>` regression) are resolved.
- JSON persistence + Redis integration is complete and operational (tracked in `docs/done/` and roadmap completed section).
- OLC framework refactor Phases 0-7 are complete; major editor migration is complete, with only remaining editor backports/new editor items still open under Phase 8.
- Object multityping Phases 1-4 are complete (data model, migration, serialization, and stop-writing legacy value arrays).
- Account/auth consolidation and crypto phase 1 are complete.

### Testing baseline
- JSON-driven integration test framework is established and documented.
- Build + test workflows exist and are in regular use.

---

## What Is Active / In Progress

### 1) Skills/classes backport
- `PLAN_backport_skills_classes.md` is active.
- Phases 0-6 complete; remaining work is editor/legacy cleanup phases.
- This is a high-impact dependency for class/job and party follow-on work.

### 2) Object multityping gameplay migration
- `PLAN_backport_object_multityping.md` Phase 5 is in progress.
- Remaining work is conversion of gameplay/script/editor logic from raw `value[]` usage to type accessors.

### 3) Event runtime completion criteria
- `PLAN_EVENT_COMPLETION_CRITERIA.md` Phase 1 is in progress.
- Runtime ownership and completion policy are not fully finished yet.

### 4) Bootstrap + infra polish
- Bootstrap is partially implemented and still has required outputs pending.

---

## Remaining Work by Priority

## P0 (Do next)

1. **Room PK semantics cleanup (`cpk` removal / `chaotic` introduction)** — **DONE (2026-02-18)**
   - Add `chaotic` room flag: causes inventory loss behavior currently tied to `cpk`.
   - Decouple item-loss behavior from PK-permission behavior.
   - New behavior rule:
     - `player_killing` alone: enables PK, no forced inventory-loss semantics.
     - `chaotic` alone: enables inventory-loss-on-death semantics, no PK by itself.
     - `player_killing` + `chaotic`: equivalent to current `cpk` gameplay behavior.
   - Migration target: remove direct dependence on legacy `cpk` semantics from runtime checks and builder-facing editing.
   - Completion noted in current-cycle docs: runtime PK/full-CPK split semantics and room-flag model are now represented as `player_killing` + `chaotic`.

2. **Roadmap/status reconciliation pass** — **DONE (2026-02-18)**
   - Top-level roadmap now reflects widevnum as completed work.
   - Widevnum docs and roadmap are aligned.

3. **Complete active phase work (no new major feature starts)** — **IN PROGRESS**
- Finish skills/classes Phase 7+ follow-up (editor/cleanup scope).
- Continue object multityping Phase 5 conversion in highest-traffic gameplay paths first.
- **Completed slice (2026-02-18):** skill usability helpers added and integrated (`skill_entry_is_usable_now` / `skill_is_usable_now`) for centralized runtime availability checks.
- **Completed slice (2026-02-18):** runtime `skill_from_sn(...)` compatibility shim retired; runtime callsites are on `skill_find_uid(...)` (docs may still mention historical usage).

## P1 (Immediately after P0)

4. **Event completion criteria Phases 2-3**
   - Move from runtime baseline to full data-driven completion settings and bracket support.

5. **Testing roadmap Phase 2 kickoff**
   - Unit test schema + pure function handler support.
   - Memory-testing integration (ASan/Valgrind) under framework flow.

6. **Bootstrap completion**
   - Finish required generated artifacts and deployment parity from plan.

## P2 (Near-term, sequence-sensitive)

7. **Group system refactor (`GROUP_DATA`)**
   - Enables cleaner group identity and unblocks party/pubsub downstream.

8. **Script/prog grouping and OLC ergonomics**
   - Good leverage but should follow core active migrations above.

9. **Remort/multiclass system removal**
   - Cleanup pass after class/job progression parity is stable.

## P3 (Post-stabilization)

10. **Technical debt criticals after migration stabilization**
    - String safety pass (`sprintf`→`snprintf`, safe string wrappers).
    - Reset system modernization and large architecture initiatives as documented.

---

## Cross-Doc Risks and Gaps

1. **Status drift risk**
   - Several docs are accurate in isolation but not synchronized with top-level roadmap status.

2. **Mixed-state plan granularity**
   - Some docs under `docs/done/` still contain unchecked planning checklists; these are historical but can be mistaken as current backlog.

3. **Priority fragmentation**
   - Priority labels exist in many files, but no single source currently enforces execution ordering across systems.

4. **Implementation guide gaps (high-friction onboarding)**
   - We still need practical "how-to" docs for common extension work:
     - adding new player/admin commands (`interp.c` command table + handler placement conventions),
     - adding/updating OLC editors (`src/editors/` patterns, registration, permissions, display tabs),
     - implementing JSON persistence correctly (path helpers, migration/backward compatibility, cache interactions, atomic writes),
     - wiring tests for new systems (JSON suite definitions + C test dispatch integration).
   - Add these as first-class docs to reduce tribal knowledge and avoid inconsistent implementations.

---

## Recommended Execution Order (Practical)

1. Reconcile roadmap statuses to match completion docs.
2. Finish skills/classes active phase.
3. Continue object multityping Phase 5 conversion.
4. Advance event criteria Phase 2.
5. Start testing roadmap Phase 2 (unit + memory tooling).

---

## Immediate Next Steps (Skills/Classes Track)

The current branch has active momentum on skills/classes migration and runtime safety.
Use this short sequence for the next sessions before starting new large features.

1. **Consolidate skill usability gates**
   - Keep `skill_entry_is_usable_now(...)` / `skill_is_usable_now(...)` as the single entrypoint for runtime availability checks.
   - Migrate high-traffic command handlers that still rely on ad-hoc `get_skill(...) == 0` checks to these helpers.
   - Preserve behavior where traits intentionally bypass skill checks.

2. **Finish Phase 9 read-path migration slices (pointer-first affects)**
   - Continue converting `paf->type` comparison/read sites to prefer `paf->skill` pointer with `type` fallback.
   - Prioritize combat/update/script paths first, then lower-frequency admin/editor paths.

3. **Retire remaining `gsn_*` dependencies in thin slices**
   - Replace declaration/definition usage incrementally (`merc.h`, `db.c`, callsites grouped by subsystem).
   - Do not do a big-bang removal; gate each slice behind clean build + smoke checks.

4. **Stabilize build verification workflow**
   - Standardize on one authoritative clean build check per slice (`./build clean`) and record the final terminal result only.
   - Avoid mixing stale terminal/history output into status decisions.

5. **Keep docs synchronized per slice**
   - Update `WORKLOG_skills_classes.md` after each migration slice with objective, changed files, and validation command.
   - Check off or re-scope corresponding items in `PLAN_backport_skills_classes.md` to prevent status drift.

### Exit Criteria for this mini-track

- Availability checks are centralized and used by major player skill/spell command paths.
- No runtime `skill_from_sn(...)` references remain (docs-only references acceptable).
- Pointer-first affect read paths cover core gameplay loops.
- Clean debug build passes after each slice.

---

## Definition of Done for the `cpk` Migration Item

- Runtime no longer depends on legacy `cpk` as a single combined semantic.
- Room flags support independent toggling of `player_killing` and `chaotic`.
- Death/inventory-loss logic keys on `chaotic`.
- PK legality checks key on `player_killing` (and other existing PK rules), not `chaotic` alone.
- Rooms needing old `cpk` behavior are configured with both flags.
- Builder/editor/help text updated so staff can configure this intentionally.
