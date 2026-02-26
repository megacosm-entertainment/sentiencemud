# Complete Flagset Migration - Remaining Work

## Overview

The main flagset migration is complete. What remains are **~400 uses** of IS_SET_OLD/SET_BIT_OLD across these categories:

## Remaining Flag Types (by frequency)

| Category | Uses | Variables | Constants |
|----------|------|-----------|-----------|
| Exit flags | ~130 | `pexit->exit_info`, `ex->rs_flags` | EX_* |
| Object value flags | ~100 | `obj->value[1]`, `obj->value[2]`, `obj->value[4]` | CONT_*, GATE_*, WEAPON_TWO_HANDS |
| Lock flags | ~45 | `obj->lock->flags`, `pexit->door.lock.flags` | LOCK_* |
| Ban flags | ~15 | `pban->ban_flags` | BAN_* |
| Token flags | ~15 | `token->flags`, `token_index->flags` | TOKEN_* |
| Shop flags | ~8 | `keeper->shop->flags` | SHOPFLAG_* |
| Instance flags | ~8 | `instance->flags` | INSTANCE_* |
| Channel display flags | ~5 | `ch->pcdata->channel_flags` | FLAG_* (FLAG_TELLS, etc.) |
| Misc | ~10 | Various | Various |

**Note:** `church->flag` and `ch->pcdata->flag` are **display strings** (clan tags), not bitmasks.

## Recommended Approach: Hybrid with _BITS Macros

Since many of these flags are embedded in value[] arrays or exit_info fields, full conversion to flagset_t is complex. Instead:

1. **Convert constants to strings** (human-readable)
2. **Keep storage as integers** (no structure changes)
3. **Use the _BITS macros** for string-based access to integer storage

This gives you human-readable flag names without restructuring data.

---

## Phase 1: Exit Flags (EX_*) - ~130 uses

### Step 1.1: Create exit_flags table (tables.c)

Add after other flag tables:

```c
const struct flag_type exit_flags[] = {
    { "isdoor",      0, true },   // A
    { "closed",      1, true },   // B
    { "locked",      2, true },   // C
    { "hidden",      3, true },   // D
    { "found",       4, true },   // E
    { "pickproof",   5, true },   // F
    { "nopass",      6, true },   // G
    { "easy",        7, true },   // H
    { "hard",        8, true },   // I
    { "infuriating", 9, true },   // J
    { "noclose",    10, true },   // K
    { "nolock",     11, true },   // L
    { "broken",     12, true },   // M
    { "barred",     13, true },   // N
    { "nobash",     14, true },   // O
    { "walkthrough",15, true },   // P
    { "nobar",      16, true },   // Q
    { "vlink",      17, true },   // R
    { "aerial",     18, true },   // S
    { "nohunt",     19, true },   // T
    { "environment",20, true },   // U
    { "nounlink",   21, true },   // V
    { "prevfloor",  22, true },   // W
    { "nextfloor",  23, true },   // X
    { "nosearch",   24, true },   // Y
    { "mustsee",    25, true },   // Z
    { NULL,          0, false }
};
```

### Step 1.2: Declare in tables.h

```c
extern const struct flag_type exit_flags[];
```

### Step 1.3: Convert constants (merc.h)

```c
// OLD:
#define EX_ISDOOR     (A)
#define EX_CLOSED     (B)
// ...

// NEW:
#define EX_ISDOOR     "isdoor"
#define EX_CLOSED     "closed"
#define EX_LOCKED     "locked"
#define EX_HIDDEN     "hidden"
#define EX_FOUND      "found"
#define EX_PICKPROOF  "pickproof"
#define EX_NOPASS     "nopass"
#define EX_EASY       "easy"
#define EX_HARD       "hard"
#define EX_INFURIATING "infuriating"
#define EX_NOCLOSE    "noclose"
#define EX_NOLOCK     "nolock"
#define EX_BROKEN     "broken"
#define EX_BARRED     "barred"
#define EX_NOBASH     "nobash"
#define EX_WALKTHROUGH "walkthrough"
#define EX_NOBAR      "nobar"
#define EX_VLINK      "vlink"
#define EX_AERIAL     "aerial"
#define EX_NOHUNT     "nohunt"
#define EX_ENVIRONMENT "environment"
#define EX_NOUNLINK   "nounlink"
#define EX_PREVFLOOR  "prevfloor"
#define EX_NEXTFLOOR  "nextfloor"
#define EX_NOSEARCH   "nosearch"
#define EX_MUSTSEE    "mustsee"
```

### Step 1.4: Convert usages

Search and replace pattern:

```c
// OLD:
IS_SET_OLD(pexit->exit_info, EX_CLOSED)
SET_BIT_OLD(pexit->exit_info, EX_CLOSED)
REMOVE_BIT_OLD(pexit->exit_info, EX_CLOSED)

// NEW:
IS_SET_BITS(pexit->exit_info, EX_CLOSED, exit_flags)
SET_BIT_BITS(pexit->exit_info, EX_CLOSED, exit_flags)
REMOVE_BIT_BITS(pexit->exit_info, EX_CLOSED, exit_flags)
```

### Files to modify:
- act_move.c (~40 uses)
- handler.c (~20 uses)
- db.c, db2.c (~15 uses)
- olc_act.c, olc_save.c (~15 uses)
- act_obj.c, act_obj2.c (~10 uses)
- wilds.c (~10 uses)
- Various others (~20 uses)

**Find all with:**
```bash
grep -rn "IS_SET_OLD.*exit_info\|SET_BIT_OLD.*exit_info\|REMOVE_BIT_OLD.*exit_info" --include="*.c" .
grep -rn "IS_SET_OLD.*rs_flags\|SET_BIT_OLD.*rs_flags" --include="*.c" .
```

---

## Phase 2: Container Flags (CONT_*) - ~60 uses in value[1]

### Step 2.1: Create container_flags table (tables.c)

```c
const struct flag_type container_flags[] = {
    { "closeable",  0, true },   // A
    { "pickproof",  1, true },   // B
    { "closed",     2, true },   // C
    { "locked",     3, true },   // D
    { "put_on",     4, true },   // E
    { "snapkey",    5, true },   // F
    { "pushopen",   6, true },   // G
    { "closelock",  7, true },   // H
    { NULL,         0, false }
};
```

### Step 2.2: Convert constants (merc.h)

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

### Step 2.3: Convert usages

```c
// OLD:
IS_SET_OLD(obj->value[1], CONT_CLOSED)

// NEW:
IS_SET_BITS(obj->value[1], CONT_CLOSED, container_flags)
```

**Find all with:**
```bash
grep -rn "IS_SET_OLD.*value\[1\].*CONT_\|IS_SET_OLD.*CONT_" --include="*.c" .
```

---

## Phase 3: Gate/Portal Flags (GATE_*) - ~40 uses in value[2]

### Step 3.1: Create gate_flags table (tables.c)

```c
const struct flag_type gate_flags[] = {
    { "normal_exit",   0, true },   // A
    { "nocurse",       1, true },   // B
    { "gowith",        2, true },   // C
    { "buggy",         3, true },   // D
    { "random",        4, true },   // E
    { "arearandom",    5, true },   // F
    { "gravity",       6, true },   // G
    { "nosneak",       7, true },   // H
    { "noprivacy",     8, true },   // I
    { "safe",          9, true },   // J
    { "silententry",  10, true },   // K
    { "silentexit",   11, true },   // L
    { "sneak",        12, true },   // M
    { "turbulent",    13, true },   // N
    { "candragitems", 14, true },   // O
    { "force_brief",  15, true },   // P
    { "dungeon",      16, true },   // Q
    { "sectionrandom",17, true },   // R
    { "instancerandom",18, true },  // S
    { "dungeonrandom",19, true },   // T
    { NULL,            0, false }
};
```

### Step 3.2: Convert constants (merc.h)

```c
#define GATE_NORMAL_EXIT   "normal_exit"
#define GATE_NOCURSE       "nocurse"
#define GATE_GOWITH        "gowith"
// ... etc
```

### Step 3.3: Convert usages

```c
// OLD:
IS_SET_OLD(portal->value[2], GATE_DUNGEON)

// NEW:
IS_SET_BITS(portal->value[2], GATE_DUNGEON, gate_flags)
```

---

## Phase 4: Lock Flags (LOCK_*) - ~45 uses

### Step 4.1: Create lock_flags table (tables.c)

```c
const struct flag_type lock_flags[] = {
    { "locked",    0, true },   // A
    { "magic",     1, true },   // B
    { "snapkey",   2, true },   // C
    { "script",    3, true },   // D
    { "noremove",  4, true },   // E
    { "broken",    5, true },   // F
    { "jammed",    6, true },   // G
    { "nojam",     7, true },   // H
    { "created",  25, true },   // Z
    { NULL,        0, false }
};
```

### Step 4.2: Convert constants and usages

Same pattern as above.

---

## Phase 5: Token Flags (TOKEN_*) - ~15 uses

### Step 5.1: Create token_flags table (tables.c)

```c
const struct flag_type token_flags[] = {
    { "purge_death",   0, true },   // A
    { "purge_idle",    1, true },   // B
    { "purge_quit",    2, true },   // C
    { "purge_reboot",  3, true },   // D
    { "purge_rift",    4, true },   // E
    { "reversetimer",  5, true },   // F
    { "casting",       6, true },   // G
    { "noskilltest",   7, true },   // H
    { "singular",      8, true },   // I
    { "see_all",       9, true },   // J
    { "spellbeats",   10, true },   // K
    { "permanent",    25, true },   // Z
    { NULL,            0, false }
};
```

---

## Phase 6: Remaining Small Categories

### Ban Flags (~15 uses)
```c
const struct flag_type ban_flags[] = {
    { "suffix",    0, true },
    { "prefix",    1, true },
    { "permit",    2, true },
    { "all",       3, true },
    { "newbies",   4, true },
    { "email",     5, true },
    { NULL,        0, false }
};
```

### Shop Flags (~8 uses)
```c
const struct flag_type shop_flags[] = {
    { "tax_exempt",    0, true },
    { "only_church",   1, true },
    // ... add others as found
    { NULL,            0, false }
};
```

### Instance Flags (~8 uses)
```c
const struct flag_type instance_flags[] = {
    // ... define based on existing INSTANCE_* constants
    { NULL,            0, false }
};
```

### Channel Display Flags (~5 uses)
```c
const struct flag_type channel_display_flags[] = {
    { "gossip",   0, true },   // A - show flag on gossip
    { "ooc",      1, true },   // B
    { "yell",     2, true },   // C
    { "flaming",  3, true },   // D
    { "quote",    4, true },   // E
    { "helper",   5, true },   // F
    { "tells",    6, true },   // G
    { "music",    7, true },   // H
    { "ct",       8, true },   // I - church talk
    { NULL,       0, false }
};
```

---

## Execution Order

Work through in order of most uses first:

1. **Exit flags (EX_*)** - 130 uses, biggest impact
2. **Container flags (CONT_*)** - 60 uses
3. **Gate flags (GATE_*)** - 40 uses
4. **Lock flags (LOCK_*)** - 45 uses
5. **Token flags (TOKEN_*)** - 15 uses
6. **Ban flags (BAN_*)** - 15 uses
7. **Shop flags (SHOPFLAG_*)** - 8 uses
8. **Instance flags** - 8 uses
9. **Channel display flags (FLAG_*)** - 5 uses

---

## Per-Phase Workflow

For each category:

1. **Create the flag table** in tables.c
2. **Declare it** in tables.h
3. **Convert the #define constants** in merc.h from `(A)` to `"string"`
4. **Find all uses:**
   ```bash
   grep -rn "IS_SET_OLD.*CONSTANT_PREFIX\|SET_BIT_OLD.*CONSTANT_PREFIX" --include="*.c" .
   ```
5. **Replace each use:**
   - `IS_SET_OLD(var, CONST)` → `IS_SET_BITS(var, CONST, table)`
   - `SET_BIT_OLD(var, CONST)` → `SET_BIT_BITS(var, CONST, table)`
   - `REMOVE_BIT_OLD(var, CONST)` → `REMOVE_BIT_BITS(var, CONST, table)`
6. **Compile and fix errors**
7. **Test**
8. **Commit**

---

## Save/Load Considerations

The _BITS macros work with integer storage, so existing save files will continue to work as-is. The flag values are still stored as numbers.

If you want human-readable save files later, you can update save/load to use:
```c
// Save:
fprintf(fp, "ExitFlags %s~\n", print_flags_from_bits(pexit->exit_info, exit_flags));

// Load:
pexit->exit_info = fread_flag_bits(fp, exit_flags);
```

But this is optional and can be done separately.

---

## Cleanup After All Phases

1. **Remove old macros** (or rename for clarity):
   ```c
   // Keep only if needed for truly internal/binary flags:
   #define IS_SET_RAW(var, bit)      ((var) & (bit))
   #define SET_BIT_RAW(var, bit)     ((var) |= (bit))
   #define REMOVE_BIT_RAW(var, bit)  ((var) &= ~(bit))
   ```

2. **Delete backup files:**
   ```bash
   rm -f *.bak* editors/*/*.backup
   ```

3. **Final commit**

---

## Time Estimates

| Phase | Uses | Time |
|-------|------|------|
| Exit flags | 130 | 2-3 hours |
| Container flags | 60 | 1-1.5 hours |
| Gate flags | 40 | 1 hour |
| Lock flags | 45 | 1 hour |
| Token flags | 15 | 30 min |
| Ban flags | 15 | 30 min |
| Shop flags | 8 | 20 min |
| Instance flags | 8 | 20 min |
| Channel display | 5 | 15 min |
| Cleanup | - | 30 min |
| **Total** | ~400 | **8-10 hours** |

---

## Validation Commands

After each phase:

```bash
# Compile
make clean && make

# Check no old-style uses remain for that category
grep -rn "IS_SET_OLD.*EX_" --include="*.c" .   # Should be empty after Phase 1
grep -rn "IS_SET_OLD.*CONT_" --include="*.c" . # Should be empty after Phase 2
# etc.

# Final check - should only show remaining categories
grep -rn "IS_SET_OLD\|SET_BIT_OLD" --include="*.c" . | wc -l
```

---

## Notes

- Each phase is independent - you can do one per session
- The _BITS macros already exist and are tested
- No structure changes needed
- Existing save files continue to work
- Human-readable save files are optional enhancement for later
