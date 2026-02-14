# Account System Overhaul — Work Log

**Plan:** [PLAN_account_overhaul.md](PLAN_account_overhaul.md)

---

## Phase 0: JSON Completeness ✅ COMPLETE

### Step 0a: Fix `played` bug
- Fixed unit mismatch in `json_char.c` where playtime was saved as hours but read as seconds
- Changed JSON write to save raw seconds; renamed key from `played_hours` to `played`
- Added backward-compatible migration: if `played_hours` found, multiply by 3600

### Step 0b: Add missing account-level fields (6 fields)
Added to `json_account.c`:
- `creation_host`
- `last_login_host`
- `default_character`
- `mfa_pending_key`
- `mfa_pending`
- `email_verification_last_sent` (fixed per-account vs per-character scope bug)

### Step 0c: Add missing ACCOUNT_CHARACTER fields (13 fields)
Added per-character auth/email fields to `json_account.c`:
- `last_host`, `old_password`, `reset_code`, `reset_time`, `reset_state`
- `mfa_pending_key`, `recovery_codes[].code`, `recovery_codes[].used`
- `email`, `email_verified`, `pending_email`
- `email_verification_code`, `email_verification_time`

### Step 0d: Add missing character (pfile) fields (27 fields)
Added to `json_char.c`:
- CRITICAL: `character_id` (ch->id[0], ch->id[1])
- HIGH: `staff_rank`, `dead`, `death_time_left`, `classes.current`, `classes.current_sub`
- MEDIUM: affect `slot`, note timestamps, vitals_before, ignoring, shifted, ships,
  unlocked_areas, granted_commands, last_logoff, songs
- LOW: player_flag, room_before_arena, imm_flag, danger_range, afk_message,
  vis_to, quiet_to, before_social, challenge_delay, last_inquiry_read, last_area

### Audit
- Fixed `imm_flag` read gap (write existed but read was missing in `json_char.c`)
- Created [SERIALIZATION_COMPARISON.md](SERIALIZATION_COMPARISON.md) documenting all entity type parity

---

## Phase 1: Dead Code Cleanup & Navigation Fixes — IN PROGRESS

### Cancel/Back-Out Flows ✅ COMPLETE

Added cancel/back-out to all 13 login flows in `nanny.c`:

| Flow | Fix Applied |
|------|-------------|
| `login_verify_account_email_change` | Accept "cancel" to return to account menu |
| `login_account_mfa_verify_for_settings` | Accept "cancel" to return to account menu |
| `login_change_account_password` | Empty input cancels (was: re-prompt) |
| `login_confirm_account_password_change` | Accept "cancel" to return to account menu |
| `login_account_mfa_confirm` | Accept "cancel" to return to account menu |
| `login_change_character_email` | Moved cancel check before email validation (was: unreachable) |
| `login_link_character_password` | Accept "cancel" to return to account menu |
| `login_verify_unlink_password` | Accept "cancel" to return to character menu |
| `login_verify_unlink_mfa` | Accept "cancel" to return to character menu |
| `login_verify_delete_password` | Accept "cancel" to return to character menu |
| `login_verify_delete_mfa` | Accept "cancel" to return to character menu |
| `login_character_mfa_confirm` | Accept "cancel" to return to character menu |
| `login_character_mfa_verify_for_settings` | Accept "cancel" to return to character menu |
| `login_verify_character_email_change` | Accept "cancel" to return to character menu |

### Helper Extraction ✅ COMPLETE

**`direct_login_character()`** — `nanny_utils.c`
- Replaces ~100-line duplicate blocks in Y and Z account menu handlers
- Handles character loading, auth checking, reconnection, and game entry
- Also used by numeric character selection

**`sort_account_characters()`** — `nanny_utils.c`
- Replaces 4 inline bubble-sort blocks
- Returns sorted staff and regular character arrays
- Used by `display_account_menu()`, `login_account_menu()`, and character selection

### Dead Code Removal ✅ COMPLETE

Removed in-game commands that duplicate account system functionality:

| What | Where | Action |
|------|-------|--------|
| `do_keygen()` function | `otp.c` | Removed |
| `generate_key()` function | `otp.c` | Removed |
| `do_password()` function | `act_info.c` | Removed |
| `do_email()` function | `act_comm.c` | Removed |
| `mfa_question` field | `merc.h` pc_data struct | Removed |
| `mfa_question` handler | `interp.c` interpret() | Removed |
| `mfa_question` prompt | `comm.c` prompt handling | Removed |
| `DECLARE_DO_FUN(do_keygen)` | `interp.h` | Removed |
| `DECLARE_DO_FUN(do_password)` | `interp.h` | Removed |
| `DECLARE_DO_FUN(do_email)` | `interp.h` | Removed |
| `void generate_key(...)` | `merc.h` | Removed |
| `do_keygen` entry | `tables.c` function table | Removed |
| `do_email` entry | `tables.c` function table | Removed |
| `do_password` entry | `tables.c` function table | Removed |
| `email` entry | `interp.c` static cmd_table | Removed |
| `password` entry | `interp.c` static cmd_table | Removed |
| `mfa` command | `commands.json` | Disabled with redirect message |

**Preserved:** `check_mfa()` wrapper in `otp.c` — still called from `nanny.c` (lines 1642, 2734).

### Remaining Phase 1 Work

- [x] Wire up `nanny_menus.c` handlers (replace inline logic in `login_account_menu`/`login_character_menu`)
- [x] Consolidate `validate_password_strength()` / `validate_password_strength_basic()`

### Phase 1.5: Menu Handler Full Rewrite

**Date:** February 2026

Subagent audit revealed the existing `nanny_menus.c` handlers were oversimplified stubs that didn't match
the inline logic (wrong permission checks, missing auth flows, missing display calls, wrong option letters).
User chose full rewrite over incremental patching.

**Changes:**
- `nanny_menus.c`: Full rewrite (~441 → 883 lines). All 26 handlers now match inline logic exactly:
  - Account handlers (14): C, I, L, E, P, M, V, R, S, Q, Y, Z, numeric, alpha
  - Character handlers (12): L, P, X, M, E, V, R, U, D, C, Y, B
- `nanny_menus.h`: Rewritten with all 26 handler declarations (~91 → 112 lines)
- `login_account_menu()` in `nanny.c`: Reduced from ~280 lines inline switch to ~40-line dispatcher
- `login_character_menu()` in `nanny.c`: Reduced from ~325 lines inline switch to ~20-line dispatcher
- `nanny.h`: Added `find_most_recent_character()` and `set_default_character()` declarations
- `nanny.c`: Added `#include "nanny/nanny_menus.h"`
- Net reduction: ~631 lines removed from `nanny.c` (6,530 → 5,899)

**Key corrections from audit:**
- `can_unlink_characters()` not `can_link_characters()` for unlink permission
- `IS_IMMORTAL()` not `staff_rank` check for staff character creation
- `set_default_character()` not manual assignment
- `display_account_mfa_menu()`/`display_character_mfa_menu()` calls for MFA transitions
- Proper cancel state transitions (not just `nanny_send_error`)

### Phase 1.6: Password Validation Consolidation

**Date:** February 2026

- Removed `validate_password_strength_basic()` from `nanny_utils.c` and `nanny_utils.h`
- `nanny_auth.c` now calls `validate_password_strength()` from `auth.c` (already included `auth.h`)
- The auth.c version is stricter: requires both letters AND digits in addition to length checks
- `validate_password_strength_basic()` was a placeholder that only checked length

### Phase 1 — COMPLETE

All Phase 1 items finished. Build is clean (zero new warnings).

---

## Phase 5: OTP & Auth Cleanup

**Date:** February 11, 2026

Items 1 & 2 (do_keygen removal, generate_key removal) were completed in Phase 1.

### Item 3: MFA Key Encryption Consistency

- `setup_mfa_for_account()` in otp.c was using `encrypt_string()` (v1 AES-CBC)
- `setup_mfa_for_char()` was already using `encrypt_string_versioned()` (v2 XSalsa20-Poly1305)
- Fixed: Changed `setup_mfa_for_account()` to use `encrypt_string_versioned()`
- Existing v1 keys are transparently migrated by `migrate_otp_key()` on first use
- Key rotation command in act_wiz.c handles bulk re-encryption

### Item 4: Recovery Code Cleanup

Audit found the recovery code system was already complete:
- `generate_recovery_codes()` in handler.c: generic, works for both account and character
- `display_recovery_codes()` in handler.c: takes ACCOUNT_CHARACTER
- `display_account_recovery_codes()` in handler.c: takes ACCOUNT_DATA
- Account MFA menu option 'G' already provides recovery code regeneration
- Recovery codes are hashed in-place after display (`hash_recovery_codes_in_place()`)

**Bug found and fixed:** `check_account_mfa()` in otp.c was missing recovery code fallback.
`check_char_mfa()` properly checks recovery codes after TOTP fails, but the account
version only checked TOTP. Fixed to match character behavior.

### Item 5: Key Storage Path Audit

Comprehensive audit of all MFA field accesses across the codebase:

| File | Access Type | Category | Status |
|------|------------|----------|--------|
| otp.c | Key generation, validation, migration | Auth API | ✅ Correct |
| nanny.c | Guard checks (`!IS_NULLSTR(mfa_key)`) | Read-only routing | ✅ Appropriate |
| nanny.c | MFA confirm/disable flows | Direct manipulation | ✅ Fixed (see below) |
| nanny_menus.c | Guard checks for UI | Read-only routing | ✅ Appropriate |
| nanny_utils.c | Guard check in direct_login | Read-only | ✅ Appropriate |
| comm.c | Reconnect MFA check | Read-only | ✅ Appropriate |
| save.c | Serialization/deserialization | Legacy format I/O | ✅ Expected |
| act_wiz.c | Staff MFA reset, key rotation | Admin operations | ✅ Appropriate |
| mem.c | Initialization defaults | Memory setup | ✅ Expected |

**Bugs found and fixed:**
1. `check_account_mfa()` missing pending key activation — when a TOTP code validates
   against the pending key during setup, the character version (`check_char_mfa()`)
   promotes it to active and sets `mfa_enabled = true`. Account version didn't. Fixed.
2. `login_account_mfa_disable_confirm()` didn't clear `mfa_enabled`/`mfa_pending` flags
   when disabling MFA. Comment said "no need" but leaving stale flags is incorrect. Fixed.
3. `login_character_mfa_disable_confirm()` same issue — didn't clear `mfa_enabled`. Fixed.

### Phase 5 — COMPLETE

All items finished. Build is clean (zero new warnings).
