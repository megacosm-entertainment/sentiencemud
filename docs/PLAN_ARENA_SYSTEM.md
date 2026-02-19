# Arena System Enhancement Plan

## Context

The current arena system is a simple 1v1 challenge between two players: one player challenges another, both are teleported to a single hardcoded arena, they fight with instant respawn, and return when leaving. Several places hardcode the area name `"Arena"` and `"Plith"` for tally/message logic.

The goal is to evolve this into a multi-arena system with configurable rulesets and tournament support, leveraging the existing event system and blueprint/instance infrastructure.

**Design decisions:**
- Single elimination tournaments only (initially)
- Arena config embedded in events for tournaments; standalone arenas reference blueprints for instancing
- Replace `challenge` command with a new `arena` command that supports arena/ruleset selection
- Support solo (1v1), party (team vs team), and FFA modes

---

## Phase 1: Arena Definitions and the `arena` Command

**Standalone value:** Multiple named arenas with rulesets, new `arena` command replaces `challenge`.

### 1.1 New Files

| File | Purpose |
|------|---------|
| `arena.h` | ARENA_DEF struct, ARENA_RULESET, rule flag constants, function prototypes |
| `arena.c` | Arena manager: load/save, lookup, transport, rule enforcement, `do_arena` command |
| `io/json/json_arena.c` | JSON load/save for `data/system/arenas.json` |
| `io/json/json_arena.h` | JSON function prototypes |

### 1.2 Data Structures (`arena.h`)

```c
/* Arena ruleset flags */
#define ARENA_RULE_NO_MAGIC        (A)
#define ARENA_RULE_NO_POTIONS      (B)
#define ARENA_RULE_NO_SCROLLS      (C)
#define ARENA_RULE_NO_STAVES       (D)
#define ARENA_RULE_NO_FLEE         (E)
#define ARENA_RULE_EQUALIZE        (F)   /* Normalize stats to a target level */
#define ARENA_RULE_NO_ITEMS        (G)   /* No consumable item use */
#define ARENA_RULE_NO_PETS         (H)   /* No pets/followers */

/* Arena mode - determines participation structure */
#define ARENA_MODE_DUEL            0     /* 1v1 only (classic challenge) */
#define ARENA_MODE_PARTY           1     /* Team vs team (party-based) */
#define ARENA_MODE_FFA             2     /* Free for all (multiple solo fighters) */

typedef struct arena_ruleset ARENA_RULESET;
struct arena_ruleset {
    long        flags;              /* ARENA_RULE_* bitfield */
    int16_t     mode;               /* ARENA_MODE_* */
    int16_t     min_party_size;     /* Min fighters per side (1 for solo, 2+ for party) */
    int16_t     max_party_size;     /* Max fighters per side (caps team size) */
    int16_t     max_combatants;     /* Max total fighters in FFA mode (0 = unlimited) */
    int16_t     equalize_level;     /* Target level for stat normalization (0 = none) */
    int16_t     respawn_hp_pct;     /* HP% on respawn (default 50) */
    int16_t     respawn_mana_pct;   /* Mana% on respawn (default 50) */
    int16_t     respawn_move_pct;   /* Move% on respawn (default 50) */
    char        *level_brackets;    /* Bracket spec: "1-50,51-90,91+" */
};

typedef struct arena_def ARENA_DEF;
struct arena_def {
    long            uid;
    char            *name;              /* Display name ("Plith Coliseum") */
    char            *description;
    char            *area_name;         /* Area name for room association (replaces hardcoded "Arena") */
    WNUM_LOAD       spawn_room;         /* Primary spawn room */
    WNUM_LOAD       spectator_room;     /* Optional spectator room */
    WNUM_LOAD       respawn_room;       /* Respawn room (0 = use area recall) */
    long            blueprint_vnum;     /* Blueprint for instanced arenas (0 = use static rooms) */
    ARENA_RULESET   ruleset;
    bool            enabled;
    bool            is_default;         /* Default arena for unspecified challenges */
    char            *announce_msg;
    char            *victory_msg;
    char            *comments;
    ARENA_DEF       *next;
};
```

Key design: `blueprint_vnum` allows an arena to reference a BLUEPRINT for instancing. When set, `arena_transport_to()` calls `create_instance()` to spin up a private arena instance for each match. When 0, the static `spawn_room` is used (current behavior).

### 1.3 Core Functions (`arena.c`)

```c
/* Lifecycle */
void        arena_init(void);                           /* Called from boot_db() */
void        arena_load(void);                           /* Load from data/system/arenas.json */
bool        arena_save(void);                           /* Save to JSON */

/* Lookup */
ARENA_DEF  *arena_find_uid(long uid);
ARENA_DEF  *arena_find_name(const char *name);
ARENA_DEF  *arena_find_default(void);
ARENA_DEF  *arena_find_by_area(const char *area_name);

/* Runtime queries */
ARENA_DEF  *arena_get_for_room(ROOM_INDEX_DATA *room);  /* Find arena def for a room */
bool        arena_check_action(CHAR_DATA *ch, int action, bool show_msg);

/* Transport */
bool        arena_transport_to(CHAR_DATA *ch, ARENA_DEF *arena);
bool        arena_return_from(CHAR_DATA *ch);

/* The new arena command */
DO_FUN(do_arena);
```

### 1.4 The `arena` Command (replaces `challenge`)

```
arena                       -- Show available arenas and your arena stats
arena list                  -- List all enabled arenas with rulesets
arena challenge <player> [arena_name]  -- Challenge player to duel (1v1)
arena challenge <player> party [arena] -- Challenge player's party (team vs team)
arena rules [arena_name]    -- Show detailed ruleset for an arena
arena accept                -- Accept a pending challenge
arena decline               -- Decline a pending challenge
arena leave                 -- Leave the arena (returns to pre-arena room)
arena spectate [arena_name] -- Move to spectator room
```

The old `challenge` command becomes a shim: `do_challenge()` calls `do_arena()` with `"challenge"` prepended, preserving backwards compat.

**Party challenge flow:**
- `arena challenge Bob party Coliseum` - challenges Bob's party with your party
- The challenger must be the group leader; the challenged player must also be a group leader
- All group members of both sides are transported to the arena
- Arena's `max_party_size` caps how many from each group can enter
- Win condition: last side with fighters standing wins
- On arena death: respawn at spectator room (if set) instead of recall, to watch the rest of the match
- When all fighters on one side are eliminated, the match ends

**FFA mode:**
- FFA arenas allow multiple participants without team structure
- Last fighter standing wins
- Used primarily for tournament FFA brackets or open arena combat

### 1.5 Ruleset Enforcement

Central function `arena_check_action()` called from:

| Rule Flag | Enforcement Point | File |
|-----------|-------------------|------|
| `ARENA_RULE_NO_MAGIC` | Before `do_cast()` proceeds | `magic.c` |
| `ARENA_RULE_NO_POTIONS` | In `do_quaff()` | `act_obj.c` |
| `ARENA_RULE_NO_SCROLLS` | In `do_recite()` | `act_obj.c` |
| `ARENA_RULE_NO_STAVES` | In `do_zap()`, `do_brandish()` | `act_obj2.c` |
| `ARENA_RULE_NO_FLEE` | In `do_flee()` | `fight.c` |
| `ARENA_RULE_NO_ITEMS` | Blanket in quaff/recite/eat/zap/brandish | Multiple |
| `ARENA_RULE_EQUALIZE` | Applied on transport, removed on return | `arena.c` |
| `ARENA_RULE_NO_PETS` | Prevent pets entering; block charm/order | `arena.c`, `magic.c` |

### 1.6 Modifications to Existing Code

**`fight.c` - `player_kill()` (~line 8036):**
Replace `!str_cmp(victim->in_room->area->name, "Arena")` with `arena_get_for_room(ch->in_room) != NULL`.

**`fight.c` - `raw_kill()` (~line 3745, ~line 3934):**
Replace hardcoded `"Plith"` area check with `arena_get_for_room()`. Use `arena_def->respawn_room` if set, else fall back to recall room.

**`fight.c` - `do_challenge()` (~line 6948):**
Rewrite as a thin shim that calls `do_arena(ch, "challenge ...")`. Keep the function for backwards compat.

**`act_move.c` (~line 1246):**
Replace `!str_cmp(ch->in_room->area->name, "Arena")` with `arena_get_for_room()` lookup.

**`interp.c` - challenge acceptance (~line 1390):**
Move acceptance logic into `arena.c` (`arena_accept_challenge()`). The interp.c intercept calls this new function instead of inline logic.

**`interp.c` - cmd_table:**
Add `{ "arena", do_arena, POS_SLEEPING, 0, LOG_ALWAYS, 1, false }`.

**`db.c` - `boot_db()`:**
Add `arena_init()` call during boot sequence.

**`magic.c`, `act_obj.c`, `act_obj2.c`, `fight.c`:**
Add `arena_check_action()` calls at relevant enforcement points.

**`CMakeLists.txt` and `Makefile`:**
Add `arena.c`, `io/json/json_arena.c`.

### 1.7 Persistence

File: `data/system/arenas.json`
```json
{
    "_format": "arenas",
    "_version": 1,
    "next_uid": 2,
    "arenas": [
        {
            "uid": 1,
            "name": "Plith Coliseum",
            "area_name": "Arena",
            "spawn_room": { "area": 0, "vnum": 10513 },
            "ruleset": { "flags": 0, "mode": 0, "respawn_hp_pct": 50 },
            "enabled": true,
            "is_default": true
        }
    ]
}
```

### 1.8 Migration

Create one default ARENA_DEF matching the current hardcoded arena. All existing behavior continues to work. The old `challenge` command becomes a shim to the new `arena challenge` command.

---

## Phase 2: Tournament Event Type

**Standalone value:** Automated single-elimination tournaments using the event system.

### 2.1 New Files

| File | Purpose |
|------|---------|
| `arena_tournament.c` | Tournament orchestration: bracket generation, match lifecycle, tick logic |
| `arena_tournament.h` | Tournament structs (TOURN_STATE, TOURN_MATCH, TOURN_ROUND) and prototypes |

### 2.2 Event System Extension

Add to event type enum in `evtedit.c`:
```c
EVT_TYPE_TOURNAMENT,    /* After EVT_TYPE_CUSTOM */
```

Add to `evt_type_flags` table:
```c
{ "tournament", EVT_TYPE_TOURNAMENT, true, NULL },
```

Add tournament fields to `EVENT_INDEX_DATA` (evtedit.c line 38):
```c
long        arena_uid;              /* Which ARENA_DEF to use */
int16_t     tournament_format;      /* TOURN_FORMAT_SINGLE_ELIM (only one for now) */
int16_t     match_time_limit;       /* Seconds per match (0 = no limit) */
int16_t     between_match_delay;    /* Seconds between matches */
int16_t     best_of;                /* Best of N (1, 3, 5) */
```

Add to `EVENT_INSTANCE`:
```c
void        *tournament;            /* TOURN_STATE* when type == TOURNAMENT */
```

### 2.3 Tournament Data Structures (`arena_tournament.h`)

```c
#define TOURN_FORMAT_SINGLE_ELIM    0

#define MATCH_PENDING       0
#define MATCH_TRANSPORTING  1   /* Players being moved to arena */
#define MATCH_IN_PROGRESS   2
#define MATCH_COMPLETE      3
#define MATCH_BYE           4
#define MATCH_FORFEIT       5

#define TOURN_PHASE_SIGNUP      0
#define TOURN_PHASE_BRACKETS    1   /* Brackets generated, matches queuing */
#define TOURN_PHASE_COMPLETE    2

/* A match side: solo fighter or a party */
typedef struct match_side MATCH_SIDE;
struct match_side {
    CHAR_DATA       **fighters;         /* Array of fighters on this side */
    char            **fighter_names;    /* Cached names for disconnect handling */
    int             count;              /* Number of fighters */
    int             alive;              /* Fighters still standing */
};

typedef struct tournament_match TOURN_MATCH;
struct tournament_match {
    int             match_id;
    int             round_num;
    MATCH_SIDE      side1;              /* First side (solo or party) */
    MATCH_SIDE      side2;              /* Second side (solo or party) */
    MATCH_SIDE      *winning_side;      /* Points to side1 or side2 */
    int             state;
    time_t          started_at;
    int             time_limit;
    TOURN_MATCH     *winner_advances_to;
    int             winner_slot;        /* 1 or 2 */
    INSTANCE        *arena_instance;    /* Instanced arena for this match (NULL if static) */
    TOURN_MATCH     *next;
};

typedef struct tournament_round TOURN_ROUND;
struct tournament_round {
    int             round_num;
    TOURN_MATCH     *matches;
    bool            complete;
    TOURN_ROUND     *next;
};

typedef struct tournament_state TOURN_STATE;
struct tournament_state {
    TOURN_ROUND     *rounds;
    int             current_round;
    int             total_rounds;
    int             format;
    ARENA_DEF       *arena;
    int             match_time_limit;
    int             between_match_delay;
    int             best_of;
    int             next_match_id;
    int             phase;              /* TOURN_PHASE_* */
    time_t          signup_ends;        /* When signup period closes */
    time_t          next_match_time;    /* Delay timer for next match start */
};
```

### 2.4 Tournament Lifecycle

```
1. Staff creates event: evtedit create "Weekly Tournament"
   - Set type=tournament, arena_uid=1, format=single_elim
   - Set duration (signup period), match_time_limit, between_match_delay

2. Staff starts event: event start "Weekly Tournament"
   - EVENT_INSTANCE created, TOURN_STATE allocated
   - Phase = TOURN_PHASE_SIGNUP
   - Announcement broadcast, signup period begins

3. Players join: event join "Weekly Tournament"
   - Added as EVENT_PART (existing participant system)
   - Level bracket validation if arena has brackets
   - For party tournaments: group leader joins, registering the whole party as a unit
   - For solo tournaments: individual signup

4. Signup period ends (duration timer expires or staff command):
   - arena_tournament_generate_brackets() called
   - Single-elim bracket tree built from participants
   - Byes assigned if not power-of-2 count
   - Phase = TOURN_PHASE_BRACKETS
   - Bracket announcement broadcast

5. Match execution (in arena_tournament_tick, called from event_runtime_update):
   - Pick next MATCH_PENDING match in current round
   - If arena has blueprint_vnum: create_instance() for private arena
   - Transport fighters (solo or party) via arena_transport_to()
   - State = MATCH_IN_PROGRESS, timer starts

6. Match resolution:
   - On kill: arena_tournament_on_kill() tracks eliminations per side
   - Party match: when all fighters on one side are eliminated, that side loses
   - Solo match: single kill ends the match
   - On timeout: decide by remaining HP% or declare draw
   - On disconnect: forfeit after grace period
   - Return all players from arena
   - Advance winning side in bracket

7. Round progression:
   - When all matches in round complete, advance current_round
   - Queue next round's matches

8. Tournament completion:
   - Final match winner = tournament champion
   - Announce winner, award rewards
   - Phase = TOURN_PHASE_COMPLETE
   - event_stop_definition() called
```

### 2.5 Key Functions (`arena_tournament.c`)

```c
/* Called from event_runtime_update() every second */
void    arena_tournament_tick(EVENT_INSTANCE *inst);

/* Called from event_progress_record_kill() for PvP kills */
void    arena_tournament_on_kill(EVENT_INSTANCE *inst, CHAR_DATA *killer, CHAR_DATA *victim);

/* Bracket generation */
bool    arena_tournament_generate_brackets(EVENT_INSTANCE *inst);

/* Match management */
bool    arena_tournament_start_match(EVENT_INSTANCE *inst, TOURN_MATCH *match);
void    arena_tournament_end_match(TOURN_MATCH *match, MATCH_SIDE *winner);
void    arena_tournament_advance_round(EVENT_INSTANCE *inst);

/* Player disconnect handling */
void    arena_tournament_on_disconnect(CHAR_DATA *ch);

/* Display */
void    arena_tournament_show_bracket(CHAR_DATA *ch, EVENT_INSTANCE *inst);

/* Cleanup */
void    arena_tournament_free(TOURN_STATE *state);
```

### 2.6 Integration with Instancing

When an `ARENA_DEF` has `blueprint_vnum` set:
- Each tournament match calls `create_instance(blueprint)` to spin up a private arena
- Fighters are transported into the instance entrance room
- On match completion, fighters are returned and the instance is destroyed
- This means concurrent matches in the same tournament can each have their own arena

When `blueprint_vnum` is 0:
- Static `spawn_room` is used (current behavior)
- Only one match can be active at a time (matches are serialized)

### 2.7 Modifications to Existing Code

**`evtedit.c`:**
- Add `EVT_TYPE_TOURNAMENT` to type enum and flag table
- Add tournament fields to `EVENT_INDEX_DATA`
- Add tournament OLC fields (arena, format, matchtime, matchdelay, bestof)
- In `event_runtime_update()`: call `arena_tournament_tick()` for tournament instances
- In `event_start_definition()`: allocate TOURN_STATE for tournament events
- In `event_stop_definition()`: free TOURN_STATE
- JSON save/load for new fields

**`event_types.c` / `event_types.h`:**
- Add `arena_tournament_on_kill()` call in kill progress handler
- Add prototypes

**`fight.c` - `player_kill()`:**
- Existing `event_progress_record_kill()` call handles this; just need the new case in the progress handler

**`CMakeLists.txt` and `Makefile`:**
- Add `arena_tournament.c`

### 2.8 Player-Facing Command Extension

Extend the `arena` command:
```
arena tournament [name]         -- Show bracket/status for active tournament
arena tournament list           -- List active/upcoming tournaments
```

The `event join/leave/info/status` commands continue to work for tournaments since they're standard events.

---

## Phase 3: OLC Editor and Polish

**Standalone value:** In-game arena editing, spectator mode, script integration.

### 3.1 Arena OLC Editor (`editors/arenas/arenedit.c`)

Follow the evtedit pattern:
- `ED_ARENA` constant (value 41) in `olc.h`
- `ARENEDIT()` macro
- Fields: name, description, area, spawnroom, spectatorroom, respawnroom, blueprint, rules, mode, partysize, equalize, brackets, enabled, default, announce, victory, save, reload
- Registered as `do_arenedit` in cmd_table

### 3.2 Spectator Mode

- `arena spectate [name]` moves player to `spectator_room` with `room_before_arena` saved
- Combat messages from arena rooms forwarded to spectator room
- Spectators cannot interact with combatants
- `arena leave` returns spectators too
- Eliminated party members in team matches respawn to spectator room to watch

### 3.3 Script Interface

Extend `script_commands.c`:
```
arena info <name|uid>           -- get arena def info
arena transport <char> <uid>    -- transport char to arena
arena return <char>             -- return char from arena
arena check <char> <rule>       -- check if arena rule applies
```

Add script ifchecks in `script_ifc.c`:
- `inarena` -- is character in any arena
- `arenaname` -- name of current arena
- `arenarule <rule>` -- does current arena have this rule

### 3.4 Tournament State Persistence

For surviving reboots/copyover:
- Save active tournament state to `data/system/tournament_<uid>_<instance>.json`
- Reload on boot, resolve player names to CHAR_DATA on login
- Matches in progress at reboot time are forfeited

### 3.5 Tests

- `tests/data/arena_tests.json` and `tests/unit/arena_tests.c` for arena def CRUD, ruleset checking
- `tests/data/tournament_tests.json` and `tests/unit/tournament_tests.c` for bracket generation, match progression

---

## File Summary

### New Files (Phase 1)
- `arena.h` - Arena data structures and prototypes
- `arena.c` - Arena manager, `do_arena` command, rule enforcement
- `io/json/json_arena.c` - Arena JSON persistence
- `io/json/json_arena.h` - JSON prototypes

### New Files (Phase 2)
- `arena_tournament.h` - Tournament structures and prototypes
- `arena_tournament.c` - Tournament orchestration

### New Files (Phase 3)
- `editors/arenas/arenedit.c` - Arena OLC editor
- `tests/unit/arena_tests.c` - Arena unit tests
- `tests/data/arena_tests.json` - Arena test scenarios

### Modified Files (Phase 1)
- `merc.h` - Forward-declare ARENA_DEF, include arena.h
- `fight.c` - Replace hardcoded "Arena"/"Plith", add rule enforcement in do_flee
- `act_move.c` - Replace hardcoded "Arena" area name check
- `interp.c` - Move challenge acceptance to arena.c, add arena command, update do_challenge as shim
- `magic.c` - Add arena_check_action() for no-magic rule
- `act_obj.c` - Add arena_check_action() for no-potions/scrolls
- `act_obj2.c` - Add arena_check_action() for no-staves
- `db.c` - Call arena_init() during boot
- `tables.c` - Add do_arena to function table
- `CMakeLists.txt` - Add new source files
- `Makefile` - Add new source files

### Modified Files (Phase 2)
- `editors/events/evtedit.c` - EVT_TYPE_TOURNAMENT, tournament fields, OLC, tick hook
- `event_types.c` / `event_types.h` - Tournament kill handler
- `CMakeLists.txt` / `Makefile` - Add arena_tournament.c

### Modified Files (Phase 3)
- `olc.h` - Add ED_ARENA
- `script_commands.c` - Arena script commands
- `script_ifc.c` - Arena ifchecks

---

## Verification

### Phase 1
1. `./build` from `/sentience/src` - compiles clean
2. Create `data/system/arenas.json` with default Plith arena
3. Boot game, verify `arena list` shows the default arena
4. `arena challenge <player> [arena]` works, teleports to correct arena
5. Kill tally still works in arena (no longer hardcoded to "Arena" name)
6. Arena rules enforced: test no-magic arena blocks casting
7. `arena leave` returns to pre-arena room
8. Party challenge: both groups transported, team elimination works

### Phase 2
1. `./build` compiles clean
2. Create tournament event via evtedit with arena_uid set
3. `event start` begins signup, `event join` adds participants
4. Signup period ends, bracket generated and announced
5. Matches auto-start, fighters transported to arena
6. Kill resolves match, winner advances
7. Final match winner announced, tournament ends cleanly
8. Party tournament: team brackets work correctly

### Phase 3
1. `arenedit create <name>` creates arena in-game
2. All OLC fields settable and saved
3. Script commands work from mob/room progs
4. Tournament survives copyover (state reloaded)
