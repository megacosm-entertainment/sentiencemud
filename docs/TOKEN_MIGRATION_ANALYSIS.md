# Token System Migration: 2.0 → 1.0 Analysis

**Date**: 2026-01-06
**Purpose**: Identify specific token system enhancements in 2.0 that should be ported to 1.0

---

## Key Git Commits in 2.0 Related to Tokens

Based on git history analysis, here are the major token-related improvements in 2.0:

### 1. **Merged Skills and Spells List** (commit e78dd35 - Sep 24, 2016)
**Impact**: HIGH - Major architectural change

**Changes**:
- Merged `sorted_skills` and `sorted_spells` into single `sorted_skills` list
- Added `isspell` boolean to `SKILL_ENTRY` to distinguish spells from skills
- Added `scripted` boolean to track script-granted skills
- Moved token mana cost from SPELL trigger to token value: `TOKVAL_SPELL_MANA` (value[4])
- Added `TOKVAL_SPELL_LEARN` (value[5]) for learning cost
- Token-based skills/spells can now be practiced at trainers
- Added PREPRACTICETOKEN and PRETRAINTOKEN triggers
- Changed token ratings to be "points per percent" rather than total cost

**Status in 1.0**: ✅ MOSTLY DONE
- `sorted_spells` is commented out in merc.h line 4810
- Need to verify function implementations match

---

### 2. **Token-Affect Coupling** (commit 7dda69f - Feb 4, 2024)
**Impact**: HIGH - Enables affects to track their source tokens

**Changes**:
- `AFFECT_DATA` gained `TOKEN_DATA *token` field
- `TOKEN_DATA` gained `LLIST *affects` field (list of affects created by this token)
- When affect token is removed, all associated affects are removed
- Bidirectional relationship: affects know their token, tokens track their affects

**Status in 1.0**: ❌ MISSING
- `AFFECT_DATA` structure lacks `token` pointer
- `TOKEN_DATA` structure lacks `affects` list
- No coupling mechanism exists

**Required Changes**:
```c
// In AFFECT_DATA (merc.h ~line 2297):
struct affect_data {
    // ... existing fields ...
    TOKEN_DATA *token;    // Source token for this affect
};

// In TOKEN_DATA (merc.h ~line 4241):
struct token_data {
    // ... existing fields ...
    LLIST *affects;      // List of affects created by this token
};
```

---

### 3. **Script Command Consolidation** (commit bbb1805 - Nov 11, 2016)
**Impact**: MEDIUM - Improves script interface for token skills

**Changes**:
- Added GRANTSKILL command: `GRANTSKILL player name|vnum [rating[ permanent[ flags]]]`
- Added REVOKESKILL command: `REVOKESKILL player name|vnum`
- Consolidated AWARD commands: `AWARD mobile|church type amount`
- Consolidated DEDUCT commands: `DEDUCT mobile|church type amount`
- Token skills can be granted/revoked dynamically via scripts

**Status in 1.0**: UNKNOWN - Need to check script command implementations

---

### 4. **Token Triggers** (various commits)
**Impact**: MEDIUM - Expands token scriptability

**New Triggers Added**:
- `TRIG_PRACTICE` - Fires when practicing a skill
- `TRIG_PRACTICETOKEN` - Fires when practicing a token skill
- `TRIG_PREPRACTICETOKEN` - Pre-practice validation for token skills
- `TRIG_PRETRAINTOKEN` - Pre-train validation for token skills
- `TRIG_TOKEN_GIVEN` (commit b4de833) - Fires when token is given to entity
- `TRIG_TOKEN_REMOVED` (commit ba34817) - Fires when token is removed/purged

**Status in 1.0**: UNKNOWN - Need to check trigger_table definitions

---

### 5. **Token Variable Support** (commit adcaa01)
**Impact**: LOW - Quality of life

**Changes**:
- Added variable types for TOKEN_INDEX types
- Improved script variable handling for tokens

**Status in 1.0**: Need to verify

---

### 6. **Garbage Collection** (commit 84d4ac2)
**Impact**: MEDIUM - Memory management

**Changes**:
- Added basic garbage collection for tokens
- `TOKEN_DATA` gained `gc` boolean field

**Status in 1.0**: ✅ PRESENT
- Line 4244 in merc.h shows `bool gc;` field exists

---

### 7. **Token Room/Object Support** (commits 26f05db, b0f09a3, e2a29c2)
**Impact**: LOW - Bug fixes

**Changes**:
- Fixed token support for rooms and objects
- "Did you know tokens could go on objects and rooms too?"

**Status in 1.0**: ✅ PRESENT
- TOKEN_DATA has `object` and `room` fields

---

### 8. **MXP Support** (commit 06e3b44)
**Impact**: LOW - UI enhancement

**Changes**:
- Added MXP support for stat pages
- ID lookup for `stat token` command

**Status in 1.0**: UNKNOWN - 1.0 has protocol.c which may have this

---

## Priority Migration List

### CRITICAL (Must Port):

1. **Token-Affect Coupling**
   - Add `TOKEN_DATA *token` to `AFFECT_DATA`
   - Add `LLIST *affects` to `TOKEN_DATA`
   - Update `affect_to_char()` to link affect to token
   - Update `affect_remove()` to unlink from token
   - Update `extract_token()` to remove all associated affects
   - Files: merc.h, handler.c, db.c, save.c

2. **Verify Merged Skills/Spells Implementation**
   - Check if skill_entry functions properly handle isspell flag
   - Verify token mana/learn cost values are used
   - Ensure PREPRACTICETOKEN/PRETRAINTOKEN triggers exist
   - Files: skills.c, magic.c, act_wiz.c

### HIGH (Should Port):

3. **Script Commands for Token Skills**
   - Implement GRANTSKILL command
   - Implement REVOKESKILL command
   - Files: script_commands.c, script_mpcmds.c

4. **Token Event Triggers**
   - Add TRIG_TOKEN_GIVEN trigger
   - Add TRIG_TOKEN_REMOVED trigger
   - Fire on token_to_char/token_from_char
   - Files: scripts.h, tables.c, handler.c

### MEDIUM (Nice to Have):

5. **Practice/Train Triggers**
   - TRIG_PRACTICE
   - TRIG_PRACTICETOKEN
   - Verify these are properly called in skills.c

### LOW (Optional):

6. **MXP Enhancements**
   - Already may exist in 1.0's protocol system
   - Low priority

---

## Implementation Plan

### Phase 1: Token-Affect Coupling (Est: 2-3 hours)

**Step 1: Update Structures**
- [ ] Add `TOKEN_DATA *token` to AFFECT_DATA in merc.h
- [ ] Add `LLIST *affects` to TOKEN_DATA in merc.h
- [ ] Update mem.c to initialize new fields (new_affect, new_token)

**Step 2: Update Affect Management**
- [ ] Modify `affect_to_char()` in handler.c to link affect to token
- [ ] Modify `affect_to_obj()` to link affect to token
- [ ] Modify `affect_remove()` to unlink from token's affects list
- [ ] Modify `affect_strip()` to handle token cleanup

**Step 3: Update Token Management**
- [ ] Modify `extract_token()` in handler.c to remove all affects in token->affects
- [ ] Modify `token_from_char()` to clean up affects
- [ ] Modify `token_from_obj()` to clean up affects

**Step 4: Update Persistence**
- [ ] Update `fwrite_char()` in save.c to save affect->token reference
- [ ] Update `fread_char()` in save.c to restore affect->token linkage
- [ ] May need to save token ID and restore pointers on load

**Step 5: Testing**
- [ ] Create test token with AFFECT type
- [ ] Grant token to character
- [ ] Verify affects are applied
- [ ] Remove token, verify affects are removed
- [ ] Save/load character, verify affects persist correctly

---

### Phase 2: Verify Skills/Spells Merge (Est: 1-2 hours)

**Verification Checklist**:
- [ ] Check SKILL_ENTRY has `isspell` field (line 540 in merc.h) ✅ CONFIRMED
- [ ] Check `sorted_spells` is commented out ✅ CONFIRMED
- [ ] Verify `skill_entry_addspell()` adds to sorted_skills with isspell=true
- [ ] Verify `skill_entry_rating()` handles token skills correctly
- [ ] Verify `skill_entry_mana()` exists and checks TOKVAL_SPELL_MANA
- [ ] Verify `skill_entry_learn()` exists and checks TOKVAL_SPELL_LEARN
- [ ] Check token value indices are defined (TOKVAL_SPELL_MANA, TOKVAL_SPELL_LEARN)

**If Missing**:
- Port skill_entry functions from 2.0
- Update magic.c to use skill_entry_mana() for token spells
- Update skills.c practice/train code to respect token learn costs

---

### Phase 3: Script Commands (Est: 1-2 hours)

**Implementation**:
- [ ] Add GRANTSKILL to script_commands.c
- [ ] Add REVOKESKILL to script_commands.c
- [ ] Update script command tables
- [ ] Test granting/revoking token skills via scripts

---

### Phase 4: Token Event Triggers (Est: 1 hour)

**Implementation**:
- [ ] Add TRIG_TOKEN_GIVEN to trigger enum (merc.h)
- [ ] Add TRIG_TOKEN_REMOVED to trigger enum
- [ ] Add to trigger_table in tables.c
- [ ] Call from token_to_char() in handler.c
- [ ] Call from token_from_char() / extract_token()
- [ ] Test triggers fire correctly

---

## Files Requiring Changes

| File | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
|------|---------|---------|---------|---------|
| merc.h | ✓ | ✓ | ✓ | ✓ |
| handler.c | ✓ | | | ✓ |
| mem.c | ✓ | | | |
| save.c | ✓ | | | |
| db.c | ✓ | | | |
| skills.c | | ✓ | | |
| magic.c | | ✓ | | |
| script_commands.c | | | ✓ | |
| tables.c | | | | ✓ |

---

## Testing Strategy

### Test 1: Token-Affect Coupling
```
1. Create affect token: tedit create 99001, set type affect
2. Add token trigger to apply affect when given
3. Give token to character
4. Verify affect applied (affects command)
5. Remove token
6. Verify affect removed
7. Save/quit character
8. Reload character
9. Verify affect persisted correctly
```

### Test 2: Token Skill Practice
```
1. Create skill token: tedit create 99002, set type skill
2. Set learning cost (value 5) to 100
3. Grant skill to character
4. Try to practice at trainer
5. Verify can practice if have enough trains
6. Verify skill rating increases
```

### Test 3: Script Grant/Revoke
```
1. Create mob with script containing GRANTSKILL
2. Trigger script
3. Verify character gains skill
4. Trigger REVOKESKILL
5. Verify skill removed
```

---

## Risk Assessment

### Low Risk:
- Token-affect coupling (well-isolated change)
- Script commands (additive, doesn't break existing)

### Medium Risk:
- Skills/spells merge verification (core system, needs thorough testing)

### High Risk:
- Save file format changes (if affect->token requires new persistence format)
- May need pfile version bump and migration

---

## Rollback Plan

- All changes are in git branch
- Keep backups of merc.h, handler.c, save.c
- If save format changes, document old format for recovery
- Test with disposable test characters first

---

## Success Criteria

✅ Token-affect coupling works (affects removed when token removed)
✅ Token affects persist through save/load
✅ Token skills can be practiced/trained
✅ Script commands GRANTSKILL/REVOKESKILL functional
✅ Token event triggers fire correctly
✅ No regressions in existing token functionality
✅ All tests pass

---

**End of Analysis**
