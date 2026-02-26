# Flag Conversion Phase 2 - Remaining Flag Banks

## Overview

This document tracks the conversion of remaining flag banks from bitvector-based `(A)`, `(B)`, etc. constants to string-based constants using the `flagset_t` system.

**Current Status:** ~445 uses of `IS_SET_OLD`/`SET_BIT_OLD`/`REMOVE_BIT_OLD`/`TOGGLE_BIT_OLD` remain across the codebase.

**Approach:** Convert each flag bank to use `flagset_t` where the structure allows. This requires:
1. Changing the structure field from `int`/`long` to `flagset_t`
2. Converting flag constants from `(A)`, `(B)` to string names
3. Updating all code to use flagset operations
4. Updating save/load to use string-based serialization

For flags stored in `obj->value[]` arrays, these will remain as integer storage using `_BITS` macros until the src_20_dev type-specific structure migration is complete.

**Note on JSON Migration:** All flag conversions should use string-based flagsets which serialize naturally to JSON as space-separated strings or arrays. This aligns with the planned migration of area files and other data to JSON format.

---

## Conversion Categories

### Category A: Full Flagset Conversion (Structure Change)

These flags are stored in dedicated structure fields that can be changed from `int`/`long` to `flagset_t`:

| Structure | Field | Current Type | Flag Prefix |
|-----------|-------|--------------|-------------|
| `EXIT_DATA` | `exit_info` | `int` | `EX_` |
| `EXIT_DATA` | `rs_flags` | `int` | `EX_` |
| `LOCK_STATE` | `flags` | `int` | `LOCK_` |
| `TOKEN_DATA` | `flags` | `long` | `TOKEN_` |
| `TOKEN_INDEX_DATA` | `flags` | `long` | `TOKEN_` |
| `SHOP_DATA` | `flags` | `int` | `SHOPFLAG_` |
| `BAN_DATA` | `ban_flags` | `int16_t` | `BAN_` |
| `CHURCH_DATA` | `settings` | `long` | `CHURCH_` (settings) |
| `CHURCH_LOG_ENTRY` | `categories` | `flag_t` | `CHLOG_` |
| `INSTANCE` | `flags` | `int` | `INSTANCE_` |
| `DUNGEON` | `flags` | `int` | `DUNGEON_` |

### Category B: _BITS Macros (Keep Integer Storage)

These flags are stored in `obj->value[]` arrays which will be replaced by type-specific structures in the src_20_dev merge:

| Object Type | Value Index | Flag Prefix | Future Structure |
|-------------|-------------|-------------|------------------|
| Container | `value[1]` | `CONT_` | `CONTAINER_DATA.flags` |
| Gate/Portal | `value[2]` | `GATE_` | `PORTAL_DATA.flags` |
| Furniture | `value[2]` | `STAND_/SIT_/REST_/SLEEP_/PUT_` | `FURNITURE_DATA.flags` |
| Weapon | `value[4]` | (weapon flags) | `WEAPON_ATTACK_POINT.flags` |

### Category C: Small/Specialized (Evaluate Case-by-Case)

| Flag Prefix | Usage | Notes |
|-------------|-------|-------|
| `SKILL_` | Skill sources | Small set, evaluate if worth converting |
| `CMD_TYPE_` | Command types | Used for categorization |
| `CMD_` | Command flags | Small set |
| `FLAG_` | Channel display | Player preferences |
| `CORPSE_` | Corpse state | In `obj->value[4]` |
| `CATALYST_` | Catalyst search | Bitfield for search scope |
| `PROG_` | Script flags | Runtime flags |
| `IMMORTAL_` | Duty flags | Admin permissions |
| `PROJECT_` | Project flags | Admin tracking |
| `BSFLAG_` | Blueprint section | Section properties |
| `ACCT_` | Account flags | Account permissions |
| `AFFFLAG_` | Affect meta flags | Affect behavior |
| `DONE_` | Combat tracking | Single flag currently |
| `INSTRUMENT_` | Instrument flags | Single flag currently |
| `CHURCH_PERM_` | Church permissions | Already has table |
| `CHURCH_PLAYER_` | Church player flags | Single flag currently |
| `CHURCH_RANK_` | Church rank flags | Single flag currently |

---

## Flag Banks Requiring Conversion

### Priority 1: High-Use Flags (~350 uses)

| Flag Bank | Prefix | Uses | Table Name | Status |
|-----------|--------|------|------------|--------|
| Exit flags | `EX_` | ~130 | `exit_flags` | Table exists |
| Lock flags | `LOCK_` | ~45 | `lock_flags` | Table exists |
| Container flags | `CONT_` | ~25 | `container_flags` | Table exists |
| Furniture flags | `STAND_/SIT_/REST_/SLEEP_/PUT_` | ~40 | `furniture_flags` | Table exists |
| Gate/Portal flags | `GATE_` | ~15 | `portal_exit_flags` | Table exists |
| Shop flags | `SHOPFLAG_` | ~15 | `shop_flags` | Table exists |
| Token flags | `TOKEN_` | ~15 | `token_flags` | Table exists |
| Church permission | `CHURCH_PERM_` | ~20 | `church_permission_flags` | Table exists |
| Church player | `CHURCH_PLAYER_` | ~5 | Need table |
| Church rank | `CHURCH_RANK_PROTECTED` | ~6 | Need table |
| Ban flags | `BAN_` | ~15 | Need table |
| Instance flags | `INSTANCE_` | ~8 | `instance_flags` | Table exists |

### Priority 2: Medium-Use Flags (~50 uses)

| Flag Bank | Prefix | Uses | Table Name | Status |
|-----------|--------|------|------------|--------|
| Church settings | `CHURCH_SHOW_PKS`, etc. | ~5 | Need table |
| Church log | `CHLOG_` | ~15 | `church_log_category_flags` | Table exists |
| Dungeon flags | `DUNGEON_` | ~5 | `dungeon_flags` | Table exists |
| Corpse flags | `CORPSE_` | ~10 | Need table |
| Catalyst flags | `CATALYST_ROOM`, etc. | ~5 | Need table |

### Priority 3: Low-Use Flags (~45 uses)

| Flag Bank | Prefix | Uses | Table Name | Status |
|-----------|--------|------|------------|--------|
| Skill source | `SKILL_` | ~5 | Need table |
| Command type | `CMD_TYPE_` | ~5 | Need table |
| Command flags | `CMD_` | ~3 | Need table |
| Channel display | `FLAG_` | ~5 | `channel_flags` | Table exists |
| Immortal duties | `IMMORTAL_` | ~5 | `immortal_flags` | Table exists |
| Project flags | `PROJECT_` | ~3 | `project_flags` | Table exists |
| Prog/Script flags | `PROG_` | ~5 | Need table |
| Ship flags | `SHIP_` | ~3 | `ship_flags` | Table exists |
| Blueprint section | `BSFLAG_` | ~3 | Need table |
| Account flags | `ACCT_` | ~3 | Need table |
| Affect flags | `AFFFLAG_` | ~2 | Need table |
| Instrument flags | `INSTRUMENT_ONEHANDED` | ~2 | `instrument_flags` | Table exists |
| Done flags | `DONE_` | ~1 | Need table |
| War flags | `AUTO_WAR_` | ~1 | Not flags (enum values) |
| Wilderness types | `WILDERNESS_MAIN`, etc. | ~2 | Not flags (bitfield for type selection) |

---

## Files with Most Conversions Needed

| File | Count | Primary Flag Types |
|------|-------|-------------------|
| act_move.c | 154 | EX_*, GATE_*, furniture |
| act_info.c | 94 | Various display checks |
| church.c | 36 | CHURCH_*, CHLOG_* |
| db.c | 23 | EX_*, loading |
| act_enter.c | 22 | EX_*, GATE_* |
| blueprint.c | 19 | BSFLAG_*, INSTANCE_* |
| act_obj.c | 17 | CONT_*, furniture |
| ban.c | 15 | BAN_* |
| olc_save.c | 12 | Various (saving) |
| act_wiz.c | 11 | Various admin |
| fight.c | 9 | CORPSE_* |

---

## Full Flagset Conversion Process

For flags in Category A (structure field change to flagset_t):

### Step 1: Update Structure Definition (merc.h)

```c
// BEFORE:
struct exit_data {
    int exit_info;
    int rs_flags;
    // ...
};

// AFTER:
struct exit_data {
    flagset_t exit_info;
    flagset_t rs_flags;
    // ...
};
```

### Step 2: Convert Constants (merc.h)

```c
// BEFORE:
#define EX_ISDOOR       (A)
#define EX_CLOSED       (B)
#define EX_LOCKED       (C)

// AFTER:
#define EX_ISDOOR       "isdoor"
#define EX_CLOSED       "closed"
#define EX_LOCKED       "locked"
```

### Step 3: Update Memory Management (mem.c)

Add initialization and cleanup:

```c
// In new_exit():
flagset_init(&pExit->exit_info);
flagset_init(&pExit->rs_flags);

// In free_exit():
flagset_free(&pExit->exit_info);
flagset_free(&pExit->rs_flags);

// In copy functions if applicable:
flagset_copy(&dest->exit_info, &src->exit_info);
```

### Step 4: Update Save/Load Functions

```c
// SAVE (olc_save.c or similar):
// BEFORE:
fprintf(fp, "D%d %d %ld %ld\n", door, pexit->exit_info, pexit->key, pexit->u1.vnum);

// AFTER:
fprintf(fp, "ExitInfo %s~\n", flagset_to_string(&pexit->exit_info));

// LOAD (db.c or similar):
// BEFORE:
pexit->exit_info = fread_number(fp);

// AFTER:
char *flags = fread_string(fp);
flagset_from_string(&pexit->exit_info, flags, exit_flags);
free_string(flags);
```

### Step 5: Update All Flag Operations

```c
// IS_SET_OLD -> flagset_isset
// BEFORE:
if (IS_SET_OLD(pexit->exit_info, EX_CLOSED))

// AFTER:
if (flagset_isset(&pexit->exit_info, EX_CLOSED, exit_flags))

// SET_BIT_OLD -> flagset_set
// BEFORE:
SET_BIT_OLD(pexit->exit_info, EX_CLOSED);

// AFTER:
flagset_set(&pexit->exit_info, EX_CLOSED, exit_flags);

// REMOVE_BIT_OLD -> flagset_remove
// BEFORE:
REMOVE_BIT_OLD(pexit->exit_info, EX_CLOSED);

// AFTER:
flagset_remove(&pexit->exit_info, EX_CLOSED, exit_flags);

// TOGGLE_BIT_OLD -> flagset_toggle
// BEFORE:
TOGGLE_BIT_OLD(pexit->exit_info, EX_CLOSED);

// AFTER:
flagset_toggle(&pexit->exit_info, EX_CLOSED, exit_flags);
```

### Step 6: Update Assignment/Copy Operations

```c
// Direct assignment (often for reset states):
// BEFORE:
pexit->exit_info = pexit->rs_flags;

// AFTER:
flagset_copy(&pexit->exit_info, &pexit->rs_flags);

// Clear all flags:
// BEFORE:
pexit->exit_info = 0;

// AFTER:
flagset_clear(&pexit->exit_info);
```

### Step 7: Update Comparison Operations

```c
// Check if empty:
// BEFORE:
if (pexit->exit_info == 0)

// AFTER:
if (flagset_isempty(&pexit->exit_info))

// Check if flags equal:
// BEFORE:
if (pexit->exit_info == pexit->rs_flags)

// AFTER:
if (flagset_equal(&pexit->exit_info, &pexit->rs_flags))
```

---

## _BITS Macro Patterns (Category B - Integer Storage)

For flags stored in `obj->value[]` arrays, use `_BITS` macros until src_20_dev migration:

### Pattern 1: Simple Flag Check
```c
// BEFORE:
if (IS_SET_OLD(obj->value[1], CONT_CLOSED))

// AFTER:
if (IS_SET_BITS(obj->value[1], CONT_CLOSED, container_flags))

// merc.h constant change:
// #define CONT_CLOSED (C)
// becomes:
#define CONT_CLOSED "closed"
```

### Pattern 2: Flag Setting
```c
// BEFORE:
SET_BIT_OLD(obj->value[1], CONT_CLOSED);

// AFTER:
SET_BIT_BITS(obj->value[1], CONT_CLOSED, container_flags);
```

### Pattern 3: Flag Removal
```c
// BEFORE:
REMOVE_BIT_OLD(obj->value[1], CONT_CLOSED);

// AFTER:
REMOVE_BIT_BITS(obj->value[1], CONT_CLOSED, container_flags);
```

### Pattern 4: Flag Toggle
```c
// BEFORE:
TOGGLE_BIT_OLD(pexit->exit_info, EX_CLOSED);

// AFTER:
TOGGLE_BIT_BITS(pexit->exit_info, EX_CLOSED, exit_flags);
```

---

## Tables to Create

### 1. Ban Flags Table (tables.c)
```c
const struct flag_type ban_flags[] = {
    { "suffix",     0, true },   // BAN_SUFFIX
    { "prefix",     1, true },   // BAN_PREFIX
    { "permit",     2, true },   // BAN_PERMIT
    { "all",        3, true },   // BAN_ALL
    { "newbies",    4, true },   // BAN_NEWBIES
    { "email",      5, true },   // BAN_EMAIL
    { "permanent",  6, true },   // BAN_PERMANENT
    { NULL,         0, false }
};
```

### 2. Corpse Flags Table
```c
const struct flag_type corpse_flags[] = {
    { "cpkdeath",    0, true },  // CORPSE_CPKDEATH
    { "ownerloot",   1, true },  // CORPSE_OWNERLOOT
    { "charred",     2, true },  // CORPSE_CHARRED
    { "frozen",      3, true },  // CORPSE_FROZEN
    { "melted",      4, true },  // CORPSE_MELTED
    { "withered",    5, true },  // CORPSE_WITHERED
    { "pkdeath",     6, true },  // CORPSE_PKDEATH
    { "arenadeath",  7, true },  // CORPSE_ARENADEATH
    { "immortal",   25, true },  // CORPSE_IMMORTAL (Z)
    { NULL,          0, false }
};
```

### 3. Catalyst Search Flags Table
```c
const struct flag_type catalyst_search_flags[] = {
    { "room",        0, true },  // CATALYST_ROOM
    { "containers",  1, true },  // CATALYST_CONTAINERS
    { "carry",       2, true },  // CATALYST_CARRY
    { "worn",        3, true },  // CATALYST_WORN
    { "hold",        4, true },  // CATALYST_HOLD
    { "active",      5, true },  // CATALYST_ACTIVE
    { NULL,          0, false }
};
```

### 4. Church Player Flags Table
```c
const struct flag_type church_player_flags[] = {
    { "excommunicated", 0, true },  // CHURCH_PLAYER_EXCOMMUNICATED
    { NULL,             0, false }
};
```

### 5. Church Settings Flags Table
```c
const struct flag_type church_settings_flags[] = {
    { "show_pks",        0, true },  // CHURCH_SHOW_PKS
    { "allow_crosszones",1, true },  // CHURCH_ALLOW_CROSSZONES
    { "public_motd",     2, true },  // CHURCH_PUBLIC_MOTD
    { "public_rules",    3, true },  // CHURCH_PUBLIC_RULES
    { "public_info",     4, true },  // CHURCH_PUBLIC_INFO
    { NULL,              0, false }
};
```

### 6. Church Rank Flags Table
```c
const struct flag_type church_rank_flags[] = {
    { "protected", 0, true },  // CHURCH_RANK_PROTECTED
    { NULL,        0, false }
};
```

### 7. Skill Source Flags Table
```c
const struct flag_type skill_source_flags[] = {
    { "spell",     0, true },  // SKILL_SPELL
    { "practice",  1, true },  // SKILL_PRACTICE
    { "improve",   2, true },  // SKILL_IMPROVE
    { "favourite", 3, true },  // SKILL_FAVOURITE
    { NULL,        0, false }
};
```

### 8. Command Type Flags Table
```c
const struct flag_type cmd_type_flags[] = {
    { "none",      0, true },  // CMD_TYPE_NONE
    { "move",      1, true },  // CMD_TYPE_MOVE
    { "combat",    2, true },  // CMD_TYPE_COMBAT
    { "object",    3, true },  // CMD_TYPE_OBJECT
    { "info",      4, true },  // CMD_TYPE_INFO
    { "comm",      5, true },  // CMD_TYPE_COMM
    { "racial",    6, true },  // CMD_TYPE_RACIAL
    { "ooc",       7, true },  // CMD_TYPE_OOC
    { "immortal",  8, true },  // CMD_TYPE_IMMORTAL
    { "olc",       9, true },  // CMD_TYPE_OLC
    { "admin",    10, true },  // CMD_TYPE_ADMIN
    { "newbie",   11, true },  // CMD_TYPE_NEWBIE
    { NULL,        0, false }
};
```

### 9. Command Flags Table
```c
const struct flag_type cmd_flags[] = {
    { "hide_lists", 0, true },  // CMD_HIDE_LISTS
    { "is_ooc",     1, true },  // CMD_IS_OOC
    { NULL,         0, false }
};
```

### 10. Script/Prog Flags Table
```c
const struct flag_type prog_flags[] = {
    { "nodestruct", 0, true },  // PROG_NODESTRUCT
    { "at",         1, true },  // PROG_AT
    { "nodamage",   2, true },  // PROG_NODAMAGE
    { "norawkill",  3, true },  // PROG_NORAWKILL
    { "silent",     4, true },  // PROG_SILENT
    { NULL,         0, false }
};
```

### 11. Blueprint Section Flags Table
```c
const struct flag_type bpsection_flags[] = {
    { "no_rotate", 0, true },  // BSFLAG_NO_ROTATE
    { NULL,        0, false }
};
```

### 12. Account Flags Table
```c
const struct flag_type account_flags[] = {
    { "can_create_staff",      0, true },  // ACCT_CAN_CREATE_STAFF
    { "can_link",              1, true },  // ACCT_CAN_LINK
    { "can_unlink",            2, true },  // ACCT_CAN_UNLINK
    { "can_delete_immediately",3, true },  // ACCT_CAN_DELETE_IMMEDIATELY
    { NULL,                    0, false }
};
```

### 13. Affect Flags (Meta) Table
```c
const struct flag_type affect_meta_flags[] = {
    { "nodispel",  0, true },  // AFFFLAG_NODISPEL
    { "nocancel",  1, true },  // AFFFLAG_NOCANCEL
    { NULL,        0, false }
};
```

### 14. Done Flags Table
```c
const struct flag_type done_flags[] = {
    { "reverie", 0, true },  // DONE_REVERIE
    { NULL,      0, false }
};
```

---

## Constant Conversions (merc.h)

Each constant needs to be converted from bitvector to string:

### Exit Flags (EX_*)
```c
// OLD:
#define EX_ISDOOR       (A)
#define EX_CLOSED       (B)
// ... etc

// NEW:
#define EX_ISDOOR       "isdoor"
#define EX_CLOSED       "closed"
#define EX_LOCKED       "locked"
#define EX_HIDDEN       "hidden"
#define EX_FOUND        "found"
#define EX_PICKPROOF    "pickproof"
#define EX_NOPASS       "nopass"
#define EX_EASY         "easy"
#define EX_HARD         "hard"
#define EX_INFURIATING  "infuriating"
#define EX_NOCLOSE      "noclose"
#define EX_NOLOCK       "nolock"
#define EX_BROKEN       "broken"
#define EX_BARRED       "barred"
#define EX_NOBASH       "nobash"
#define EX_WALKTHROUGH  "walkthrough"
#define EX_NOBAR        "nobar"
#define EX_VLINK        "vlink"
#define EX_AERIAL       "aerial"
#define EX_NOHUNT       "nohunt"
#define EX_ENVIRONMENT  "environment"
#define EX_NOUNLINK     "nounlink"
#define EX_PREVFLOOR    "prevfloor"
#define EX_NEXTFLOOR    "nextfloor"
#define EX_NOSEARCH     "nosearch"
#define EX_MUSTSEE      "mustsee"
```

### Lock Flags (LOCK_*)
```c
#define LOCK_LOCKED     "locked"
#define LOCK_MAGIC      "magic"
#define LOCK_SNAPKEY    "snapkey"
#define LOCK_SCRIPT     "script"
#define LOCK_NOREMOVE   "noremove"
#define LOCK_BROKEN     "broken"
#define LOCK_JAMMED     "jammed"
#define LOCK_NOJAM      "nojam"
#define LOCK_CREATED    "created"
```

### Container Flags (CONT_*)
```c
#define CONT_CLOSEABLE  "closeable"
#define CONT_PICKPROOF  "pickproof"
#define CONT_CLOSED     "closed"
#define CONT_LOCKED     "locked"
#define CONT_PUT_ON     "put_on"
#define CONT_SNAPKEY    "snapkey"
#define CONT_PUSHOPEN   "pushopen"
#define CONT_CLOSELOCK  "closelock"
```

### Furniture Flags
```c
#define STAND_AT        "stand_at"
#define STAND_ON        "stand_on"
#define STAND_IN        "stand_in"
#define SIT_AT          "sit_at"
#define SIT_ON          "sit_on"
#define SIT_IN          "sit_in"
#define REST_AT         "rest_at"
#define REST_ON         "rest_on"
#define REST_IN         "rest_in"
#define SLEEP_AT        "sleep_at"
#define SLEEP_ON        "sleep_on"
#define SLEEP_IN        "sleep_in"
#define PUT_AT          "put_at"
#define PUT_ON          "put_on"
#define PUT_IN          "put_in"
#define PUT_INSIDE      "put_inside"
```

### Gate Flags (GATE_*)
```c
#define GATE_NORMAL_EXIT    "normal_exit"
#define GATE_NOCURSE        "nocurse"
#define GATE_GOWITH         "gowith"
#define GATE_BUGGY          "buggy"
#define GATE_RANDOM         "random"
#define GATE_AREARANDOM     "arearandom"
#define GATE_GRAVITY        "gravity"
#define GATE_NOSNEAK        "nosneak"
#define GATE_NOPRIVACY      "noprivacy"
#define GATE_SAFE           "safe"
#define GATE_SILENTENTRY    "silententry"
#define GATE_SILENTEXIT     "silentexit"
#define GATE_SNEAK          "sneak"
#define GATE_TURBULENT      "turbulent"
#define GATE_CANDRAGITEMS   "candragitems"
#define GATE_FORCE_BRIEF    "force_brief"
#define GATE_DUNGEON        "dungeon"
#define GATE_SECTIONRANDOM  "sectionrandom"
#define GATE_INSTANCERANDOM "instancerandom"
#define GATE_DUNGEONRANDOM  "dungeonrandom"
```

*(Additional constant conversions for remaining flag banks follow the same pattern)*

---

## Notes on src_20_dev Object Changes

The `src_20_dev` directory contains significant refactoring of object data structures:

1. **Type-specific structures**: Objects now use dedicated structures (`WEAPON_DATA`, `ARMOR_DATA`, `CONTAINER_DATA`, etc.) instead of generic `value[]` arrays.

2. **Impact on flag conversions**: Many flags currently stored in `obj->value[N]` will move to dedicated fields in these structures. For example:
   - Container flags in `obj->value[1]` -> `CONTAINER_DATA.flags`
   - Gate flags in `obj->value[2]` -> `PORTAL_DATA.flags`
   - Weapon flags in `obj->value[4]` -> `WEAPON_ATTACK_POINT.flags`

3. **Migration strategy**:
   - First, convert current flags to string-based `_BITS` macros
   - Later, when merging src_20_dev changes, the flags will naturally migrate to the new structures
   - The string-based flag names will remain consistent across both systems

---

## Validation Commands

After each conversion phase:

```bash
# Check remaining OLD macro usage
grep -c "IS_SET_OLD\|SET_BIT_OLD\|REMOVE_BIT_OLD" *.c | grep -v ":0$"

# Compile
make clean && make

# Check for specific flag prefix
grep -rn "IS_SET_OLD.*EX_" --include="*.c" .    # Should be empty after EX_ conversion
grep -rn "IS_SET_OLD.*CONT_" --include="*.c" .  # Should be empty after CONT_ conversion
```

---

## Execution Order

1. **Exit flags (EX_*)** - Highest use, foundational
2. **Lock flags (LOCK_*)** - Frequently used with exits
3. **Container flags (CONT_*)** - Object interactions
4. **Furniture flags** - Position handling
5. **Gate flags (GATE_*)** - Portal system
6. **Church flags** - All CHURCH_* variants
7. **Shop flags (SHOPFLAG_*)** - Economy
8. **Token flags (TOKEN_*)** - Token system
9. **Ban flags (BAN_*)** - Admin tools
10. **Instance/Dungeon flags** - Instancing system
11. **Remaining miscellaneous flags**

---

## Post-Conversion Cleanup

After all conversions are complete:

1. Remove `_OLD` macros from merc.h (or rename to `_RAW` for truly binary flags)
2. Delete all `.bak*` files
3. Full compilation test
4. Runtime testing of affected systems
5. Final commit

---

## Related Documentation

- [CLAUDE_FLAGSET_MIGRATION.md](CLAUDE_FLAGSET_MIGRATION.md) - Original flagset migration plan
- [CLAUDE_REMAINING_FLAGS.md](CLAUDE_REMAINING_FLAGS.md) - Previous remaining flags document

---

*Last updated: 2026-01-16*
