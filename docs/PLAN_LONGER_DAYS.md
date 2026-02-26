# Plan: Lengthen Game Day (15-Minute Ticks + Timestamp-Based Affects)

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Context

Currently, 1 tick (~60 real seconds) = 1 game hour, giving a 24-minute game day. The goal is to slow the day-night cycle so 1 tick = 15 game minutes, giving a 96-minute game day (96 ticks). This makes the world feel less rushed and allows for more granular time-of-day effects.

Simultaneously, the affect/spell duration system is being upgraded from tick-based countdown (1-hour granularity) to timestamp-based expiry with second-level precision. Durations will display as "2 hours 15 minutes 34 seconds" and effects can expire at any moment, not just on tick boundaries.

**Key invariant**: Real-world pacing stays the same. A buff that lasted 24 real minutes still lasts 24 real minutes. Regen that healed X per real minute still heals X per real minute. Only the in-game clock runs slower.

## New Constants

```c
#define TICKS_PER_HOUR         4    // 4 ticks per game hour (was 1)
#define GAME_MINUTES_PER_TICK  15   // Each tick advances 15 game minutes (was 60)
#define SECONDS_PER_TICK       (PULSE_TICK / PULSE_PER_SECOND)  // 60 real seconds
```

---

## Phase 1: Time System — Add Minute Granularity

**Files**: `merc.h`, `update.c`, `db.c`, `act_info.c`

### 1A. Add `minute` field to `time_info_data` (`merc.h:1278`)
```c
struct time_info_data {
    int minute;  // NEW: 0-59 game minutes
    int hour; int day; int month; int year; int moon;
};
```

### 1B. Change `time_update()` (`update.c:1387-1599`)
- Instead of `time_info.hour++`, advance by `GAME_MINUTES_PER_TICK` (15) minutes
- Roll over to next hour when `minute >= 60`
- Set a global `bool hour_changed` flag (used by Phase 4-6)
- Only fire hourly events (sunrise/sunset, boat arrivals, day rollover) when `hour_changed` is true
- Moon calculation unchanged — it uses `time_info.hour` which still counts 0-23

### 1C. Update boot time calculation (`db.c:~780`)
- Derive time from total ticks elapsed, compute minute/hour/day/month/year
- Initialize `time_info.minute` alongside existing fields

### 1D. Update time display (`act_info.c:~4854`)
- Show "It is 3:45 PM" instead of "It is 3 o'clock PM"

---

## Phase 2: Timestamp-Based Affect Expiry

**Files**: `merc.h`, `handler.c`, `update.c`, `act_info.c`, `mem.c`

### 2A. Add `time_t expires_at` to `affect_data` (`merc.h:2525`)
- `expires_at > 0`: absolute Unix timestamp when affect expires
- `expires_at == 0` with `duration < 0`: permanent affect
- `expires_at == 0` with `duration >= 0`: legacy (shouldn't occur after migration)

### 2B. Convert duration → timestamp in `affect_to_char()` / `affect_to_obj()` (`handler.c:~1457, ~1480`)
- When `duration > 0` and `expires_at == 0`: compute `expires_at = current_time + duration * SECONDS_PER_TICK`
- This means **no changes to spell code** — the 137 sites that set `af.duration = level/4` continue working. The conversion happens at apply time.

### 2C. Add per-second affect expiry sweep (`update.c`)
- New `affect_expiry_update()` function, called every `PULSE_PER_SECOND` (1 second)
- Iterates online characters + their objects, checks `current_time >= paf->expires_at`
- Removes expired affects with wear-off messages
- Lightweight: just timestamp comparison + removal

### 2D. Update `char_update()` affect loop (`update.c:2278-2307`)
- Remove the `paf->duration--` decrement for timestamp-based affects
- Keep spell level fade but adjust frequency: `number_range(0, 4 * TICKS_PER_HOUR - 1) == 0` to maintain same real-world fade rate
- Keep legacy duration path as fallback

### 2E. Same for `obj_update()` affect loop (`update.c:2419-2444`)

### 2F. Add `format_duration_seconds()` helper (`handler.c:~10982`)
- Converts seconds remaining to "X hours Y minutes Z seconds"
- Omits zero components (e.g., "45 minutes 12 seconds", not "0 hours 45 minutes 12 seconds")

### 2G. Update affect display (`act_info.c:4344-4374`)
- Change from `"for %d hours"` to `format_duration_seconds(expires_at - current_time)`
- Both custom-named and skill-named affect display blocks (two locations)

---

## Phase 3: JSON Persistence

**Files**: `io/json/json_char.c`, `io/json/json_persist.c`

### Save: Add `"expires_in"` field (seconds remaining)
- All affect save locations: if `expires_at > 0`, save `"expires_in": max(0, expires_at - current_time)`
- Keep saving `"duration"` for backward compatibility
- Locations: `json_char.c:1037`, `json_persist.c:518, 1607`

### Load: Prefer `"expires_in"`, fall back to legacy `"duration"`
- If `"expires_in"` exists: `expires_at = current_time + expires_in_value`
- If only `"duration"` exists and `> 0`: legacy conversion `expires_at = current_time + duration * 60`
- Preserves offline-freeze behavior (effects pause while logged out)
- Locations: `json_char.c:4380, 4912`, `json_persist.c:566, 1999`

### Area template affects (`json_area.c`) — NO changes needed
- These are templates; conversion happens at apply time in `affect_to_char()`

---

## Phase 4: Regeneration Scaling

**File**: `update.c:1793-1830`

- Divide `hit_gain()`, `mana_gain()`, `move_gain()`, `toxin_gain()` results by `TICKS_PER_HOUR` when applying
- Use `UMAX(1, gain / TICKS_PER_HOUR)` when `gain > 0` to prevent rounding to zero at low levels
- Alternative: accumulate remainder across ticks for exact parity (adds 3 int fields to CHAR_DATA)

---

## Phase 5: Condition Decay Gating

**File**: `update.c:2270-2275`

- Gate hunger/thirst/drunk/stoned/full decay on `hour_changed` flag
- Conditions only decay once per game hour (every 4 ticks) instead of every tick
- Maintains same real-world decay rate

---

## Phase 6: Other Tick-Dependent Timers

**File**: `update.c`

All gated on `hour_changed` to maintain real-world pacing:
- Token timers (`update.c:1747-1766`): `--token->timer` only on hour boundary
- Death timer (`update.c:1848-1881`): `ch->time_left_death--` only on hour boundary
- Maze timer (`update.c:1838-1846`): `ch->maze_time_left--` only on hour boundary
- Challenge delay (`update.c:1835`): `ch->pcdata->challenge_delay--` only on hour boundary
- Light duration (`update.c:1890-1901`): `--LIGHT(obj)->duration` only on hour boundary

---

## Phase 7: Scripting Engine

**File**: `script_commands.c`

- Script `ADDAFFECT`/`ADDAFFECTNAME` duration parameter — works automatically (conversion in `affect_to_char`)
- Script `SETAFFECT` duration field modification — update to adjust `expires_at` when modifying duration
- Script `GETAFFECT` duration field reading — return remaining seconds / 60 (remaining "hours") for backward compat, or add a new `remaining_seconds` field

---

## Verification

1. `cd /sentience/src && ./build tests && cd /sentience && ./sent -test`
2. Boot the game, verify time display shows minutes (e.g., "It is 3:45 PM")
3. Cast a spell, verify `affects` command shows countdown with seconds
4. Wait 60 real seconds, verify affect countdown decreased by ~60 seconds
5. Verify sunrise/sunset still happen at correct game hours (5am/6am/7pm/8pm)
6. Verify regen rate feels similar to before (not 4x faster)
7. Save a character with active affects, reload, verify durations preserved correctly
8. Test with a legacy save file (no `expires_in` field) to verify backward compat

---

## Implementation Order

1. Phase 1 (time system) — self-contained, testable independently
2. Phase 2A-2B (affect struct + conversion) — foundation
3. Phase 3 (persistence) — needed before testing affects across save/load
4. Phase 2C-2G (expiry sweep + display) — wire up the new system
5. Phase 4-6 (regen/conditions/timers) — simple rate adjustments
6. Phase 7 (scripting) — incremental, lower priority
