# PLAN: Preferences as Source of Truth

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Why this note exists

We currently represent player settings in **two places**:

1. Runtime bitfields (`act[]`, `act2[]`, `comm`)
2. Preference records (`pcdata->preferences`, account prefs, game default prefs)

This split creates drift. A recent example: `COMM_MXP` can be saved as enabled in preference data, then appear disabled after load because flag serialization/deserialization paths and preference-apply paths are not fully aligned.

## Modernization decision

**Preferences are the source of truth for user-configurable settings.**

Bitfields remain a runtime projection/cache for fast checks and legacy compatibility, but they are not authoritative configuration state.

## Scope

Applies to settings already modeled in `pc_set_table[]`, including:

- `comm`-backed toggles (e.g., `mxp`, `prompt`, `brief`, `compact`, etc.)
- `act`/`act2`-backed toggles exposed through prefs
- channel mute/display preferences

## Target behavior

1. Load preferences first (game defaults -> account -> character overrides).
2. Derive runtime flags from effective preferences.
3. Runtime toggles must write preference overrides.
4. Save path should persist preferences; flags are derived output (or compatibility mirror), not configuration input.
5. On login, no downstream pass should silently override a setting that has an explicit character/account preference.

## Transitional constraints

During migration, legacy saves may still include bitfields. While this remains true:

- Unknown flag bits must not be dropped during JSON round-trips.
- Preference-backed bits should be recomputed from effective preferences after load.
- Any compatibility merge logic must prefer explicit character/account preference values.

## Implementation plan

### Phase 1 (safety + correctness)

- Ensure comm/plr flag tables are complete for serialized names.
- Preserve unknown bits during deserialization where legacy numeric fields exist.
- Audit login sequence for ordering issues (`load -> apply prefs -> runtime init`).

### Phase 2 (single write path)

- Route all player-facing toggle commands through preference update helpers.
- Remove direct long-term reliance on raw bit toggling in command handlers.
- Centralize effective setting computation in one module.

### Phase 3 (de-dup + cleanup)

- Treat serialized raw flags as compatibility fields only.
- Prefer saving canonical preference state; optionally keep raw fields as generated mirrors.
- Remove obsolete migration shims once all active pfiles are on modern format.

## Acceptance criteria

- Toggling `mxp` ON survives logout/login across multiple sessions.
- Character override beats account default; account default beats game default.
- No preference-backed setting flips unexpectedly during login.
- Regression tests cover at least one `comm` toggle (`mxp`) and one inverted toggle.

## Follow-up tasks

- Add automated integration test for preference persistence order.
- Add debug trace hook for effective setting resolution at login.
- Document all preference-backed flags and whether they are inverted.
