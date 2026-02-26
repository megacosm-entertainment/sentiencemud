# Plan: Runtime Corpse Type Registry

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Goal

Move corpse types out of compile-time `RAWKILL_*` space so builders can add new corpse types without code changes, while preserving compatibility with existing saves and scripts.

## Current State

- Corpse presentation data is JSON-backed and editable (`corpsedit`, `corpses.json`).
- Runtime still relies on compile-time type IDs/constants:
  - `RAWKILL_*` and `RAWKILL_MAX` in `merc.h`
  - `dam_to_corpse` in `tables.c`
  - `corpse_blending` in `tables.c`
- Corpse identity is currently numeric and fixed.

## Migration Strategy

### Phase 1 (in progress): API decoupling

- Stop direct `RAWKILL_MAX` assumptions in editor/json code.
- Route type metadata access through helper APIs (`corpse_type_count`, `corpse_type_lookup`, `corpse_type_name`).
- Keep current numeric IDs and behavior unchanged.

### Phase 2: Introduce runtime type keys

- Add stable string key per corpse type (e.g. `"normal"`, `"charred"`, `"ashen"`).
- Persist both:
  - `id` (legacy numeric, for backward compatibility)
  - `key` (new canonical runtime identity)
- Add lookup maps:
  - key -> runtime index
  - legacy id -> runtime index (for migrated saves)

### Phase 3: Runtime registry container

- Replace fixed `corpse_info_table[]` assumptions with a dynamic registry loaded from JSON.
- Maintain deterministic load order and explicit validation for duplicate keys/ids.
- Add startup validation and fail-safe fallback to baseline builtin set.

### Phase 4: Data-driven corpse routing

- Move damage-to-corpse mapping (`dam_to_corpse`) into JSON:
  - keyed by damage class name
  - weighted result list by corpse key
- Move blend matrix (`corpse_blending`) into JSON:
  - key-key -> result key
  - support wildcard and symmetric rules
- Add OLC commands or admin import path for editing these rules safely.

### Phase 5: Save/Script compatibility

- Object/corpse persistence:
  - save corpse type key alongside numeric type
  - on load: prefer key; fallback to numeric id
- Script commands that accept corpse types:
  - support type key strings
  - keep legacy names as aliases
- Keep `RAWKILL_*` aliases only for compatibility paths, then deprecate.

### Phase 6: Cleanup

- Remove compile-time routing tables and `RAWKILL_MAX` hard dependency.
- Reduce `RAWKILL_*` usage to compatibility shim layer only.
- Add migration tests for:
  - old corpse ids loading correctly
  - new runtime-only types participating in kill/decay/blend flow

## Risk Notes

- Save compatibility is the highest risk area; dual-read (key + id) is required during transition.
- Reordering runtime type definitions must not mutate legacy corpse identity unintentionally.
- Routing validation must catch unknown type keys at boot time.

## Suggested Acceptance Criteria

1. New corpse type added only in JSON can be selected by damage routing and created in game.
2. Existing player/object data using legacy numeric IDs still loads without behavior changes.
3. Scripts using legacy names still function; new keys are accepted.
4. Build contains no direct gameplay dependency on `RAWKILL_MAX` sizing.
