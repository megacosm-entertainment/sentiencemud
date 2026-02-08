# Account and Character Loading Performance Issues

## Problem Statement

After the game runs for a while, account and character loading becomes progressively slower, especially for characters with large inventories. This suggests a memory leak or accumulation of data that isn't being cleaned up properly.

## Root Causes Identified

### 1. Missing Refcount Increment on Cached Account Load

**Location**: [save.c:5241-5253](save.c#L5241-L5253)

**Issue**: When `load_account()` finds a cached account in `loaded_accounts`, it returns early WITHOUT incrementing the refcount:

```c
if (loaded_accounts) {
    ITERATOR it;
    ACCOUNT_DATA *acct;
    iterator_start(&it, loaded_accounts);
    while ((acct = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
        if (!str_cmp(acct->username, name)) {
            d->account = acct;
            iterator_stop(&it);
            return true;  // <-- BUG: Returns without acct->refcount++
        }
    }
    iterator_stop(&it);
}
```

**Impact**:
- Refcount doesn't match actual number of active connections
- Account may be freed prematurely when a descriptor disconnects
- OR account may NEVER be freed because refcount is inconsistent

**Expected Behavior**: Should increment `acct->refcount++` before returning cached account

---

### 2. Stale Character Metadata on Reconnect

**Location**: [save.c:5241-5253](save.c#L5241-L5253)

**Issue**: When returning a cached account, character metadata in `account->characters` list may be stale (not re-read from disk).

**Scenario**:
1. Player A logs in → Account loaded, character list populated
2. Player A disconnects → Account stays in `loaded_accounts` cache
3. Admin modifies character file on disk (adds items, changes data)
4. Player A reconnects → Cached account returned with OLD character metadata

**Impact**:
- Character inventory/stats may not reflect disk state
- Accumulated temporary data from previous session not cleared

---

###3. Character Load Path - No Duplicate Prevention

**Current Behavior**:
- Each call to `load_char_obj()` creates a NEW `CHAR_DATA` instance
- On reconnect, the NEW instance is freed and descriptor attached to EXISTING instance
- BUT: What if reconnect logic fails? The new instance may leak.

**Reconnect Path Analysis**:
1. `load_char_obj()` → Creates new `CHAR_DATA`, loads inventory [save.c:921](save.c#L921)
2. `login_read_motd()` → Checks for existing char in `loaded_chars` [nanny.c:2437-2446](nanny.c#L2437-L2446)
3. If found → Frees new char, reconnects to existing [nanny.c:2467-2468](nanny.c#L2467-L2468)
4. If NOT found → New char stays, added to `loaded_chars` [nanny.c:2524-2525](nanny.c#L2524-L2525)

**Potential Issue**: Between steps 1-3, TWO character instances exist briefly:
- The new one being loaded
- The existing one in `loaded_chars`

Both may have full inventories loaded, doubling memory usage during reconnect.

---

### 4. Vault Items Accumulation

**Location**: [save.c:5316-5356](save.c#L5316-L5356)

**Issue**: When loading an account, vault items are loaded from disk. But if account is cached, are old vault items cleared first?

**Code Analysis**:
```c
while (!str_cmp((word = fread_word(fp)), "#O"))
{
    obj = fread_obj_new(fp);
    if (obj == NULL)
        continue;

    objNestList[obj->nest] = obj;

    if (obj->nest == 0) {
        obj->next_content = account->vault_items;  // <-- Prepends to existing list!
        account->vault_items = obj;
```

**Problem**: `obj->next_content = account->vault_items` PREPENDS to the existing vault list. If account is cached and vault is reloaded, items accumulate!

**Expected**: Should clear `account->vault_items` before loading vault section.

---

## Inventory Cleanup - Verified Working

The following cleanup paths appear correct:

### `free_char()` - [mem.c:710-735](mem.c#L710-L735)
✅ Properly iterates and extracts all inventory objects:
```c
// Free items in inventory using lcarrying
if (ch->lcarrying) {
    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj(obj);
    }
    iterator_stop(&it);
}
// ... also frees lworn and llocker
```

### Reconnect Cleanup - [nanny.c:2467](nanny.c#L2467) and [comm.c:2741-2744](comm.c#L2741-L2744)
✅ Both paths free temporary character:
```c
// nanny.c:2467
free_char(ch);

// comm.c:2742-2744
if (d->character && d->character != ch) {
    free_char(d->character);
}
```

---

## Recommended Fixes

### Fix 1: Increment Refcount on Cached Account Load

**File**: [save.c:5246-5250](save.c#L5246-L5250)

```c
if (!str_cmp(acct->username, name)) {
    d->account = acct;
    acct->refcount++;  // <-- ADD THIS
    iterator_stop(&it);
    return true;
}
```

### Fix 2: Clear Vault Items Before Reloading

**File**: [save.c:5316](save.c#L5316)

```c
else if (!str_cmp(word, "VAULT"))
{
    // Clear existing vault items first to prevent accumulation
    while (account->vault_items) {
        OBJ_DATA *obj_next = account->vault_items->next_content;
        extract_obj(account->vault_items);
        account->vault_items = obj_next;
    }

    // Process vault items
    OBJ_DATA *obj;
    // ... rest of vault loading code
```

### Fix 3: Consider Reloading Character Metadata on Cached Account

**File**: [save.c:5247-5250](save.c#L5247-L5250)

**Option A**: Always reload account from disk (slower but always fresh)
**Option B**: Add timestamp check - only reload if disk file is newer
**Option C**: Add a "dirty" flag that forces reload

For now, recommend **Option C**:
```c
if (!str_cmp(acct->username, name)) {
    d->account = acct;
    acct->refcount++;

    // If account was modified on disk, reload character metadata
    if (acct->needs_reload || check_account_modified(name)) {
        reload_account_character_list(acct, fp);
        acct->needs_reload = false;
    }

    iterator_stop(&it);
    return true;
}
```

### Fix 4: Add Duplicate Character Prevention

**File**: [save.c:901](save.c#L901) - `load_char_obj()`

Add check at start of function:
```c
bool load_char_obj(DESCRIPTOR_DATA *d, char *name)
{
    // Check if character is already loaded
    ITERATOR it;
    CHAR_DATA *existing;
    iterator_start(&it, loaded_chars);
    while ((existing = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (!IS_NPC(existing) && !str_cmp(existing->name, name)) {
            // Character already loaded - this is a reconnect
            // Don't create a new instance
            iterator_stop(&it);
            d->character = existing;
            existing->desc = d;
            return true;
        }
    }
    iterator_stop(&it);

    // Normal load path - create new character
    // ... existing code
}
```

---

## Testing Plan

### 1. Memory Leak Test
1. Start fresh MUD instance
2. Create test account with character
3. Add 1000 items to character inventory
4. Login/logout 100 times
5. Monitor memory usage:
   ```bash
   while true; do
       ps aux | grep sent | grep -v grep
       sleep 60
   done
   ```

### 2. Vault Accumulation Test
1. Create account with 10 vault items
2. Login/logout 10 times
3. Check vault item count: `count_vault_items(account)` - should stay at 10, not grow

### 3. Refcount Test
1. Add logging to `load_account()` and `close_socket()`:
   ```c
   log_stringf("Account %s refcount: %d", account->username, account->refcount);
   ```
2. Login/logout multiple times
3. Verify refcount goes up/down correctly
4. Verify account freed when refcount hits 0

### 4. Stale Data Test
1. Login as character A
2. While logged in (from another terminal), manually edit character file to add an item
3. Logout and login again
4. Verify new item appears (or doesn't, depending on caching behavior)

---

## Related Code Locations

### Account Management
- [save.c:5233](save.c#L5233) - `load_account()`
- [save.c:753](save.c#L753) - `save_char_obj()`
- [comm.c:1442-1448](comm.c#L1442-L1448) - Account cleanup on disconnect

### Character Management
- [save.c:901](save.c#L901) - `load_char_obj()`
- [mem.c:683](mem.c#L683) - `free_char()`
- [nanny.c:2437-2496](nanny.c#L2437-L2496) - Reconnect logic in `login_read_motd()`
- [comm.c:2632](comm.c#L2632) - `check_reconnect()`
- [comm.c:2730](comm.c#L2730) - `complete_reconnect()`

### Global Lists
- [comm.c:422](comm.c#L422) - `loaded_accounts` creation
- [db.c:703](db.c#L703) - `loaded_accounts` declaration
- `loaded_chars` - Similar management to loaded_accounts

---

## Implementation Status

### ✅ FIXED: Refcount Bug (Fix #1)

**Date**: 2026-01-01
**Files Modified**:
- [save.c:5248](save.c#L5248) - Added `acct->refcount++` when returning cached account
- [save.c:5250-5251](save.c#L5250-L5251) - Added logging for cached account usage
- [comm.c:1444-1448](comm.c#L1444-L1448) - Added logging for refcount changes

**Changes**:
```c
// save.c:5246-5252
if (!str_cmp(acct->username, name)) {
    d->account = acct;
    acct->refcount++;  // NEW: Increment refcount for cached account
    iterator_stop(&it);
    log_stringf("load_account: Using cached account %s (refcount now %d)",
               acct->username, acct->refcount);  // NEW: Logging
    return true;
}

// comm.c:1443-1451
dclose->account->refcount--;
log_stringf("close_socket: Account %s refcount decreased to %d",
           dclose->account->username, dclose->account->refcount);  // NEW: Logging
if (dclose->account->refcount <= 0) {
    log_stringf("close_socket: Freeing account %s (refcount %d)",
               dclose->account->username, dclose->account->refcount);  // NEW: Logging
    list_remlink(loaded_accounts, dclose->account, false);
    free_account(dclose->account);
}
```

**Impact**: This bug could cause:
- Memory leaks (accounts never freed if refcount too high)
- Crashes (accounts freed prematurely if refcount too low)
- Incorrect account lifetime management

### ✅ VERIFIED: No Vault Accumulation (Fix #2 Not Needed)

**Analysis**: The cached account path at [save.c:5246-5252](save.c#L5246-L5252) returns BEFORE vault loading code is reached at [save.c:5316](save.c#L5316). Therefore, vault items are NOT reloaded and CANNOT accumulate.

**Vault Loading Path**:
1. Check cached accounts (5241-5256) → If found, return early
2. Create new account (5256-5269)
3. Load from disk (5290-5417) → Only executed if account NOT cached
4. Vault section (5315-5356) → Only executed if loading from disk

**Conclusion**: Vault accumulation is NOT an issue.

### ⏸️ DEFERRED: Stale Metadata (Fix #3)

**Status**: Not a memory leak, data integrity issue only.
**Decision**: Monitor in production before implementing.
**Workaround**: If stale data becomes an issue, implement account cache invalidation or reload-on-demand.

### ⏸️ DEFERRED: Duplicate Prevention (Fix #4)

**Status**: Existing reconnect logic at [nanny.c:2437-2496](nanny.c#L2437-L2496) and [comm.c:2741-2744](comm.c#L2741-L2744) handles this correctly.
**Conclusion**: Optimization not needed, current code is safe.

---

## Priority

**HIGH**: ✅ Fix #1 (Refcount) - **COMPLETED** - Critical bug that could cause crashes or memory leaks
**N/A**: ~~Fix #2 (Vault Items)~~ - Analysis shows this is not an issue
**MEDIUM**: Fix #3 (Stale Metadata) - Data integrity issue but not a leak - **DEFERRED**
**LOW**: Fix #4 (Duplicate Prevention) - Optimization, existing reconnect logic handles it - **DEFERRED**

---

## Remaining Investigation

If performance issues persist after the refcount fix, investigate:

1. **Character inventory loading** - Each `load_char_obj()` call loads full inventory from disk
2. **Reconnect double-loading** - Brief window where both new and existing character have inventories loaded
3. **Database query performance** - Room lookups, object indexing over time
4. **List iteration performance** - `loaded_chars` and `loaded_accounts` scanned linearly
5. **Memory fragmentation** - Long-running process may have fragmented heap

## Next Steps for Testing

1. **Deploy fix to test environment**
2. **Monitor logs** for refcount messages:
   ```bash
   tail -f log/current.log | grep "load_account:\|close_socket:"
   ```
3. **Run memory leak test** - Login/logout 100 times, check memory usage
4. **Monitor production** - Watch for improved performance over time
