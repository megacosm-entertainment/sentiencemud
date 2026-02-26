# Code Migration Patterns

## Overview

This document provides patterns for migrating code from the old bitvector flag system to the new flagset system.

## Common Patterns

### Pattern 1: Array Access → Flagset Field

**Old:**
```c
ch->act[0]
ch->affected_by[1]
obj->extra[2]
```

**New:**
```c
ch->act_flags          // For NPCs
ch->plr_flags          // For PCs
ch->affected_by        // No array index
obj->extra_flags       // No array index
```

### Pattern 2: IS_SET Macro Calls

**Old:**
```c
IS_SET(ch->act[0], ACT_SENTINEL)
IS_SET(ch->affected_by[0], AFF_BLIND)
IS_SET(ch->affected_by[1], AFF2_SILENCE)
IS_SET(obj->extra[0], ITEM_GLOW)
```

**New:**
```c
flagset_isset(&ch->act_flags, "sentinel", act_flags)
flagset_isset(&ch->affected_by, "blind", affect_flags)
flagset_isset(&ch->affected_by, "silence", affect_flags)  // No AFF2 distinction
flagset_isset(&obj->extra_flags, "glow", extra_flags)
```

**Or use helper macros (if defined):**
```c
IS_ACT(ch, "sentinel")
IS_AFFECTED(ch, "blind")
```

### Pattern 3: SET_BIT Macro Calls

**Old:**
```c
SET_BIT(ch->act[0], ACT_AGGRESSIVE)
SET_BIT(ch->affected_by[1], AFF2_HASTE)
SET_BIT(obj->extra[0], ITEM_MAGIC)
```

**New:**
```c
flagset_set(&ch->act_flags, "aggressive", act_flags)
flagset_set(&ch->affected_by, "haste", affect_flags)
flagset_set(&obj->extra_flags, "magic", extra_flags)
```

### Pattern 4: REMOVE_BIT Macro Calls

**Old:**
```c
REMOVE_BIT(ch->act[0], ACT_AGGRESSIVE)
REMOVE_BIT(ch->affected_by[0], AFF_BLIND)
REMOVE_BIT(obj->extra[1], ITEM_INVIS)
```

**New:**
```c
flagset_remove(&ch->act_flags, "aggressive", act_flags)
flagset_remove(&ch->affected_by, "blind", affect_flags)
flagset_remove(&obj->extra_flags, "invis", extra_flags)
```

### Pattern 5: TOGGLE_BIT Macro Calls

**Old:**
```c
TOGGLE_BIT(ch->act[0], ACT_SENTINEL)
```

**New:**
```c
flagset_toggle(&ch->act_flags, "sentinel", act_flags)
```

### Pattern 6: Bitwise OR Combinations

**Old:**
```c
// In tables.c or initialization code
PART_HIDE|PART_GUTS|PART_EYE|PART_EAR|PART_BRAINS
ACT_SENTINEL|ACT_AGGRESSIVE|ACT_STAY_AREA
ITEM_GLOW|ITEM_MAGIC|ITEM_NODROP
```

**New - Option A (Multiple set calls):**
```c
flagset_t parts;
flagset_init(&parts);
flagset_set(&parts, "hide", part_flags);
flagset_set(&parts, "guts", part_flags);
flagset_set(&parts, "eye", part_flags);
flagset_set(&parts, "ear", part_flags);
flagset_set(&parts, "brains", part_flags);
```

**New - Option B (String list):**
```c
flagset_t parts;
flagset_from_string(&parts, "hide guts eye ear brains", part_flags);
```

### Pattern 7: Direct Assignment

**Old:**
```c
ch->act[0] = ACT_SENTINEL | ACT_AGGRESSIVE;
ch->affected_by[0] = 0;  // Clear all
```

**New:**
```c
flagset_clear(&ch->act_flags);
flagset_set(&ch->act_flags, "sentinel", act_flags);
flagset_set(&ch->act_flags, "aggressive", act_flags);

// Or using string:
flagset_from_string(&ch->act_flags, "sentinel aggressive", act_flags);

// Clear all:
flagset_clear(&ch->affected_by);
```

### Pattern 8: Checking Multiple Flags

**Old:**
```c
if (IS_SET(ch->act[0], ACT_SENTINEL) && IS_SET(ch->act[0], ACT_AGGRESSIVE))
```

**New:**
```c
if (flagset_isset(&ch->act_flags, "sentinel", act_flags) &&
    flagset_isset(&ch->act_flags, "aggressive", act_flags))
```

### Pattern 9: NPC vs PC Flag Handling

**Old:**
```c
if (IS_NPC(ch))
    SET_BIT(ch->act[0], ACT_SENTINEL);
else
    SET_BIT(ch->act[0], PLR_AUTOEXIT);
```

**New:**
```c
if (IS_NPC(ch))
    flagset_set(&ch->act_flags, "sentinel", act_flags);
else
    flagset_set(&ch->plr_flags, "autoexit", plr_flags);
```

### Pattern 10: Flag Copying

**Old:**
```c
ch->affected_by[0] = victim->affected_by[0];
ch->affected_by[1] = victim->affected_by[1];
```

**New:**
```c
flagset_copy(&ch->affected_by, &victim->affected_by);
```

## Special Cases

### Affect Structures

**Old:**
```c
paf->bitvector = AFF_BLIND;
paf->bitvector2 = AFF2_SILENCE;
```

**New:**
```c
flagset_init(&paf->bitvector);
flagset_set(&paf->bitvector, "blind", affect_flags);
// AFF2 flags now merged into same flagset:
flagset_set(&paf->bitvector, "silence", affect_flags);
```

### MOB/OBJ Index Data

**Old:**
```c
pMobIndex->act[0] = ACT_IS_NPC | ACT_SENTINEL;
pObjIndex->extra[0] = ITEM_GLOW;
```

**New:**
```c
flagset_init(&pMobIndex->act_flags);
flagset_set(&pMobIndex->act_flags, "is_npc", act_flags);
flagset_set(&pMobIndex->act_flags, "sentinel", act_flags);

flagset_init(&pObjIndex->extra_flags);
flagset_set(&pObjIndex->extra_flags, "glow", extra_flags);
```

### Room Flags

**Old:**
```c
pRoom->room_flag[0] = ROOM_DARK | ROOM_INDOORS;
pRoom->room_flag[1] = ROOM2_NO_RECALL;
```

**New:**
```c
flagset_init(&pRoom->room_flags);
flagset_set(&pRoom->room_flags, "dark", room_flags);
flagset_set(&pRoom->room_flags, "indoors", room_flags);
flagset_set(&pRoom->room_flags, "no_recall", room_flags);  // ROOM2 merged
```

## Tables and Static Data

### Rawkill Table Example

**Old (tables.c line 2450):**
```c
PART_HIDE|PART_GUTS|PART_EYE|PART_EAR|PART_BRAINS|PART_HEART|PART_LONG_TONGUE|PART_EYESTALKS|PART_TENTACLES|PART_SCALES
```

**New - Option A (Initialization function):**
```c
// In the rawkill structure, change field type from long to flagset_t
// Then initialize in an init function:
void init_rawkill_table(void) {
    flagset_from_string(&rawkill_table[RAWKILL_BEHEAD].lostparts,
        "hide guts eye ear brains heart long_tongue eyestalks tentacles scales",
        part_flags);
}
```

**New - Option B (Keep as string in table):**
```c
// Change structure field from long to char *
"hide guts eye ear brains heart long_tongue eyestalks tentacles scales"
// Parse when needed
```

## Priority Order for Migration

1. **High Priority** - Core game loop files:
   - handler.c (affect system)
   - db.c (loading/saving)
   - fight.c (combat)
   - update.c (game tick)

2. **Medium Priority** - Command and spell files:
   - act_*.c (player commands)
   - magic*.c (spell implementations)
   - OLC editors (medit, oedit, redit)

3. **Low Priority** - Utility and less-used files:
   - Specialized commands
   - Helper functions
   - Scripts

## Testing Strategy

After migrating each file:
1. Compile and fix any errors
2. Test basic functionality (login, movement, combat)
3. Test specific features changed in that file
4. Check for memory leaks with valgrind
5. Verify save/load still works

## Helper Macros to Define

Consider defining these in merc.h to simplify migrations:

```c
#define IS_ACT(ch, flag)     (IS_NPC(ch) ? flagset_isset(&(ch)->act_flags, (flag), act_flags) : false)
#define IS_PLR(ch, flag)     (!IS_NPC(ch) ? flagset_isset(&(ch)->plr_flags, (flag), plr_flags) : false)
#define SET_ACT(ch, flag)    flagset_set(&(ch)->act_flags, (flag), act_flags)
#define SET_PLR(ch, flag)    flagset_set(&(ch)->plr_flags, (flag), plr_flags)
#define REMOVE_ACT(ch, flag) flagset_remove(&(ch)->act_flags, (flag), act_flags)
#define REMOVE_PLR(ch, flag) flagset_remove(&(ch)->plr_flags, (flag), plr_flags)
```

## Common Pitfalls

1. **Forgetting to check IS_NPC** when accessing act_flags vs plr_flags
2. **Using wrong table** (e.g., affect_flags vs act_flags)
3. **Not initializing flagsets** before use
4. **Memory leaks** from not freeing flagsets
5. **Array indices** - Remember there are no [0]/[1] anymore
6. **AFF2 flags** - They're now in the same affect_flags table, not separate

## Performance Notes

- `flagset_isset()` is O(log n) for string lookup, but O(1) for bit cache check
- Consider caching frequently-checked flags if performance critical
- Bit cache is automatically maintained by the flagset system
- No performance regression expected for IS_SET-style checks
