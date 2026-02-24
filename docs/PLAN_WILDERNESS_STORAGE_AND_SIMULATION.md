# Plan: Wilderness Storage and Simulation Contract (2026)

**Date:** February 24, 2026  
**Status:** Corrective Re-sequencing Active

---

## Purpose

Define a concrete, staged implementation plan for wilderness storage and runtime simulation using:

- `wmap` for base terrain payloads
- `wterr` for terrain definitions
- `_state` for dynamic runtime actors/effects
- `_mods` for explicit terrain overrides

This plan is the foundation for weather, NPC ships, NPC spawns, and future elevation-aware dynamics.

---

## Current Baseline (Confirmed)

- Wilderness uses virtual rooms with `staticmap` + `map`.
- Runtime overlays already exist (`runtime_chunks` / temporary zones).
- In-engine async wildgen worker exists (`wilds_wildgen.c`) with threaded PNG import.
- Grid import + elevation grid validation exists.
- Elevation validation currently reports but does not yet apply elevation layer effects.
- Area loading is JSON-capable, and **regions are area-native**.

---

## Non-Goals (This Phase)

- Web editor UX and collaborative editing.
- Full fleet/naval-combat completion.
- Replacing virtual rooms with fully static wilderness rooms.
- Boot-time full-map room materialization.

---

## Data Contract

Implementation reality (2026-02-24):

- Dedicated sidecar persistence is implemented for base/definitions/runtime layers:
   - `wilderness.wmap`
   - `wilderness.wterr.json`
   - `wilderness_vlinks.json`
   - `wilderness_mods.json`
   - `wilderness_state.json`
- Sidecars are stored per wilderness under `data/world/wilderness_state/<uid>_<name>/` with an `images/` child directory.
- Area JSON now stores wilderness metadata + storage association, while legacy embedded `staticmap`/`terrains`/`vlinks` remain read-compatible for migration.
- Legacy flat sidecar files and legacy embedded payloads are migrated/exported automatically during storage load/save passes.

## A) Base Map (`*.wmap`)

Binary, compact, immutable-at-runtime wilderness base payload.

Required fields:

- format/version/magic
- dimensions (`width`, `height`)
- terrain tile stream
- elevation stream (or optional section, if enabled)
- checksum/hash for merge compatibility

Rules:

- Treated as source data from wildgen/import pipeline.
- Never edited directly by runtime simulation.

## B) Terrain Definitions (`*.wterr`)

JSON terrain dictionary keyed by terrain token/id.

Required fields per terrain:

- tile token/id
- display metadata
- sector/movement semantics
- room flags and behavioral tags
- wildgen color mapping metadata

Rules:

- Shared definitions can be reused across wilderness maps.
- Runtime logic resolves effective terrain through `wterr` metadata.

## C) Runtime State (`*_state.json`)

Mutable coordinate-native state for simulation actors and temporary/permanent world features.

Contains:

- weather cells/storm actors
- npc ship actors (position, velocity, behavior profile, route state)
- npc spawn controllers (camp/spawn node runtime state)
- dynamic feature instances (flood, fire, blockade, event markers)
- timing data (`created`, `expires`, scheduler metadata)
- `base_map_checksum`

Rules:

- Temporary entries can expire or be dropped during regeneration merge.
- Permanent feature entries survive checksum changes (subject to validity checks).

## D) Terrain Mods (`*_mods.json`)

Explicit terrain overrides applied after base map load.

Contains:

- point/range modifications
- old/new terrain/elevation metadata
- author/reason/timestamp
- permanence marker
- `base_map_checksum`

Rules:

- Applied only when checksum-compatible (or via explicit migration tooling).
- Never mixed with transient state actors.

---

## Region Model (Area-Native Source of Truth)

Regions are now part of area data and should be consumed by wilderness systems directly.

Contract:

- Area JSON region definitions are authoritative.
- Wilderness references area regions by region id/uid and bounds.
- Weather spawn weighting, NPC ship routing, spawn tables, hazard rates, and narration pull from region tags/profiles.

Wilderness-specific region usage:

- climate profiles (`humid_coast`, `arid`, `storm_belt`)
- navigation traits (`open_water`, `shoals`, `chokepoint`)
- spawn traits (`pirate_activity`, `merchant_lane`, `predator_zone`)
- accessibility/wayfinding labels for route narration

---

## Runtime Resolution Rules

Unify terrain/system reads behind a single resolver stack:

1. Read base tile/elevation from `wmap`
2. Apply compatible `_mods`
3. Apply persistent `vlinks` marker layer
4. Apply runtime overlays/state effects
5. Resolve effective terrain metadata via `wterr`

All gameplay systems must consume effective terrain, not direct `staticmap` reads.

---

## Virtual Room Group Loading (Keep VRooms, Add Batching)

The core model remains virtual rooms for memory efficiency at large map sizes (including historical 6000x6000 scale).  
Optimization layer: load/unload rooms in chunk groups instead of only one-by-one materialization.

### Chunk Activation Model

- Partition wilderness coordinates into fixed chunks (initial target: 32x32; tunable).
- Each chunk tracks:
   - loaded room count
   - active actors (players/ships/events)
   - last-access tick
   - pinned reason (vlink, encounter, script lock)

### Group Load Triggers

- Player enters an unloaded chunk.
- Nearby chunk prefetch around player movement heading.
- NPC ship/weather actors approach a chunk with player-proximate relevance.
- Explicit staff/admin prewarm command.

### Group Unload Rules

- Chunk has no players and no pinned reasons.
- Chunk TTL expired (cooldown window).
- Global memory pressure requires LRU chunk eviction.

### Safety/Performance Guards

- Hard cap on total loaded wilderness rooms.
- Hard cap on simultaneously loaded chunks per wilderness.
- Backoff when pulse time exceeds threshold (defer non-critical prefetch).
- Never load full wilderness at boot.

### Design Intent

- Preserve low memory profile of virtual rooms.
- Reduce per-step room churn by amortizing creation work over chunk groups.
- Support smoother traversal and ship/weather projection in active play corridors.

---

## Simulation Domains

## 1) Weather

- Coordinate actors with lifecycle, drift vector, intensity.
- Region-aware spawning/behavior from area-region profiles.
- Effects projected into overlays/state (visibility, movement penalty, hazard ticks).

## 2) NPC Ships

- Coordinate actors independent of loaded room count.
- Behavior profiles: patrol/trade/escort/pirate/faction response.
- Weather and region traits influence route choice/risk.

## 3) NPC Spawns / Features

- Spawn controllers keyed to region + terrain eligibility.
- Temporary and permanent wilderness features managed in `_state`.
- Terrain-changing design edits tracked in `_mods`.

---

## Elevation Rollout

Stage 1 (current):

- Validate elevation grid integrity during async import.

Stage 2:

- Persist elevation channel in `wmap`.
- Add effective elevation resolver.

Stage 3:

- Use elevation in movement/pathing/visibility/weather runoff rules.

---

## Regeneration + Merge Policy

When base map is regenerated/imported:

1. Load new `wmap` and compute checksum.
2. Attempt `_mods` apply:
   - checksum match -> apply
   - mismatch -> quarantine/backup and skip auto-apply
3. Load `_state`:
   - checksum match -> apply full
   - mismatch -> apply permanent-safe subset only; drop/reseed temporary actors
4. Persist updated state with new checksum and migration report.

---

## Phased Delivery Plan

## Corrective Alignment (2026-02-24)

The implementation sequence drifted from the contract. Specifically:

- `_state` / `_mods` persistence landed before dedicated `wmap` / `wterr` artifacts.
- Runtime persistence currently depends on area-serialized base map/terrain payloads.

To realign with contract intent, this plan now enforces the following gate:

- No additional state-manipulation feature expansion (weather/ships/spawn runtime state growth) until `wmap` + `wterr` read/write is implemented and active in the resolver load chain.

Current `_state` / `_mods` implementation remains valid as a provisional layer, but it is not considered contract-complete until `wmap` / `wterr` are first-class persisted artifacts.

## Phase 0 - Contract and Plumbing

- Add module boundaries for `wmap/wterr/state/mods` loaders.
- Define versioned schemas + checksum utility.
- Add startup/shutdown hooks for wilderness state persistence.

Status (2026-02-24):

- Implemented: module boundaries (`wilderness_storage`, `wilderness_state`, `wilderness_mods`).
- Implemented: checksum utility and version constants.
- Implemented: lifecycle hooks (init/load pass, pulse autosave pass, shutdown save pass).
- Deferred: dedicated `wmap/wterr` artifact load/save modules (base map and terrain dictionary still come from area serialization paths).

Exit criteria to leave Phase 0 (revised):

- `wmap` writer/reader exists and is wired for wilderness base payload load.
- `wterr` writer/reader exists and is wired for terrain dictionary load.
- Resolver base source is `wmap/wterr` (not direct area payload assumptions).
- Bootstrap fixture includes deterministic `wmap` + `wterr` assets.

Status update (2026-02-24, corrective pass):

- Implemented: `wmap` binary read/write module (`wilderness_wmap`) with storage lifecycle integration (load before `_mods/_state`, save before `_mods/_state`).
- Implemented: `wterr` JSON dictionary read/write module (`wilderness_wterr`) with storage lifecycle integration.
- Implemented: `vlinks` sidecar JSON module (`wilderness_vlinks`) with dedicated load/save lifecycle.
- Implemented: sidecar processing order aligned to resolver precedence (`wmap -> wterr -> mods -> vlinks -> state`).
- Implemented: automatic export migration pass that writes sidecar artifacts from existing in-zone wilderness payloads during storage init/save.
- Implemented: area JSON save cutover to metadata+association only for wilderness payloads (legacy embedded map/terrain/vlinks are no longer emitted in new saves).
- Pending gate item: bootstrap fixture assets/tests for sidecar-authoritative wilderness loading.

## Phase 1 - Effective Tile Unification

- Replace direct terrain reads with effective resolver calls.
- Ensure movement/map display/weather hooks read same source.
- Add regression tests for resolver consistency.

Phase boundary note:

- Phase 1 is runtime resolver/routing only (no persistence-schema commitments).
- Persistence formats, merge rules, and actor/state serialization remain Phase 2.

Status (2026-02-24):

- Implemented: active wilderness map render paths use effective tile resolver.
- Implemented: vlink map marker writes/restores route through runtime/base tile helpers.
- Confirmed: remaining direct map/static indexing in `wilds.c` is in legacy `#if 0` code path.

## Phase 1.4 - Hardening and Regression Safety

- Add regression tests for resolver consistency and crash-prone movement paths.
- Ensure wildcard/script movement into unresolved wilderness coordinates fails safe.
- Keep behavior stable while persistence schema work (Phase 2) remains isolated.

Status (2026-02-24):

- Implemented: resolver consistency unit suite (`wilderness_resolver_unit_tests`).
- Implemented: integration regression for wildcard/script movement fallback guard in `script_engine_tests`.
- Implemented: `char_to_vroom` null-room fallback to `room_default` to prevent boot/script crash loop.

Next (to enter Phase 2):

- Replace `_state`/`_mods` stubs with JSON serialization and checksum/version fields.
- Implement merge/quarantine behavior for base-map checksum drift.
- Bind actor/state persistence for dynamic camps, routes/schedules, spawn controllers, and weather cells.

## Bootstrap Fixture Rollout (post-Phase 2 core)

- Use `bootstrap_data/area/tests.json` as the wilderness fixture host area (avoid introducing an extra fixture area file).
- Add supporting bootstrap sidecar files under `bootstrap_data/world/wilderness_state/1_bootstrap_test_wilds/` (`wmap`, `wterr`, `mods`, `state`, `vlinks`) with deterministic content.
- Extend bootstrap fixture validation to assert bootstrap area discoverability under the two-area model (`bootstrap.json` + `tests.json`).
- Promote the wilderness bootstrap fixture checks into `bootstrap_ci` profile once stable.

Acceptance for rollout:

- `bootstrap_fixture_tests` validates wilderness fixture discoverability.
- At least one integration test runs against bootstrap wilderness fixture data (not ad-hoc in-memory only).
- Fixture data remains minimal and deterministic to keep CI runtime stable.

## Phase 1.5 - Chunk Group Materialization

- Add chunk index and chunk activity metadata to wilderness runtime.
- Implement load/unload of virtual rooms by chunk with TTL + LRU policy.
- Add prefetch ring around players and optional actor-driven prewarm hooks.
- Add telemetry for room churn, chunk churn, pulse time, and memory envelope.

Status (2026-02-24):

- Implemented (slice 1): chunk runtime metadata fields (`loaded_rooms`, `active_actors`, `last_access`, `pin_flags`).
- Implemented (slice 1): per-second chunk pulse (`wilds_chunk_pulse`) wired into update loop.
- Implemented (slice 1): TTL unload pass for inactive, unpinned chunk rooms.
- Implemented (slice 2): bounded prefetch ring around active wilderness chunks.
- Implemented (slice 2): explicit pressure eviction (LRU-style oldest-idle chunk unload under room/chunk caps).
- Implemented (slice 2): chunk telemetry logging for prefetch/unload activity.
- Implemented (slice 3): actor-driven prewarm hook on player wilderness entry.
- Implemented (slice 3): basic in-game chunk status visibility in `wlist` output.
- Pending: richer telemetry surfaces (admin command/report).

## Phase 2 - State + Mods Persistence (Contract-Ordered)

- Implement `_state` and `_mods` read/write.
- Add merge logic with quarantine/backup behavior.
- Add admin diagnostics (`status/checksum/merge report`).

Status (2026-02-24):

- Implemented out of order (provisional): `_mods` JSON read/write with schema/version/wilds uid/base-map checksum.
- Implemented out of order (provisional): checksum gating on `_mods` load (mismatch skip + warning).
- Implemented out of order (provisional): diff-based mod serialization (`staticmap` vs runtime `map`) and dirty integration via `set_wilds_runtime_tile`.
- Implemented out of order (provisional): `_state` JSON read/write with schema/version/wilds uid/base-map checksum.
- Implemented out of order (provisional): checksum-aware `_state` quarantine on mismatch (file rename + skip apply).
- Implemented out of order (provisional): runtime feature/actor record persistence and temporary zone state serialization.

Revalidation requirement (new):

- After `wmap/wterr` lands, rebind `_state/_mods` checksum + load/apply logic to `wmap` checksum source.
- Re-run integration tests for resolver, script movement fallback, and bootstrap fixture persistence.

## Immediate Execution Order (corrected)

1. Implement `wmap` artifact read/write (binary header, dimensions, terrain stream, checksum).
2. Implement `wterr` artifact read/write (terrain dictionary JSON + metadata schema/version).
3. Wire wilderness load pipeline to source base terrain + definitions from `wmap/wterr`.
4. Add bootstrap fixture `wmap/wterr/vlinks` files and fixture validation tests.
5. Re-run checksum/merge verification for `_mods/_state` against sidecar-authoritative load path.
6. Add explicit integration test for precedence (`mods` then `vlinks` then runtime `state` overlays).
7. Only then resume new state-manipulation domain work (weather/ships/spawns).

## Phase 3 - Area-Region Integration

- Bind wilderness simulation lookups to area-native regions.
- Add region profile tables for weather/ships/spawn weighting.
- Add script hook points by region transition.

## Phase 4 - Weather + NPC Ship Activation

- Reactivate weather actor loop on shared coordinate scheduler.
- Introduce NPC ship runtime manager with persistence.
- Couple weather + ships + region hazards.
- Use chunk-prewarm hooks for player-visible interactions (without global room loading).

## Phase 5 - NPC Spawn/Feature Expansion

- Add dynamic spawn controllers and feature lifecycle rules.
- Separate temporary event state from permanent world-change state.
- Add balancing/telemetry.

## Phase 6 - Elevation Enablement

- Turn on elevation channel application.
- Integrate elevation into pathing, visibility, and hazard simulation.

---

## Code Touchpoints (Planned)

- `wilds.c` / `wilds.h`: effective resolver + runtime layer integration
- `wilds_wildgen.c`: staged import output hooks + checksum handoff
- `weather.c`: coordinate-native weather actor persistence coupling
- `boat.c`: NPC ship manager integration and region/weather coupling
- `update.c`: scheduler ownership for wilderness simulation pulses
- `json_area.c`: area-region extraction helpers for wilderness runtime
- New modules:
  - `wilderness_storage.c/.h`
  - `wilderness_state.c/.h`
  - `wilderness_mods.c/.h`
   - `wilderness_wmap.c/.h`
   - `wilderness_wterr.c/.h`
   - `wilderness_vlinks.c/.h`

---

## Success Criteria

- Wilderness state survives reboot without requiring persistent room residency.
- Weather, NPC ships, and NPC spawns run at coordinate level independent of loaded vrooms.
- Region-aware simulation behavior uses area-native region definitions.
- Regeneration from wildgen/import can occur without corrupting runtime state.
- Elevation pipeline is validated now and activatable without schema redesign.
- Large wilderness maps avoid boot-time lag from full room creation.
- Runtime memory remains bounded via chunk + room caps under sustained load.

---

## Program Dependencies

- `WILDERNESS_ANALYSIS.md`
- `PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`
- `PLAN_SHIP_SYSTEM_ARCHITECTURE.md`
- `PLAN_CORE_SYSTEMS_INTEGRATION.md`
- `TODO_CORE_SYSTEMS_INTEGRATION.md`
