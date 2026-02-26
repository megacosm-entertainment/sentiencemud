# Leaderboard System: Shell Scripts → Redis Sorted Sets

## Context

The current leaderboard system uses external shell scripts that grep through every player file on disk, sort, and write `.info` files. The game loads these every 12 hours. This approach is fragile (scripts parse old pfile format that's being replaced by JSON), slow (12h staleness), expensive (full disk scan), and externally dependent. This migration moves leaderboards to real-time Redis sorted sets with in-memory fallback, eliminating the shell scripts entirely.

## Leaderboards

| Constant | Key | Description | Sort |
|----------|-----|-------------|------|
| `REPORT_TOP_PLAYER_KILLERS` (0) | `leaderboard:pkers` | PK kills | Descending |
| `REPORT_TOP_CPLAYER_KILLERS` (1) | `leaderboard:cpkers` | CPK kills | Descending |
| `REPORT_TOP_WEALTHIEST` (2) | `leaderboard:wealthiest` | Bank balance | Descending |
| `REPORT_TOP_WORST_RATIO` (3) | `leaderboard:ratio` | Win % (derived) | Ascending |
| `REPORT_TOP_MONSTER_KILLERS` (4) | `leaderboard:monsters` | Monster kills | Descending |
| `REPORT_TOP_QUESTS` (5) | `leaderboard:quests` | Quests completed | Descending |
| `REPORT_TOP_DEATHS` (7) **NEW** | `leaderboard:deaths` | Total deaths | Descending |

Ratio uses a separate Redis hash `leaderboard:ratio:data` storing per-player kills/deaths for recomputation. Min 50 total PvP fights threshold.

---

## Implementation Steps

### Step 1: Redis Sorted Set API — `redis_cache.h` / `redis_cache.c`

Add 7 new functions following existing pattern (check available, lock mutex, redisCommand, check reply, update stats, unlock):

```c
bool redis_leaderboard_update(const char *board, const char *player, double score);
    // ZADD leaderboard:<board> <score> <player>

int redis_leaderboard_get_top(const char *board, int max, char **names, double *scores);
    // ZREVRANGE leaderboard:<board> 0 <max-1> WITHSCORES

int redis_leaderboard_get_bottom(const char *board, int max, char **names, double *scores);
    // ZRANGE leaderboard:<board> 0 <max-1> WITHSCORES

bool redis_leaderboard_remove(const char *board, const char *player);
    // ZREM leaderboard:<board> <player>

void redis_leaderboard_remove_all(const char *player);
    // ZREM from all boards + HDEL ratio data

bool redis_leaderboard_set_ratio_data(const char *player, int kills, int deaths);
    // HSET leaderboard:ratio:data <player>:kills <kills> <player>:deaths <deaths>

int redis_leaderboard_get_ratio_data(int max, char **names, double *scores, int min_fights);
    // HGETALL leaderboard:ratio:data → compute ratios → sort ascending → return top N
```

No TTL on leaderboard keys — they persist until explicitly removed.

### Step 2: New Structs and Constants — `merc.h`

Add:
```c
#define MAX_LEADERBOARD_ENTRIES   10
#define MAX_LEADERBOARDS          8
#define LEADERBOARD_MIN_RATIO_FIGHTS  50
#define REPORT_TOP_DEATHS         7

typedef struct leaderboard_entry {
    char    name[MAX_INPUT_LENGTH];
    double  score;
    char    display_value[64];     // "42" or "31.50%"
} LEADERBOARD_ENTRY;

typedef struct leaderboard_data {
    int                 type;
    const char         *redis_key;
    const char         *report_name;   // "{YTop 10 {WPlayer Killers..."
    const char         *description;
    const char         *col_name;
    const char         *col_value;
    bool                descending;
    bool                is_derived;    // true for ratio
    LEADERBOARD_ENTRY   entries[MAX_LEADERBOARD_ENTRIES];
    int                 count;
    time_t              last_refresh;
} LEADERBOARD_DATA;
```

Add externs: `leaderboards[MAX_LEADERBOARDS]`, `leaderboard_refresh_time`, `leaderboard_backup_time`.

### Step 3: Rewrite `stats.c`

Replace `STAT_DATA stat_table[10]` with `LEADERBOARD_DATA leaderboards[MAX_LEADERBOARDS]`.

**New functions:**
- `leaderboard_init_all()` — populate metadata from static init table (titles/descriptions from existing .info files)
- `leaderboard_update_score(int type, const char *name, double score)` — update Redis + in-memory
- `leaderboard_update_inmemory(LEADERBOARD_DATA *lb, ...)` — sorted array insert/update (qsort with 10 elements)
- `leaderboard_on_player_kill(CHAR_DATA *ch, CHAR_DATA *victim)` — update PK/CPK/ratio/deaths boards for both players
- `leaderboard_update_ratio(CHAR_DATA *ch)` — compute kills/(kills+deaths), update ratio board
- `leaderboard_update_wealth(CHAR_DATA *ch)` — convenience for bankbalance updates
- `leaderboard_refresh_from_redis()` — pull top 10 from Redis into memory (every 5 min)
- `leaderboard_load_backup()` — read `data/stats/leaderboards.json` on boot
- `leaderboard_save_backup()` — write `data/stats/leaderboards.json` (temp+rename)
- `leaderboard_seed_redis()` — on boot, push backup to Redis if ZCARD==0, else refresh from Redis

**Preserved functions (reimplemented):**
- `do_stats()` — same args + add "deaths"; reads from `leaderboards[]` instead of `stat_table[]`
- `get_stats(int type)` — same `BUFFER *` return, same formatting, reads from `leaderboards[]`
- `get_stats_for_html(int type)` — same HTML output, reads from `leaderboards[]`

### Step 4: Boot Wiring — `db.c` line 1169-1170

Replace:
```c
stats_load_time = current_time;
load_statistics();
```
With:
```c
leaderboard_init_all();
leaderboard_load_backup();
leaderboard_seed_redis();
```

### Step 5: Periodic Updates — `update.c` lines 112-117

Replace 12h `.info` reload with:
```c
// Refresh leaderboard cache from Redis every 5 minutes
if (current_time >= leaderboard_refresh_time + 300) {
    leaderboard_refresh_from_redis();
    leaderboard_refresh_time = current_time;
}
// Backup leaderboards to disk every hour
if (current_time >= leaderboard_backup_time + 3600) {
    leaderboard_save_backup();
    leaderboard_backup_time = current_time;
}
```

### Step 6: Hook Installation

**fight.c — kills and deaths:**
- After `ch->monster_kills++` at lines 1690, 6068, 6854: `leaderboard_update_score(REPORT_TOP_MONSTER_KILLERS, ch->name, (double)ch->monster_kills);` (guarded by `!IS_NPC(ch)`)
- After `victim->deaths++` at line 3908: `leaderboard_update_score(REPORT_TOP_DEATHS, victim->name, (double)victim->deaths);` (guarded by `!IS_NPC(victim)`)
- End of `player_kill()` before closing `}` at line 8013: `leaderboard_on_player_kill(ch, victim);`

**quest.c — line 753:**
- After `ch->pcdata->quests_completed++`: `leaderboard_update_score(REPORT_TOP_QUESTS, ch->name, (double)ch->pcdata->quests_completed);`

**act_info.c — do_bank:**
- After line 6022 (deposit): `leaderboard_update_wealth(ch);`
- After line 6063 (withdraw): `leaderboard_update_wealth(ch);`
- After lines 6118-6119 (wire): `leaderboard_update_wealth(ch);` and `leaderboard_update_wealth(target);`

**auction.c — bankbalance changes:**
- After line 331 (bid refund): `leaderboard_update_wealth(auction_info.high_bidder);`
- After line 335 (bid payment): `leaderboard_update_wealth(ch);`
- After line 533 (sale proceeds): `leaderboard_update_wealth(auction_info.owner);`

**save.c — catch-all wealth on save:**
- End of `save_char_obj()`: `if (!IS_NPC(ch) && ch->pcdata) leaderboard_update_wealth(ch);`

**act_comm.c — character deletion:**
- At lines ~1601/1834 (retire/delete): `redis_leaderboard_remove_all(ch->name);`

### Step 7: Migration Command

Add `do_leaderboard` command (registered in `interp.c`):
- `leaderboard migrate` — scan all `characters/[a-z]/*.json` files, extract stats, populate boards
- `leaderboard refresh` — force Redis refresh
- `leaderboard backup` — force JSON backup
- `leaderboard status` — show board counts and last refresh times

Helper `json_read_leaderboard_stats()` in `json_char.c` — lightweight JSON reader that extracts only the numeric fields needed (no CHAR_DATA allocation).

### Step 8: Update `do_reloadstats` — `act_wiz.c` line 11478

Replace `load_statistics()` call with `leaderboard_refresh_from_redis()` + `leaderboard_save_backup()`.

### Step 9: Cleanup

- Remove/deprecate `load_statistics()`, `load_stat()` in `db2.c`
- Remove `STAT_DATA` struct and `stat_table[10]` from `merc.h`/`stats.c`
- Remove `stats_load_time` from `comm.c`/`merc.h`

---

## Files Modified

| File | Changes |
|------|---------|
| `io/cache/redis_cache.h` | Add 7 leaderboard function declarations |
| `io/cache/redis_cache.c` | Implement sorted set + hash operations |
| `merc.h` | Add structs, constants, externs; deprecate STAT_DATA |
| `stats.c` | Major rewrite: new backend, all new functions |
| `fight.c` | 5 hook insertions (lines 1690, 3908, 6068, 6854, 8013) |
| `quest.c` | 1 hook (line 753) |
| `act_info.c` | 4 hooks in do_bank (lines 6022, 6063, 6118, 6119) |
| `auction.c` | 3 hooks (lines 331, 335, 533) |
| `save.c` | 1 catch-all wealth hook in save_char_obj |
| `act_comm.c` | Leaderboard removal on char delete/retire |
| `db.c` | Replace boot loading (line 1170) |
| `db2.c` | Deprecate load_statistics/load_stat |
| `update.c` | Replace 12h reload with 5min refresh + 1h backup |
| `act_wiz.c` | Update do_reloadstats (line 11478) |
| `comm.c` | Add new global time vars |
| `interp.c` | Register "leaderboard" command |
| `interp.h` | Declare do_leaderboard |
| `io/json/json_char.c` | Add json_read_leaderboard_stats() |

No new `.c` files — no build file changes needed.

## Verification

1. `cd /sentience/src && ./build tests` — must compile clean
2. `cd /sentience && ./sent -test` — no new test failures
3. In-game: `leaderboard migrate` to seed from character files
4. In-game: `stats pkers`, `stats deaths`, etc. — verify display matches old format
5. Kill a mob, verify `stats monsters` updates in real-time
6. `leaderboard backup` then restart — verify data persists
7. Stop Redis, verify in-memory fallback still works
8. `leaderboard status` — verify refresh/backup timestamps
