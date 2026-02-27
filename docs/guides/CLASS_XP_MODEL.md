# Class XP Model (Current + Proposed)

## Why this exists

The class system already supports per-class level and per-class XP storage, but upcoming job-style classes need:

1. Reliable XP required per level even when classes have different max levels
2. Configurable default XP curves (not only one hardcoded global table)
3. Multiple XP types (combat/crafting/gathering/exploration)
4. Class-level routing rules for which XP types can contribute

This guide documents current behavior and a low-risk extension path.

---

## Current behavior (as implemented)

### Newly implemented (this branch)

- Global named XP curves can now be loaded from `data/system/xp_curves.json`
- Class JSON supports:
	- `xp_curve` (named curve id)
	- `xp_accept` (accepted XP types: `combat`, `crafting`, `gathering`, `exploration`)
- XP threshold resolution order is now:
	1. `xp_table` (class override)
	2. `xp_curve` (class named curve)
	3. global `default_curve` from `xp_curves.json`
	4. built-in fallback table in code
- Typed award API now exists: `gain_exp_typed(ch, clazz, amount, xp_type, show)`
- Legacy `gain_exp()` is preserved and routes as untyped XP for compatibility
- Typed XP currently wired in:
	- combat kill XP (`XP_TYPE_COMBAT`)
	- quest completion XP (`XP_TYPE_EXPLORATION`)
	- token skill-learning XP (`XP_TYPE_EXPLORATION`)

### XP storage and leveling

- XP is tracked per class on `CLASS_LEVEL.xp`
- `gain_exp(ch, clazz, gain, show)` awards to target class, or active class if `clazz == NULL`
- If class XP reaches threshold, class level increases and class XP resets to 0
- `ch->exp` and `ch->level` are legacy mirrors of current class values

Relevant code:

- `gain_exp()` in `src/update.c`
- `exp_per_level()` in `src/skills.c`
- `class_exp_per_level()` in `src/class_data.c`

### XP per level

- `class_exp_per_level(clazz, level)` returns XP needed from `level -> level+1`
- Uses `clazz->xp_table` when present
- Falls back to hardcoded `default_xp_table` in `class_data.c`
- If `level >= clazz->max_level`, returns 0

### What already aligns with new goals

- Classes already have `type` (`mage/cleric/thief/warrior/crafting/gathering/explorer`)
- Reward scopes already support class/type/combat/always behavior for skill availability
- Per-class XP tables already exist

### Current gap

XP itself is not typed. All awards flow through one `gain_exp()` path and become generic class XP.

---

## Current OLC support for class XP settings

`ClsEdit` now supports direct per-class XP curve editing via `xptable`:

- `xptable` / `xptable show` — View effective table
- `xptable set <level> <xp>` — Set one level transition manually
- `xptable clear` — Remove custom table and use default built-in curve

Formula-based table generation is also available:

- `xptable calc linear <base> <step>`
- `xptable calc geometric <base> <percent>`
- `xptable calc quadratic <base> <step> <curve>`

These commands generate and apply a full custom table for levels `1..(max_level-1)`.

`ClsEdit` also supports the typed XP routing and named-curve settings directly:

- `xpaccept show` — Show effective accepted XP types (explicit or class default)
- `xpaccept list` — Show available XP type tokens
- `xpaccept set <type...>` — Replace accepted set (`combat crafting gathering exploration`)
- `xpaccept add <type>` / `xpaccept remove <type>` — Incremental edits
- `xpaccept default` — Clear override and use class-type defaults
- `xpcurve show` — Show active named curve (or default)
- `xpcurve list` — List all named curves loaded from `xp_curves.json`
- `xpcurve set <curve_id>` — Set class named curve
- `xpcurve clear` — Clear override and use global default curve

This removes the need to hand-edit class JSON for XP acceptance and curve selection.

---

## Proposed extension model

## 1) Add typed XP categories

Add enum + flags:

- `XP_TYPE_COMBAT`
- `XP_TYPE_CRAFTING`
- `XP_TYPE_GATHERING`
- `XP_TYPE_EXPLORATION`

(Optionally `XP_TYPE_QUEST`, `XP_TYPE_SOCIAL`, etc. later.)

Also add a bitmask helper set:

- `XP_MASK_COMBAT`, `XP_MASK_CRAFTING`, etc.
- `XP_MASK_NON_COMBAT = crafting|gathering|exploration`
- `XP_MASK_ANY = all`

## 2) Add class XP contribution policy

Each class gets policy fields:

- `xp_accept_mask` (which XP types can level this class)
- `xp_curve_id` (which default curve to use when class has no explicit `xp_table`)

Examples:

- Warrior: accept combat only
- Blacksmith: accept crafting only
- Ranger: combat + gathering + exploration
- Novice/generalist: any

This directly supports:

- class-specific (single type)
- class-type based
- combat/non-combat
- any

## 3) Add configurable default XP curves

Instead of a single hardcoded default table, support named curves loaded from JSON.

Suggested source:

- `data/system/xp_curves.json`

Suggested schema:

- `default_curve`: name
- `curves`: object of curve name -> array of level XP values

Then resolution order in `class_exp_per_level`:

1. Class `xp_table` override
2. Class `xp_curve_id` named curve
3. Global `default_curve`
4. Built-in fallback table (current hardcoded one)

This keeps compatibility while enabling flexible defaults.

## 4) Add typed gain API (without breaking old callers)

Keep existing API:

- `gain_exp(ch, clazz, gain, show)`

Add new API:

- `gain_exp_typed(ch, clazz, gain, xp_type, show)`

Behavior:

- Validate class target
- Check class `xp_accept_mask & xp_type`
- If not accepted, either discard or reroute (configurable)
- Apply existing level-up path unchanged

Then migrate call sites gradually:

- Kill XP -> combat
- Craft completion -> crafting
- Gather node harvest -> gathering
- Discovery/room/map milestone -> exploration

Old callers can default to combat (or generic) to remain stable during migration.

---

## Routing semantics (recommended)

Define explicit routing mode for typed XP event when `clazz == NULL`:

- `ACTIVE_ONLY`: award to active class only if it accepts type
- `ACTIVE_ELSE_FIRST_MATCH`: active class else first joined matching class
- `SPLIT_MATCHING`: split across all joined classes that accept type

Recommended default now: `ACTIVE_ONLY` for predictability and low risk.

Optional future mode for profession progression: `ACTIVE_ELSE_FIRST_MATCH`.

---

## Data model changes

## CLASS_DATA

Add:

- `long xp_accept_mask;`
- `char *xp_curve_id;` (or small fixed string)

JSON fields per class:

- `xp_accept`: array of strings (`["combat","exploration"]`)
- `xp_curve`: string (`"combat_fast"`)

## CLASS_LEVEL (optional analytics)

If desired, track per-type totals for visibility without changing level logic:

- `long xp_by_type[XP_TYPE_MAX];`

This is optional and can be phase 2.

---

## Migration plan

### Phase 1: Infrastructure only

- Add XP type enum/flags
- Add class policy fields + JSON read/write
- Add curve registry loader with fallback to current table
- Add `gain_exp_typed()` wrapper calling existing gain logic

No gameplay behavior change yet.

### Phase 2: Producer migration

Update XP producers to call typed API:

- Combat kill paths
- Crafting systems
- Gathering systems
- Exploration milestones

Keep legacy `gain_exp` available.

### Phase 3: UX and diagnostics

Add commands/displays:

- class XP acceptance summary
- class curve name
- optional per-type earned totals

### Phase 4: Policy tuning

Tune class acceptance masks and curve assignment per class identity.

---

## Validation checklist

- Class with custom `xp_table` still levels exactly as before
- Class with named curve and no table uses named curve
- Missing curve safely falls back to global default then built-in table
- Typed XP not accepted by class does not level class
- Legacy untyped XP call path still works
- Existing save/load of classes remains backward compatible

---

## Suggested defaults

- Combat classes: accept combat
- Crafting classes: accept crafting
- Gathering classes: accept gathering + exploration
- Hybrid adventure classes: combat + exploration
- General classes (if any): any

Use named curves to express pace:

- `combat_fast`
- `trade_medium`
- `explore_slow`

---

## Notes for implementation

- Preserve `gain_exp()` signature for scripts and old call sites
- Keep built-in fallback XP table to avoid boot failures
- Avoid changing level-up side effects; only change XP admission and threshold source
- Reuse existing class type and scope concepts where possible for consistency
