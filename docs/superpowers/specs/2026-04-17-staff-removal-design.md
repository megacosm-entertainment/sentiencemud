# Staff Removal — Design Spec

## Problem

Removing staff status is incomplete. Two gaps exist:

1. **`staff delete`** removes the `IMMORTAL_DATA` record from the in-memory `immortal_list` and saves `staff.json`, but does **not**:
   - Reset `ch->pcdata->staff_rank` to `STAFF_PLAYER`
   - Clear `ch->pcdata->immortal` pointer (if online)
   - Update the `ACCOUNT_CHARACTER` entry (`staff` / `staff_rank` fields)
   - Save the character pfile
   - Clear the account's `ACCT_CAN_CREATE_STAFF` flag when no staff characters remain

   Result: on next login, `save.c:1278` re-links the character as immortal because `staff_rank > STAFF_PLAYER` in the pfile, or because the immortal_list entry was re-created as a safety fallback.

2. **`sdemote`** has a floor at `STAFF_GIMP` (rank 1) — it cannot return a character to `STAFF_PLAYER` (rank 0). There is no path to fully revoke staff via demotion.

## Solution

### Shared helper: `remove_staff_status()`

A new function in `staff.c` that performs the complete removal sequence:

```c
void remove_staff_status(const char *name);
```

Steps, in order:

1. **Remove IMMORTAL_DATA** from `immortal_list`, free it, call `save_immstaff()`.
2. **Online character path**: find via `get_char_world()`. If found:
   - Set `ch->pcdata->staff_rank = STAFF_PLAYER`
   - Set `ch->pcdata->immortal = NULL`
   - Call `save_char_obj(ch)`
3. **Offline character path**: if not online, use `load_char_obj_basic()` into a temporary descriptor:
   - Set `staff_rank = STAFF_PLAYER`
   - Set `immortal = NULL`
   - Save via `save_char_obj()`
   - Free the temporary character
4. **Account update**: call `find_account(name)` to locate the account:
   - Find the matching `ACCOUNT_CHARACTER` entry, set `staff = false`, `staff_rank = 0`
   - Scan remaining characters — if none have `staff == true`, clear `ACCT_CAN_CREATE_STAFF` from `acct->acct_flags`
   - Call `save_account(acct)`
   - Free the account if it was loaded temporarily

### Changes to `do_sdelete`

Replace the current manual linked-list removal and `free_immortal()` call with a call to `remove_staff_status(immortal->name)`. Keep the existing authorization and argument parsing. Messaging stays the same.

### Changes to `do_sdemote`

- Remove the `STAFF_GIMP` floor check that currently prevents demotion below rank 1.
- When the new rank resolves to `STAFF_PLAYER` (rank 0): call `remove_staff_status(player->name)` instead of setting `staff_rank` directly.
- Allow `STAFF_PLAYER` / `"player"` as a valid target rank in the rank lookup.

### Edge cases

- **Character doesn't exist**: `remove_staff_status` should handle this gracefully — remove the immortal record even if the pfile is missing, log a warning.
- **Account not found**: Log a warning but don't fail — the immortal record and pfile are still cleaned up.
- **Already not staff**: If the character's `staff_rank` is already `STAFF_PLAYER`, skip the pfile update.
- **Self-demotion**: `sdemote` already prevents demoting someone of equal or higher rank. No change needed.

## Files Modified

| File | Change |
|------|--------|
| `staff.c` | Add `remove_staff_status()`, update `do_sdelete()`, update `do_sdemote()` |
| `merc.h` | Declare `remove_staff_status()` |
| `tests/` | New test suite for staff removal |
| `tests/data/` | JSON test definitions |
| `CMakeLists.txt` | Add test source file |
| `Makefile` | Add test source file |

## Testing

### Unit tests (prefix: `stfrm_`)

1. `stfrm_delete_clears_immortal_list` — after `remove_staff_status`, `find_immortal()` returns NULL
2. `stfrm_delete_resets_staff_rank` — online character's `staff_rank` becomes `STAFF_PLAYER`
3. `stfrm_delete_clears_immortal_pointer` — online character's `immortal` pointer is NULL
4. `stfrm_sdemote_to_player_triggers_removal` — demoting to `STAFF_PLAYER` calls full removal logic
5. `stfrm_sdemote_below_gimp_allowed` — demotion to player rank succeeds (no floor)
6. `stfrm_account_entry_updated` — `ACCOUNT_CHARACTER.staff` is false after removal
7. `stfrm_account_flag_cleared` — `ACCT_CAN_CREATE_STAFF` cleared when last staff char removed
8. `stfrm_nonexistent_char_graceful` — removing a character that doesn't exist doesn't crash

## Non-goals

- No changes to `spromote` or `sadd` (they work correctly as-is).
- No confirmation prompt on `staff delete` (the command is explicit enough).
- No changes to the staff rank hierarchy itself.
