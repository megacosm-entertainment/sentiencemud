# Flag System Migration Status

## Completed Work

### Phase 1: Core Flagset System ✅
- Created flagset.c/flagset.h with hybrid storage (strings + bit cache)
- Implemented all core API functions
- Added backward compatibility helpers (bitvector conversion)
- Debug and validation functions (flagset_debug_print, flagset_validate)

### Phase 2: Flag Table Modernization ✅
- Updated flag_type structure (removed bit field, added description)
- Merged 273 flags from 11 tables into 5 unified tables
- Fixed all table ordering issues to match letter sequences
- Added NULL entries for gaps in letter sequences

### Phase 3: Structure Definitions ✅
- CHAR_DATA: All 16 flag fields converted to flagset_t
- OBJ_DATA: extra_flags, wear_flags converted
- MOB_INDEX_DATA: All 8 flag fields converted
- OBJ_INDEX_DATA: extra_flags, wear_flags converted
- ROOM_INDEX_DATA: room_flags, rs_room_flags converted
- AFFECT_DATA: bitvector → flagset_t
- RACE_DATA: All flag fields converted

### Phase 4: Save/Load System ✅
- fwrite_char(): Saves flags as space-separated strings
- fread_char(): Reads both old (bitvector) and new (string) formats
- Automatic conversion on load (backward compatible)
- All character flags updated (act/plr, affected_by, imm/res/vuln, comm, wiznet, lostparts)

### Phase 5: Memory Management ✅
- new_char()/free_char(): Initialize/free all 16 character flagsets
- new_obj()/free_obj(): Initialize/free object flagsets
- new_mob_index()/free_mob_index(): Initialize/free MOB_INDEX flagsets
- new_obj_index()/free_obj_index(): Initialize/free OBJ_INDEX flagsets
- new_room_index()/free_room_index(): Initialize/free room flagsets

### Phase 6: Flag Constant Conversions ✅ (400/410 = 98%)

**Completed conversions:**
- ✅ ACT flags (61) - NPC behaviors
- ✅ AFF flags (54) - Character affects
- ✅ ROOM flags (57) - Room properties (31 room + 26 room2)
- ✅ EXTRA flags (70) - Object attributes (29 + 30 + 11 from extra/extra2/extra3)
- ✅ FORM flags (26) - Body forms
- ✅ WEAR flags (26) - Equipment slots
- ✅ PART flags (24) - Body parts (18 + 6 combat parts)
- ✅ OFF flags (24) - Offensive abilities
- ✅ IMM flags (29) - Damage immunities (A-T, U, X-Z, aa-ee)
- ✅ RES flags (29) - Damage resistances (shares imm_flags table)
- ✅ VULN flags (29) - Damage vulnerabilities (shares imm_flags table)
- ✅ COMM flags (29) - Communication preferences (A-W, Y-Z, aa, cc-ee)

**All flag tables fixed and reordered:**
- room_flags: Reordered to letter sequence (A)-(ee) + (A)-(bb)
- extra_flags: Reordered and added missing flags (shop_bought, noquest, trapped, unseen)
- part_flags: Reordered with NULL placeholders for gaps (S-T)
- off_flags: Reordered and added missing flags (bite, breath), NULL for gaps (H, V)
- imm_flags: Reordered and added missing flag (bite), NULL for gaps (V-W)
- comm_flags: Completely rebuilt in letter order, added 7 missing flags

### Phase 6: Critical Macros ✅
- IS_NPC: Checks both act_flags and plr_flags
- IS_AFFECTED: Uses flagset_isset()
- IS_UNDEAD: Checks act_flags and form_flags
- IS_OBJCASTER, IS_INVASION_LEADER, IS_PK, IS_PIRATE
- IS_SOCIAL, IS_SAFE, IS_OUTSIDE
- ACT_IS_NPC, ACT_UNDEAD, ACT2_INVASION_LEADER, ACT2_PIRATE
- PLR_PK, FORM_UNDEAD, ROOM_SAFE, ROOM_INDOORS, AREA_SOCIAL

### Conversion Script Enhancements ✅
- Handles gaps in letter sequences (NULL entries)
- Supports label comments (`/* for combat */`, `/* actual form */`)
- Handles merged flag tables with offset parameter
- Handles out-of-order flags (fixed IMM_AIR, RES_AIR, VULN_AIR positioning)
- Proper section boundary detection
- Already-converted flag tracking

## Remaining Work

### Phase 6: PLR Flag Constants (~10 remaining)
**Status**: Most PLR flags already converted, ~10 remain

**Note**: PLR table structure is complex with two sections (plr + plr2), some gaps.
Next action: Verify which PLR flags still need conversion and complete them.

### Phase 6: Code Usage Updates (Not Started)

After constants are converted, need to update all code that uses flags:

**High Priority Files (~20 files)**:
- handler.c: affect_modify(), affect_to_char(), etc. (~100 changes)
- db.c: load_mobiles(), load_objects(), load_rooms() (~60 changes)
- act_*.c files: Command implementations (~40 changes each)
- magic*.c files: Spell implementations (~30 changes each)
- fight.c: Combat flag checks (~40 changes)

**Medium Priority Files (~30 files)**:
- update.c, comm.c, interp.c, nanny.c
- OLC editors: medit.c, oedit.c, redit.c, aedit.c
- JSON: json_char.c, json_obj.c
- Scripts: script_*.c files

**Low Priority Files (~50 files)**:
- Remaining act_*.c, skill_*.c, spell_*.c files
- Utility files

**Estimated Changes**: ~3000-5000 individual flag references

**Key Changes Required**:
1. Replace array indexing: `ch->act[0]` → `ch->act_flags`
2. Replace IS_SET calls: `IS_SET(ch->affected_by[0], AFF_BLIND)` → `flagset_isset(&ch->affected_by, "blind", affect_flags)`
3. Replace SET_BIT calls: `SET_BIT(ch->act[0], ACT_SENTINEL)` → `flagset_set(&ch->act_flags, "sentinel", act_flags)`
4. Replace REMOVE_BIT calls: `REMOVE_BIT(ch->affected_by[1], AFF2_SILENCE)` → `flagset_remove(&ch->affected_by, "silence", affect_flags)`
5. Update flag constant usage to use new string constants

### Phase 7: Cleanup and Testing (Not Started)

- Remove old bitvector conversion functions (if not needed for backward compat)
- Remove flagbank arrays from tables.c
- Remove letter-based constants (A), (B), (C), etc. from merc.h
- Create wiznet_flags table (currently using comm_flags as placeholder)
- Comprehensive testing:
  - Character creation/save/load
  - Spell affects
  - Combat
  - OLC editors
  - Scripts
  - Backward compatibility with old save files

## Current Status

**Code Compiles**: NO
- Structure fields changed but ~3000-5000 usage sites still reference old syntax
- Example: `ch->act[0]` needs to become `ch->act_flags`
- Example: `IS_SET(ch->affected_by[0], AFF_BLIND)` needs updating

**Migration Progress**: 98% complete (flag constants)
- Core infrastructure: 100%
- Flag constants: 98% (400/410 converted)
- Code usage: 0%

**Overall Progress**: ~45% complete
- Infrastructure complete
- Most flag constants converted
- Code migration is the major remaining task

## Next Steps

1. ✅ Complete PLR flag constant conversions (~10 remaining)
2. Begin systematic code updates starting with handler.c and db.c
3. Update OLC editors and JSON serialization
4. Update all command and spell implementations
5. Comprehensive testing phase
6. Performance benchmarking
7. Documentation updates

## Tools

**convert_flags.py**: Automated flag constant converter
- Successfully converted 400/410 flag constants
- Handles complex table layouts (gaps, merged tables, out-of-order flags)
- Provides detailed conversion output

## Technical Notes

### Table Structure
All flag tables now follow consistent letter-order structure:
- Single letters (A)-(Z) = positions 0-25
- Double letters (aa)-(ee) = positions 26-51
- NULL entries for unused positions
- Comments indicating letter positions

### Backward Compatibility
- Old save files auto-convert on load via `flagset_from_bitvector()`
- New save format uses human-readable strings
- Flagset system provides O(1) runtime checks via bit cache
- No arbitrary flag limits (can grow indefinitely)

### Memory Optimization
- String interning could be added if memory usage becomes concern
- Current implementation uses dynamic arrays
- Bit cache provides fast lookups without memory overhead

### Future Enhancements
- Resistance system will replace IMM/RES/VULN with percentage-based damage adjustment
- Flag reorganization for config settings planned after migration complete
