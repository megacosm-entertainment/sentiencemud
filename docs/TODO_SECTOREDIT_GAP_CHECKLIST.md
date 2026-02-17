# SectorEdit Gap Checklist (PLAN_OLC_REFACTOR §8.3)

Date: 2026-02-17

This tracks integration gaps against `PLAN_OLC_REFACTOR` section 8.3 and records the branch constraint to avoid `gsct_*` global sector pointers.

## Constraints

- Do NOT introduce global sector pointer singletons (`gsct_*`) in active `src` path.
- Keep compatibility with legacy `SECT_*` ids during migration.
- Prefer runtime registry lookup APIs over direct global pointer state.

## Wrap-up Status (Editor Backport Phase)

- 8.3 sectoredit/editor-runtime backport scope is complete in the active transitional architecture.
- This session completed runtime gameplay integration for sector-configured hide messages and sector affinity damage modifiers.
- Focused sector runtime integration tests were added and validated for hide-message and affinity API behavior.
- Structural room-model migration is now complete in active runtime (`int sector_type` replaced by reference-backed room fields).

## 8.3 Prereq Checklist

1. Backport full `SECTOR_DATA` struct to `merc.h`
- Status: Completed (reference-backed room integration)
- Notes: Active room model now stores sector references (`SECTOR_RUNTIME_DATA *`) with compatibility getters/setters for legacy id-based call sites.

2. Create `sectors.c` with load/save and sector runtime list
- Status: Completed (transitional runtime module)
- Notes: Runtime load/save currently exists via `sectoredit` JSON path (`sectors.json`) with id-keyed cache and APIs.

3. Refactor every room: `int sector_type` -> `SECTOR_DATA *sector`
- Status: Completed
- Notes: `ROOM_INDEX_DATA` sector storage migrated to reference-backed fields (`sector` / `rs_sector`), with shim APIs preserving existing call signatures.

4. Update all `SECT_*` comparisons to pointer/lookup semantics
- Status: Completed for active runtime/editor/script paths (shim-based)
- Notes: Active gameplay/runtime comparisons are being migrated to `room_in_sector`/`room_sector_type`; remaining raw uses are compatibility/persistence/reflection paths pending structural room refactor.

5. Bootstrap default sectors from current `SECT_*` defines
- Status: Completed (transitional)
- Notes: Runtime sector data seeds from existing sector constants/default names and movement defaults.

6. Build framework editor `editors/sectors/sectoredit.c`
- Status: Completed (transitional MVP)
- Notes: Framework editor exists with list/show/name/movecost/healrate/manarate/comments/save/reload.

## Immediate Next Steps (ordered)

1. Extract sector runtime from editor into dedicated runtime module (`sectors_runtime.c`) so editor is thin. ✅ Completed
2. Add read/write API for room-facing code paths (`sector_move_cost`, `sector_name`, defaults by id). ✅ Completed (initial API surface)
3. Introduce compatibility accessor macros/functions for room sector field usage before structural refactor. ✅ Completed
	- Added `sector_type_sanitize`, `room_sector_type`, `room_in_sector` runtime APIs.
4. Convert high-traffic consumers (movement/update/weather) to accessors. ✅ Completed for active runtime/editor/script paths
	- Migrated movement cost lookups in `act_move.c`.
	- Migrated movement sector comparisons in key `act_move.c` paths (`move_char`, `move_success`, hide/land checks).
	- Migrated storm/reckoning/flight-water checks in `weather.c` and `update.c`.
	- Migrated toxic/cursed/bramble checks in `update.c`.
	- Migrated room darkness sector checks in `handler.c`.
	- Migrated cursed sanctum spell penalties in `magic.c`.
	- Migrated terrain-gating checks in `magic_earth.c`.
	- Migrated lava/terrain checks in `magic_cold.c`.
	- Migrated sector-related script interface checks in `script_ifc.c`.
	- Migrated enchanted-forest drop behavior checks in `act_obj.c`.
	- Migrated spell recovery roomblock checks in `script_tpcmds.c`.
	- Migrated remaining direct room water-sector gate in `boat.c`.
	- Migrated wilderness terrain-template sector checks in `boat.c` to accessor predicates (including fixing impossible dual-water equality checks).
	- Migrated MSDP terrain reporting read in `update.c`.
	- Migrated active script `alterroom sector/rssector` to accessor-backed working values and setter/sanitizer finalization in `script_commands.c`.
	- Migrated legacy room persistence `Sector` key load path in `db.c` to `room_set_sector_type`.
	- Migrated treasure map wilderness terrain filtering in `treasuremap.c` to accessor predicates.
	- Migrated wilderness branch of script `ifc_sector` (`script_ifc.c`) to accessor predicates.
	- Migrated wilderness editor terrain sector display/setter path in `editors/wilderness/wedit.c` to accessor/setter APIs.
	- Normalized `rs_sector_type` sanitize flow in `io/json/json_area.c`, `editors/rooms/redit.c`, and `olc_save.c`.
	- Added rs-sector shim APIs and migrated active rs-sector consumers in `script_commands.c`, `db.c`, `wilds.c`, and `olc_act.c`.
	- Updated OLC prompt metadata for `ED_SECTOR` in `olc.c` so active sector target displays while editing.
	- Updated `sectoredit` command entry flow in `editors/sectors/sectoredit.c` to support top-level `list|save|reload|<sector>` parsing.
	- Replaced remaining runtime sector display/lookup usage in `redit.c`, `wedit.c`, `update.c`, and `olc_act.c` so runtime sector names are shown in editor/status/help outputs.
	- Fixed script sector lookup correctness in `script_ifc.c` to avoid treating invalid sector strings as `SECT_INSIDE`.
	- Switched `script_commands.c` `alterroom sector` parsing to runtime `sector_lookup` for sector field values.
	- Hardened `script_commands.c` `alterroom sector/rssector` handling to reject invalid runtime sector names and enforce assignment-only typed updates.
	- Removed `alterroom sector/rssector` dependency on generic flag-table parsing in `script_commands.c`; sector fields now parse via explicit typed lookup/sanitize path.
5. Plan and execute staged `ROOM_INDEX_DATA` migration from int id -> runtime sector reference. ✅ Completed

## Migration Shim Status

- Added room-sector compatibility shims:
	- `room_sector_type(const ROOM_INDEX_DATA *room)`
	- `room_set_sector_type(ROOM_INDEX_DATA *room, int sector_type)`
	- `room_in_sector(const ROOM_INDEX_DATA *room, int sector_type)`
	- `room_rs_sector_type(const ROOM_INDEX_DATA *room)`
	- `room_set_rs_sector_type(ROOM_INDEX_DATA *room, int sector_type)`
- Core write paths now route through setter APIs (`room_set_sector_type` / `room_set_rs_sector_type`) across db/olc/olc_save/rset/json_persist/wilds/script paths.
- Storage is reference-backed (`SECTOR_RUNTIME_DATA *sector` / `SECTOR_RUNTIME_DATA *rs_sector`) with id-compatible shim APIs.

## Remaining Raw `sector_type` Uses (active `src`)

Post-cleanup validation (2026-02-17):
- Global member-access scan for `->sector_type`/`->rs_sector_type` in active `src` runtime code reports 0 matches.
- Legacy raw uses remain only in archival/reference trees (`old_stuff/`, `src_20_dev/`) outside active build paths.

## Blast Radius Map (active `src`, 2026-02-17)

High-risk (direct raw sector storage touch):
- None in active runtime code; sector storage is now reference-backed in `ROOM_INDEX_DATA`.

Editor/UI blast radius (runtime sector integration complete):
- `editors/sectors/sectoredit.c`
- `editors/rooms/redit.c`
- `editors/wilderness/wedit.c`
- `olc.c`
- `olc_act.c`

Script blast radius (typed/accessor integration complete for active alterroom/ifchecks):
- `script_commands.c`
- `script_ifc.c`

Persistence/runtime consumer spread (expected and accessor-backed):
- `db.c`, `olc_save.c`, `io/json/json_area.c`, `io/json/json_persist.c`, `wilds.c`, `update.c`, `weather.c`, `magic*.c`, `act_move.c`, `boat.c`, `handler.c`, `hunt.c`, `treasuremap.c`, `scripts.c`, `fight.c`, `mem.c`.

Phase 2 target (structural refactor):
- Completed in this branch for active runtime/editor integration.

## Structural Migration Implementation Summary

Stage A — data model introduction:
- Completed. Introduced reference-based room sector identity via `SECTOR_RUNTIME_DATA *sector` and `SECTOR_RUNTIME_DATA *rs_sector` in `ROOM_INDEX_DATA`.
- Compatibility helpers continue to expose id-based getters/setters for transition safety.

Stage B — persistence dual-read/write:
- Completed for active paths via compatibility APIs: persistence/editor/runtime callsites still read/write sector ids, while room storage is reference-backed.

Stage C — runtime cutover:
- Completed. Runtime room storage no longer uses raw int sector fields.
- Shim APIs remain stable to avoid callsite churn.

Stage D — compatibility cleanup:
- Completed for `ROOM_INDEX_DATA` storage fields.
- Transitional sanitize/setter APIs remain intentionally, but now operate as id↔reference adapters.

Stage E — validation + hardening:
- Build validation completed for active branch.
- Full suite remediation remains outside this checklist scope.

## Editor-Unblock Gate (current status)

- ✅ Sector editor framework command flow and runtime persistence are in place.
- ✅ Runtime sector naming/lookup shown across editor/help/status/script interfaces.
- ✅ Active script alterroom/ifcheck sector paths use typed accessor/lookup behavior.
- ✅ Raw direct sector field access in active `src` runtime code is removed.
- ✅ Structural room-model migration is complete for active editor/runtime integration.

## Exit Criteria for “8.3 complete”

- ✅ Room sector field is no longer raw int id in active runtime.
- ✅ Core gameplay no longer directly depends on `SECT_*` comparison scatter.
- ✅ Sector registry persists/loads independently of editor module.
- ✅ No global singleton sector pointers required for runtime behavior.
