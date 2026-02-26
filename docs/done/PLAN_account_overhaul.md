# Account System Overhaul Plan

**Created:** February 11, 2026  
**Updated:** February 11, 2026  
**Status:** Phase 1 Complete  
**Scope:** Account management, OTP, menus, penalties/bonuses/notes, unlocks, dead code cleanup

---

## Current State Assessment

### File Inventory

| File | Lines | Purpose |
|------|-------|---------|
| `nanny.c` | 5,899 | Main login state machine (dispatchers + flows) |
| `nanny/nanny_menus.c` | 883 | All menu handler implementations |
| `nanny/nanny_menus.h` | 112 | Menu handler declarations |
| `nanny/nanny_auth.c` | 344 | Consolidated auth handlers |
| `nanny/nanny_utils.c` | 434 | Utility functions for nanny system |
| `nanny/nanny_utils.h` | 61 | Utility declarations |
| `nanny/nanny_states.h` | 197 | State definitions + consolidation notes |
| `nanny/nanny.h` | 142 | Declarations for all nanny functions |
| `account/auth.c` | 634 | Unified auth API |
| `account/auth_sodium.c` | 376 | Argon2id password hashing |
| `account/auth_migrate.c` | 127 | Password migration helpers |
| `account/otp.c` | 797 | OTP/TOTP/QR code generation |
| `account/account_notes.c` | 393 | Staff notes on accounts |
| `io/json/json_account.c` | 946 | JSON serialization for accounts |
| `io/json/json_account.h` | 62 | JSON account declarations |

**Total:** ~11,407 lines across 15 files

### Current Architecture

```
Connection → login_get_account → login_get_account_password
                                         ↓
                                 CON_ACCOUNT_MENU
                                 (display_account_menu / login_account_menu)
                                         ↓
                       ┌─────────────────┼──────────────────┐
                       ↓                 ↓                  ↓
              Create Character    Select Character    Account Settings
              (CON_CREATING_*)    (select_character)  (Email/Password/MFA)
                                         ↓
                                 CON_CHARACTER_MENU
                                 (display_character_menu / login_character_menu)
                                         ↓
                       ┌─────────────────┼──────────────────┐
                       ↓                 ↓                  ↓
                  Enter Game       Char Settings       Delete/Unlink
                 (proceed_to_game) (Password/MFA/Email) (Multi-step flows)
```

---

## Problems Identified

### 1. Dead Code & Duplication

#### `login_account_menu()` duplicates `nanny_menus.c` handlers — ✅ RESOLVED (see below)
> Resolved: Full rewrite completed in Phase 1. See password validation entry below.

#### `do_keygen` references `ch->pcdata->mfa_key` directly — ✅ RESOLVED
> Resolved by removing `do_keygen` entirely. MFA management is now exclusively
> through account/character menus. `mfa_question` system also removed.

#### `generate_key()` is a compatibility shim — ✅ RESOLVED
> Removed along with `do_keygen`. No callers remained.

#### Duplicate password validation — ✅ RESOLVED
> Removed `validate_password_strength_basic()` from `nanny_utils.c`. All callers now
> use `validate_password_strength()` from `auth.c`, which enforces letters+digits.

#### `login_account_menu()` duplicates `nanny_menus.c` handlers — ✅ RESOLVED
> Full rewrite of `nanny_menus.c` with production-ready handlers. Both
> `login_account_menu()` and `login_character_menu()` now dispatch to handler functions.

#### `update_password()`, `update_mfa_key()`, `update_email()` are stubs — ❌ DOES NOT EXIST
> Investigation showed these functions do not exist in auth.c. The plan was incorrect.

### 2. Navigation Issues (No Back-Out)

#### Email verification flow has no cancel option
`login_verify_account_email_change()` (nanny.c:579-621) prompts "Invalid code. Try again:" with no way to cancel or return to the account menu. Player is stuck in a loop until they enter the correct code or the code expires (72 hours).

**Fix:** Accept "cancel" or empty input to return to account menu.

#### Account password change flow has no cancel
`login_change_account_password()` (nanny.c:4360-4400) and `login_confirm_account_password_change()` (nanny.c:4401-4447) have no cancel option. Empty input re-prompts silently.

**Fix:** Accept "cancel" to return to account menu.

#### Character email change flow has no cancel
`login_change_character_email()` (nanny.c:5517-5610) doesn't accept cancel to return to character menu.

**Fix:** Accept "cancel" to return to character menu.

#### Link character password has no cancel
`login_link_character_password()` (nanny.c:2869-2917) - if an incorrect password is entered, the character is freed and user returns to account menu, but there's no way to cancel before entering a password.

**Fix:** Accept empty input or "cancel" to return to account menu.

#### Unlink password/MFA flows have no cancel
`login_verify_unlink_password()` and `login_verify_unlink_mfa()` (nanny.c:3776-3833) return to character menu on failure but don't offer an explicit cancel.

**Fix:** Accept "cancel" to return to character menu.

#### Character MFA setup has no cancel
Character MFA setup flows don't allow the user to back out once started.

**Fix:** Add cancel option to all MFA setup prompts.

### 3. Missing Features

#### Penalties system (commented out in `account_data`)
`PENALTY_DATA *penalties` and `BONUS_DATA *bonuses` are commented out in `merc.h:5054-5055`. No structures or code exist for these types.

#### Bonus system (commented out in `account_data`)
Same as above - no implementation exists.

#### Account-level notes system is staff-only
`do_accnote` exists for staff to annotate accounts, but there's no player-visible note system at the account level.

#### Character-level notes/penalties/bonuses
No systems exist for annotating characters with staff notes, penalties, or bonuses at the account-character level.

#### Account unlocks system
`avail_races` field exists on `ACCOUNT_DATA` (merc.h:5060) but is never populated or used. There's an `unlocked_areas` system on `PC_DATA` (merc.h:5247) for area unlocks, but no corresponding system for race/class unlocks tied to accounts.

#### Race availability during character creation
`login_get_new_race()` (nanny.c:2241) doesn't check account-level race unlocks. All races are shown if they meet level/remort requirements at the character level.

### 4. Structural Issues

#### The Y/Z direct-login code is duplicated — ✅ RESOLVED
> Extracted `direct_login_character()` in `nanny_utils.c`. Y, Z, and numeric
> selection handlers now call this helper.

#### Character sorting is duplicated 3 times — ✅ RESOLVED
> Extracted `sort_account_characters()` in `nanny_utils.c`. Replaces 4 inline
> bubble-sort blocks.

#### `login_account_menu()` is 446 lines
Despite the extraction of handlers in `nanny_menus.c`, the original function wasn't refactored to use them.

---

## Phased Implementation Plan

### Phase 0: JSON Completeness & Old Format Deprecation ✅ COMPLETE
**Priority: CRITICAL | Estimated: 2-3 sessions | Risk: High**
**Prerequisite for all other phases. Must be completed first.**

> **Completed:** All 46 missing fields added to JSON serializers. `played` bug fixed.
> `imm_flag` read gap fixed. See WORKLOG_account_overhaul.md for details.

The old pfile format (fprintf/fread in `save.c`) is being replaced by JSON (`json_account.c`, `json_char.c`). Currently `load_account()` auto-detects format and `save_account()` always writes JSON. However, the JSON serializers are missing fields that the old format saves. Any account/character that has been re-saved in JSON has already lost these fields. We need to add the missing fields to JSON, then deprecate the old format code.

#### Step 0a: Fix the `played` bug (CRITICAL)

The `played_hours` field in `json_char.c` has a unit mismatch bug that silently corrupts playtime data on every save/load cycle:

- **Write** (line 957): `ch->played + (int)(current_time - ch->logon) / 3600` → saves HOURS
- **Read** (line 2031): `ch->played = json_integer_value(...)` → loads into field that expects SECONDS
- **Old format**: Saves raw seconds, reads raw seconds (correct)

Each save/load cycle, playtime is divided by 3600 more. A player with 100 hours played would show ~0 hours after two saves.

**Fix:** Change the JSON write to save raw seconds (drop `/ 3600`), and rename the JSON key from `played_hours` to `played` (with backward-compatible read of both keys). Add a migration path: if `played_hours` is found, multiply by 3600 to recover seconds.

#### Step 0b: Add missing account-level fields to JSON (6 fields)

Fields saved in old format but missing from `json_account.c`:

| # | Struct Member | Old Key | JSON Key to Add | Severity | Notes |
|---|---|---|---|---|---|
| 1 | `creation_host` | `CreationHost` | `creation_host` | LOW | IP from account creation. Old reader has bug: writes `CreationHost` but reads `CreationIP`. |
| 2 | `last_login_host` | `LastHost` | `last_login_host` | LOW | Last login IP. Useful for security auditing. |
| 3 | `default_character` | `DefaultChar` | `default_character` | MEDIUM | Auto-login character. Affects the 'Y' menu option. |
| 4 | `mfa_pending_key` | `MFAPendingKey` | `mfa_pending_key` | LOW | In-progress MFA enrollment key. Transient state. |
| 5 | `mfa_pending` | `MFAPending` | `mfa_pending` | LOW | Flag that MFA enrollment is in progress. Transient state. |
| 6 | `email_verification_last_sent` | `EmailVerificationLastSent` | `email_verification_last_sent` | LOW | Rate-limiting timestamp for verification emails. |

#### Step 0c: Add missing ACCOUNT_CHARACTER fields to JSON (13 fields)

Per-character auth/email fields missing from the characters array in `json_account.c`:

| # | Struct Member | Old Key | JSON Key to Add | Severity | Notes |
|---|---|---|---|---|---|
| 1 | `last_host` | `LastHost` | `last_host` | LOW | Character's last login IP. |
| 2 | `old_pwd` | `OldPassword` | `old_password` | LOW | Legacy password for migration. |
| 3 | `reset_code` | `ResetCode` | `reset_code` | MEDIUM | Active password reset code. |
| 4 | `reset_time` | `ResetTime` | `reset_time` | MEDIUM | Password reset expiration. |
| 5 | `reset_state` | `ResetState` | `reset_state` | MEDIUM | Password reset state. |
| 6 | `mfa_pending_key` | `MFA_PendingKey` | `mfa_pending_key` | LOW | In-progress MFA setup key. |
| 7 | `recovery_codes[]` | `RecoveryCodes` | `recovery_codes[].code` | MEDIUM | MFA recovery codes. |
| 8 | `recovery_used[]` | `RecoveryUsed` | `recovery_codes[].used` | MEDIUM | Which recovery codes are consumed. |
| 9 | `email` | `Email` | `email` | MEDIUM | Character-specific email address. |
| 10 | `email_verified` | `EmailVerified` | `email_verified` | MEDIUM | Character email verification status. |
| 11 | `pending_email` | `PendingEmail` | `pending_email` | LOW | Pending email change. |
| 12 | `email_verification_code` | `EmailVerifyCode` | `email_verification_code` | LOW | Email verification code. |
| 13 | `email_verification_time` | `EmailVerifyTime` | `email_verification_time` | LOW | Email verification timestamp. |

**Note:** Many of these per-character auth fields will eventually be consolidated to the account level (Phase 5 auth cleanup). However, we must serialize them now to avoid data loss for characters that still have per-character auth data.

#### Step 0d: Add missing character (pfile) fields to JSON (27 fields)

Fields saved in old `fwrite_char()`/`fread_char()` but not in `json_char.c`:

**CRITICAL:**
| # | Struct Member | Old Key | JSON Key to Add | Notes |
|---|---|---|---|---|
| 1 | `ch->id[0]`, `ch->id[1]` | `Id`/`Id2` | `metadata.character_id` | Unique character identifier. Used for variable systems, persistence references. Without this, characters get new IDs on each load. |

**HIGH:**
| # | Struct Member | Old Key | JSON Key to Add | Notes |
|---|---|---|---|---|
| 2 | `ch->pcdata->staff_rank` | `StaffRank` | `character.staff_rank` | Immortal admin level. Lost = demoted. |
| 3 | `ch->dead` | `Dead` | `character.dead` | Death state flag. Lost = dead players come back alive. |
| 4 | `ch->time_left_death` | `DeathTimeLeft` | `character.death_time_left` | Permadeath timer. Lost = timer resets. |
| 5 | `ch->pcdata->class_current` | `Cla` | `character.classes.current` | Active class selector. Lost = game doesn't know current class. |
| 6 | `ch->pcdata->sub_class_current` | `Subcla` | `character.classes.current_sub` | Active subclass selector. Lost = game doesn't know current subclass. |

**MEDIUM:**
| # | Struct Member | Old Key | JSON Key to Add | Notes |
|---|---|---|---|---|
| 7 | `paf->slot` | Affect slot field | `affects[].slot` | Affect stacking behavior. |
| 8 | `last_note/idea/penalty/news/changes` | `Not` (5 values) | `character.note_timestamps` | Note read markers. Lost = all notes appear unread. |
| 9 | `hit_before/mana_before/move_before` | `HBS` | `character.vitals_before` | Pre-level vitals snapshot for train/level calculations. |
| 10 | `ch->pcdata->ignoring` | `Ignore` | `social.ignoring` | Ignore/block list. Lost = block lists wiped. |
| 11 | `ch->shifted` | `Shifted` | `character.shifted` | Shifted form state (werewolves/slayers). |
| 12 | `ch->pcdata->ships` | `Ship` | `character.ships` | Ship ownership. |
| 13 | `ch->pcdata->unlocked_areas` | `UnlockedArea` | `character.unlocked_areas` | Area unlock progress. Lost = must re-unlock content. |
| 14 | `ch->pcdata->commands` | `GrantedCommand` | `character.granted_commands` | Admin-granted extra commands. |
| 15 | `ch->pcdata->last_logoff` | `LogO` | `character.last_logoff` | Used for offline HP/mana regen calculations. |
| 16 | `ch->pcdata->songs_learned[]` | `Song` | `skills.songs` | Bard songs. Lost = must re-learn. |

**LOW-MEDIUM:**
| # | Struct Member | Old Key | JSON Key to Add | Notes |
|---|---|---|---|---|
| 17 | `ch->pcdata->flag` | `Flag` | `character.player_flag` | Player flag cosmetic. |
| 18 | `ch->pcdata->room_before_arena` | `Room_before_arena` | `character.room_before_arena` | Pre-arena room marker. |

**LOW:**
| # | Struct Member | Old Key | JSON Key to Add | Notes |
|---|---|---|---|---|
| 19 | `ch->pcdata->immortal->imm_flag` | `ImmFlag` | `character.imm_flag` | Immortal custom who-tag. |
| 20 | `ch->pcdata->danger_range` | `DangerRange` | `character.danger_range` | Danger range setting. |
| 21 | `ch->pcdata->afk_message` | `Afk_message` | `character.afk_message` | AFK message. |
| 22 | `ch->pcdata->vis_to_people` | `VisTo` | `social.vis_to` | Selective visibility list. |
| 23 | `ch->pcdata->quiet_people` | `QuietTo` | `social.quiet_to` | Quiet list. |
| 24 | `ch->before_social` | `Before_social` | `character.before_social` | Pre-social room marker. |
| 25 | `ch->pcdata->challenge_delay` | `ChDelay` | `character.challenge_delay` | Challenge delay timer. |
| 26 | `ch->pcdata->last_project_inquiry` | `LastInquiryRead` | `character.last_inquiry_read` | Inquiry read marker. |
| 27 | `ch->pcdata->last_area` | `LastArea` | `character.last_area` | Last area display string. |

#### Step 0e: Deprecation plan for old format code

Once all missing fields are added to JSON serialization:

1. **Add conversion utility**: An in-game `do_convert` staff command that bulk-converts all remaining old-format account/character files to JSON. Log any conversion errors.

2. **Remove old-format write path**: Delete `fwrite_account()`, `fwrite_account_character()`, and the old-format portions of `save_char_obj()` / `fwrite_char()` in `save.c`. Keep only JSON write paths.

3. **Mark old-format read path as legacy**: Keep `fread_account()`, `fread_account_character()`, `fread_char()` temporarily for reading unconverted files, but wrap them with deprecation warnings. Log whenever the old reader is invoked so we know when all files have been converted.

4. **Remove old-format read path**: Once all files are confirmed converted (no more deprecation warnings in logs), remove the old readers entirely. This may take multiple release cycles.

5. **Clean up `save.c`**: After removing both old read and write paths, `save.c` should shrink from ~7,650 lines to only the orchestration functions (`save_char_obj()`, `load_char_obj()`, etc.) that delegate to `json_char.c` and `json_account.c`. Target: ~500-1000 lines.

#### Old format bugs to NOT reproduce in JSON

The old format has pre-existing bugs that should be fixed, not preserved:

- **`CreationHost` vs `CreationIP` mismatch**: Old writer uses `CreationHost`, old reader looks for `CreationIP`. JSON should use `creation_host` consistently.
- **`StaffAccount` silently dropped**: Old writer writes `StaffAccount`, but old reader has no `case 'S'` handler so it's silently dropped on load. JSON already handles this correctly.

### Phase 1: Dead Code Cleanup & Navigation Fixes — ✅ COMPLETE
**Priority: High | Estimated: 1-2 sessions | Risk: Low**

1. **Add cancel/back-out options to all flows** ✅ COMPLETE
   - All 13 login flows now accept "cancel" or empty input to back out
   - Fixed bug in `login_change_character_email()` where cancel check came after validation
   - `login_change_account_password()` changed empty input from re-prompt to cancel

2. **Wire up extracted menu handlers** ✅ COMPLETE
   - Full rewrite of `nanny_menus.c` with production-ready handlers matching inline logic
   - `nanny_menus.h` rewritten with all 26 handler declarations
   - `login_account_menu()` reduced from ~280 lines inline switch to ~40-line dispatcher
   - `login_character_menu()` reduced from ~325 lines inline switch to ~20-line dispatcher
   - Added `find_most_recent_character()` and `set_default_character()` declarations to `nanny.h`

3. **Extract Y/Z direct-login helper** ✅ COMPLETE
   - Created `direct_login_character()` in `nanny_utils.c`
   - Used by Y, Z handlers and numeric selection in `login_account_menu()`

4. **Extract character sorting helper** ✅ COMPLETE
   - Created `sort_account_characters()` in `nanny_utils.c`
   - Replaces 4 inline bubble-sort blocks

5. **Remove dead in-game commands** ✅ COMPLETE
   - Removed `do_keygen` and `generate_key()` from `otp.c`
   - Removed `do_password` from `act_info.c`
   - Removed `do_email` from `act_comm.c`
   - Removed `mfa_question` field from `pc_data` struct and all handlers (interp.c, comm.c)
   - Removed all declarations from `interp.h` and `merc.h`
   - Removed all entries from `tables.c` and static `cmd_table` in `interp.c`
   - Disabled `mfa` command in `commands.json` with redirect message
   - `check_mfa()` wrapper in `otp.c` preserved (still used by nanny.c)

6. **Password validation consolidation** ✅ COMPLETE
   - Removed `validate_password_strength_basic()` from `nanny_utils.c` and `nanny_utils.h`
   - `nanny_auth.c` now calls `validate_password_strength()` from `auth.c` (stricter: requires letters+digits)
   - Note: `update_password()`, `update_mfa_key()`, `update_email()` stubs do not exist (plan was incorrect)

### Phase 2: Penalty & Bonus System
**Priority: Medium | Estimated: 2-3 sessions | Risk: Medium**

Define and implement the penalty/bonus system for accounts and characters.

1. **Define data structures**
   ```c
   typedef struct penalty_data PENALTY_DATA;
   struct penalty_data {
       PENALTY_DATA *next;
       int type;              // PENALTY_BAN, PENALTY_MUTE, PENALTY_RESTRICT, etc.
       int scope;             // SCOPE_ACCOUNT, SCOPE_CHARACTER
       char *reason;          // Why the penalty was applied
       char *applied_by;      // Staff member who applied it
       time_t applied_at;     // When it was applied
       time_t expires_at;     // When it expires (0 = permanent)
       char *target_name;     // Character name (if character-scoped)
       long flags;            // Additional penalty flags
   };
   
   typedef struct bonus_data BONUS_DATA;
   struct bonus_data {
       BONUS_DATA *next;
       int type;              // BONUS_XP, BONUS_GOLD, BONUS_SLOTS, etc.
       int scope;             // SCOPE_ACCOUNT, SCOPE_CHARACTER
       char *reason;          // Why the bonus was granted
       char *granted_by;      // Staff member who granted it
       time_t granted_at;     // When it was granted
       time_t expires_at;     // When it expires (0 = permanent)
       int value;             // Bonus value (e.g., +50% XP, +2 character slots)
       char *target_name;     // Character name (if character-scoped)
       long flags;            // Additional bonus flags
   };
   ```

2. **Penalty types**
   - `PENALTY_MUTE` - Cannot use public channels
   - `PENALTY_NOCHAT` - Cannot use specific chat rooms
   - `PENALTY_NOEMOTE` - Cannot use emotes
   - `PENALTY_NOTELL` - Cannot use tells
   - `PENALTY_DENY` - Cannot log in
   - `PENALTY_RESTRICT` - Various restrictions (configurable)
   - `PENALTY_FREEZE` - Account frozen (all characters)
   - `PENALTY_LOG` - All actions logged

3. **Bonus types**
   - `BONUS_XP` - XP multiplier
   - `BONUS_GOLD` - Gold multiplier
   - `BONUS_CHAR_SLOTS` - Extra character slots
   - `BONUS_STAFF_SLOTS` - Extra staff slots
   - `BONUS_CUSTOM` - Custom bonuses (descriptive)

4. **Staff commands**
   - `penalty list <account|player:name>` - List penalties
   - `penalty add <account|player:name> <type> [duration] <reason>` - Add penalty
   - `penalty remove <account|player:name> <#>` - Remove penalty
   - `bonus list <account|player:name>` - List bonuses
   - `bonus add <account|player:name> <type> [value] [duration] <reason>` - Add bonus
   - `bonus remove <account|player:name> <#>` - Remove bonus

5. **JSON serialization**
   - Add `penalties` and `bonuses` sections to account JSON
   - Serialize with human-readable type names

6. **Runtime enforcement**
   - Check penalties during login, channel use, and relevant actions
   - Apply bonuses to XP/gold calculations
   - Expiration checking in update loop

### Phase 3: Account Unlock System
**Priority: Medium | Estimated: 2-3 sessions | Risk: Medium**

Implement account-level unlocks for races (and eventually classes).

1. **Define unlock data structure**
   ```c
   typedef struct account_unlock_data ACCOUNT_UNLOCK;
   struct account_unlock_data {
       int type;               // UNLOCK_RACE, UNLOCK_CLASS, UNLOCK_FEATURE
       char *identifier;       // Race/class name or feature key
       char *unlocked_by;      // How it was unlocked (quest/achievement/staff)
       time_t unlocked_at;     // When it was unlocked
       bool permanent;         // Whether it persists across remorts
   };
   ```

2. **Populate `avail_races` on account**
   - Replace the unused `LLIST *avail_races` with `LLIST *unlocks`
   - Default unlocks: all "base" races available to new accounts
   - Additional races unlocked through gameplay progression

3. **Modify character creation**
   - `login_get_new_race()` checks account unlocks in addition to game-level requirements
   - Display unlocked races with markers (e.g., "{G[Unlocked]{x")
   - Show locked races grayed out with unlock requirements

4. **Unlock sources**
   - Script command: `unlockrace <player> <race>` (for quests/achievements)
   - Staff command: `accunlock add <account> race <racename>` 
   - Achievement system hook (future)
   - Remort system hook (future - part of class/remort backport)

5. **JSON serialization**
   - Add `unlocks` section to account JSON
   - Serialize with type, identifier, source, timestamp

6. **Race configuration**
   - Add `unlock_requirement` field to race JSON definitions
   - Values: `"default"` (always available), `"unlock"` (requires unlock), `"staff"` (staff only)
   - Display table of races with unlock status in character creation

### Phase 4: Enhanced Account/Character Notes
**Priority: Low | Estimated: 1-2 sessions | Risk: Low**

Extend the existing `do_accnote` system and add character-level notes.

1. **Enhance `do_accnote`**
   - Add note categories (e.g., "warning", "info", "punishment", "reward")
   - Add note editing capability
   - Show note count in account display for staff
   - Link notes to penalties/bonuses when applicable

2. **Add `do_charnote` command**
   - Staff command to add notes to specific characters
   - Notes stored at the `ACCOUNT_CHARACTER` level in the account JSON
   - Syntax mirrors `accnote` but targets characters

3. **Show notes to staff on login**
   - When a player's character enters the game, notify online staff if any notes exist
   - Configurable threshold (e.g., only show "warning" category notes)

4. **Player-visible account notes**
   - Allow players to view their own account notes (read-only, filtered)
   - Show penalty/bonus information in account/character menus
   - Add "Account standing" display showing active penalties/bonuses

### Phase 5: OTP & Auth Cleanup — ✅ COMPLETE
**Priority: Medium | Estimated: 1-2 sessions | Risk: Low**

> **Note:** Items 1 and 2 were completed early as part of Phase 1 dead code removal.
> `do_keygen`, `generate_key()`, and the `mfa_question` system have been fully removed.

1. ~~**Fix `do_keygen` to use auth data path**~~ ✅ REMOVED in Phase 1

2. ~~**Remove `generate_key()` compatibility function**~~ ✅ REMOVED in Phase 1

3. **Review MFA key encryption consistency** ✅ COMPLETE
   - `setup_mfa_for_account()` was using `encrypt_string()` (v1) — fixed to use `encrypt_string_versioned()` (v2)
   - `setup_mfa_for_char()` already used `encrypt_string_versioned()` (v2)
   - All new key creation paths now use v2 (XSalsa20-Poly1305)
   - Existing v1 keys are transparently migrated by `migrate_otp_key()` on first use

4. **Clean up recovery code generation/display** ✅ COMPLETE (already working)
   - `generate_recovery_codes()` is generic (works on any codes/used arrays)
   - `display_recovery_codes()` works for character (ACCOUNT_CHARACTER)
   - `display_account_recovery_codes()` works for account (ACCOUNT_DATA)
   - Account-level recovery code regeneration already exists (option 'G' in account MFA menu)
   - Fixed `check_account_mfa()` to check recovery codes as fallback (was missing)

5. **Audit key storage paths** ✅ COMPLETE
   - All new key creation goes through `setup_mfa_for_char()`/`setup_mfa_for_account()` (now both v2)
   - All code validation goes through `check_char_mfa()`/`check_account_mfa()` or auth API
   - Fixed `check_account_mfa()` to activate pending keys on confirmation (was missing)
   - MFA disable flows in nanny.c now properly clear `mfa_enabled` flag (was left stale)
   - Direct pcdata accesses in nanny.c (guard checks), save.c (serialization), and act_wiz.c (staff admin) are appropriate and necessary

### Phase 6: Menu Display Polish
**Priority: Low | Estimated: 1 session | Risk: Low**

1. **Account menu improvements**
   - Show account standing (penalties/bonuses summary)
   - Show unlock count ("X races unlocked")
   - Show note indicators for staff
   - Clean up the display formatting consistency

2. **Character menu improvements**
   - Show character-specific penalties/bonuses
   - Show auth status more clearly (password: yes/no, MFA: yes/no)
   - Show deletion status with time remaining
   - Clean up display formatting

3. **Character creation race display**
   - Show unlocked vs locked races
   - Show unlock requirements for locked races
   - Group races by category if applicable

---

## Dependencies

| Phase | Depends On | Blocks |
|-------|-----------|--------|
| Phase 0 (JSON Migration) | Nothing | Everything (must complete first) |
| Phase 1 (Cleanup) | Phase 0 | Everything else |
| Phase 2 (Penalties) | Phase 1 | Phase 4, Phase 6 |
| Phase 3 (Unlocks) | Phase 1 | Phase 6, Remort backport |
| Phase 4 (Notes) | Phase 2 | Phase 6 |
| Phase 5 (OTP) | Phase 1 | Nothing |
| Phase 6 (Polish) | Phase 2, 3, 4 | Nothing |

---

## Files to Create/Modify

### New Files
- `src/account/penalty.c` - Penalty management
- `src/account/penalty.h` - Penalty definitions and declarations
- `src/account/bonus.c` - Bonus management
- `src/account/bonus.h` - Bonus definitions and declarations
- `src/account/unlock.c` - Account unlock management
- `src/account/unlock.h` - Unlock definitions and declarations

### Modified Files (Phase 0)
- `src/io/json/json_account.c` - Add 19 missing fields (6 account + 13 character)
- `src/io/json/json_account.h` - Any new declarations needed
- `src/io/json/json_char.c` - Add 27 missing fields, fix `played` bug
- `src/io/json/json_char.h` - Any new declarations needed
- `src/save.c` - Mark old format code for deprecation, add conversion utility

### Modified Files (Phase 1+)
- `src/merc.h` - Add penalty, bonus, unlock structures; uncomment penalty/bonus pointers
- `src/nanny.c` - Navigation fixes, wire up extracted handlers, remove duplication
- `src/nanny/nanny_menus.c` - May need updates as main handlers change
- `src/nanny/nanny_menus.h` - New handler declarations
- `src/nanny/nanny_utils.c` - Character sorting helper, shared utilities
- `src/nanny/nanny_utils.h` - New utility declarations
- `src/account/otp.c` - Fix `do_keygen`, remove `generate_key()`, encryption consistency
- `src/account/auth.c` - Remove stubs, consolidate validation
- `src/account/account_notes.c` - Category support, character notes
- `src/io/json/json_account.c` - Serialize penalties, bonuses, unlocks
- `src/io/json/json_account.h` - New serialization declarations
- `src/interp.c` - Register new commands (penalty, bonus, accunlock, charnote)
- `src/const.c` - New flag tables for penalty/bonus types
- `src/CMakeLists.txt` - New source files
- `src/Makefile` - New source files
- `data/races/*.json` - Add `unlock_requirement` field

---

## Implementation Order Recommendation

Start with **Phase 0** (JSON completeness & old format deprecation) — this is a **prerequisite** for everything else. The `played` bug is actively corrupting data and the 46 missing fields represent potential data loss on every save. Phase 0 steps should be done in order:
1. Fix the `played` bug first (Step 0a) — it's the only one actively corrupting data
2. Add missing fields to `json_char.c` (Step 0d) — highest volume of missing fields, with CRITICAL `ch->id` gap
3. Add missing fields to `json_account.c` (Steps 0b + 0c) — lower severity but complete the picture
4. Build the conversion utility and deprecation wrappers (Step 0e)

Then proceed with **Phase 1** (dead code cleanup and navigation fixes) since it's pure improvement with minimal risk and will make subsequent phases cleaner. Then **Phase 5** (OTP cleanup) since it's closely related.

Phases 2, 3, and 4 can be done in any order but building penalties first (Phase 2) gives a foundation that notes and unlocks can reference.

Phase 6 should be done last as it depends on having the new data available to display.

---

## Testing Strategy

- **Phase 0:** 
  - Add JSON round-trip test cases for all new fields (write → read → verify values match)
  - Test the `played` bug fix: verify seconds → JSON → seconds round-trip preserves value
  - Test backward compatibility: verify files with old key (`played_hours`) are read correctly with migration
  - Test old format files can still be loaded (legacy reader) and re-saved as JSON with all fields
  - Run conversion utility on test accounts/characters and verify completeness
- **Phase 1:** Build test, manual login flow testing for all cancel paths
- **Phase 2-4:** Add JSON test scenarios for serialization/deserialization of new data types
- **Phase 5:** Existing MFA tests + manual verification
- **Phase 6:** Manual visual inspection of menus

All phases: `./build tests && cd /sentience && ./sent -test`

---

## Notes

- The `ACCOUNT_CHARACTER` struct already has extensive auth fields (password, MFA, email). The penalty/bonus/unlock systems should be at the `ACCOUNT_DATA` level with optional per-character targeting.
- The existing `unlocked_areas` system on `PC_DATA` is per-character and uses area UIDs. The new account unlock system should be per-account and use string identifiers for races/classes.
- The pubsub communication plan (`docs/PLAN_pubsub_communication.md`) already proposes a `CHANNEL_PENALTY_DATA` struct — the penalty system here should be compatible with or a superset of that design.
