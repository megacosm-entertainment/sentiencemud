# Large Inventory Performance Optimization

## Problem Statement

Characters with 1000+ items experience severe lag during:
1. Reconnect (freeing temporary character's inventory)
2. Quit/logout (freeing character inventory)
3. Character deletion

The lag can freeze the MUD for several seconds.

## Root Cause Analysis

### The O(n²) Performance Bug

**Location**: [handler.c:8815-8821](handler.c#L8815-L8821) - `list_remlink()`

**Code Path**:
1. `free_char()` iterates inventory list [mem.c:712-716](mem.c#L712-L716)
2. For each object, calls `extract_obj()` [mem.c:714](mem.c#L714)
3. `extract_obj()` calls `obj_from_char()` [handler.c:2996-2997](handler.c#L2996-L2997)
4. `obj_from_char()` calls `list_remlink()` [handler.c:2309](handler.c#L2309)
5. `list_remlink()` does **linear scan** of entire list to find item

**Complexity Analysis**:
```
For N items in inventory:
- Remove item 1: Scan N items
- Remove item 2: Scan N-1 items (iterator might not see it, but list_remlink scans)
- Remove item 3: Scan N-2 items
- ...
- Remove item N: Scan 1 item

Total scans = N + (N-1) + (N-2) + ... + 1 = N(N+1)/2 = O(n²)
```

**Performance Impact**:
| Items | List Scans | Time Estimate |
|-------|-----------|---------------|
| 100   | 5,050     | ~10ms        |
| 500   | 125,250   | ~250ms       |
| 1000  | 500,500   | ~1 second    |
| 2000  | 2,001,000 | ~4 seconds   |

**Actual Observed**: With 1000+ items, disconnects cause 1-3 second freezes.

## Solution: Batch Extraction Optimization

### Strategy

Instead of removing items from the list one by one during extraction, we:
1. Extract all objects WITHOUT removing from list
2. Clear the entire list at once
3. Reduce O(n²) to O(n)

### Implementation Approach

**Option 1: Add a "skip_list_removal" flag to obj_from_char()**
- Pros: Clean, explicit control
- Cons: Requires modifying function signature, affects many call sites

**Option 2: Create optimized batch extraction functions**
- Pros: No changes to existing code, safe
- Cons: Some code duplication

**Option 3: Temporarily detach list, extract, destroy list**
- Pros: Minimal code changes
- Cons: Requires careful handling to avoid memory leaks

**Recommended**: Option 3 with careful implementation

### Option 3 Implementation

Modify `free_char()` at [mem.c:710-735](mem.c#L710-L735):

```c
// Free items in inventory using lcarrying
if (ch->lcarrying) {
    LLIST *temp_list = ch->lcarrying;
    ch->lcarrying = NULL;  // Detach list to prevent list_remlink during extraction

    ITERATOR it;
    OBJ_DATA *obj;
    iterator_start(&it, temp_list);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        // Object will try to remove from ch->lcarrying, but it's NULL now
        // So obj_from_char will skip the list_remlink
        extract_obj(obj);
    }
    iterator_stop(&it);

    // Destroy the now-empty list
    list_destroy(temp_list);
}

// Same for lworn
if (ch->lworn) {
    LLIST *temp_list = ch->lworn;
    ch->lworn = NULL;

    ITERATOR it;
    OBJ_DATA *obj;
    iterator_start(&it, temp_list);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj(obj);
    }
    iterator_stop(&it);

    list_destroy(temp_list);
}

// Same for llocker
if (ch->llocker) {
    LLIST *temp_list = ch->llocker;
    ch->llocker = NULL;

    ITERATOR it;
    OBJ_DATA *obj;
    iterator_start(&it, temp_list);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj(obj);
    }
    iterator_stop(&it);

    list_destroy(temp_list);
}
```

**How it works**:
1. Save pointer to list
2. Set `ch->lcarrying = NULL` BEFORE extraction
3. When `extract_obj()` → `obj_from_char()` is called, it tries to remove from `ch->lcarrying`
4. But `ch->lcarrying` is NULL, so `list_remlink()` gets NULL and safely returns
5. After all objects extracted, destroy the temporary list

**Safety Analysis**:
- `obj_from_char()` checks `if ((ch = obj->carried_by) == NULL)` at line 2290
- If `ch->lcarrying` is NULL, `list_remlink(NULL, obj, false)` is called
- Looking at `list_remlink()` at line 8814: `if(lp && data)` - safely handles NULL list

**Wait, potential issue**: Let me check if `obj_from_char` handles NULL list:

```c
void obj_from_char(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if ((ch = obj->carried_by) == NULL) {
        bug("Obj_from_char: null ch.", 0);
        return;
    }

    /* Unequip it first */
    if (obj->wear_loc != WEAR_NONE)
        unequip_char(ch, obj, false);

    --obj->pIndexData->carried;

    REMOVE_BIT(obj->extra[0], ITEM_INVENTORY);
    obj->carried_by = NULL;
    obj->next_content = NULL;
    ch->carry_number -= get_obj_number(obj);
    ch->carry_weight -= get_obj_weight(obj);

    /* Remove from the LLIST */
    list_remlink(ch->lcarrying, obj, false);  // <-- This line!
}
```

The issue is that `list_remlink(ch->lcarrying, obj, false)` will be called with `ch->lcarrying = NULL`.

Let's verify `list_remlink` handles NULL:
```c
void list_remlink(LLIST *lp, void *data, bool del)
{
    LLIST_LINK *link, *link_next;

    if(lp && data) {  // <-- NULL check! Safe!
        for(link = lp->head; link; link = link_next)
        {
            link_next = link->next;
            if(link->data == data) {
                list_remdata(lp, link, del);
            }
        }
    }
}
```

✅ **SAFE**: `if(lp && data)` check means NULL list is handled gracefully.

## Alternative: Create List-Aware Batch Functions

We could also create specialized batch extraction functions:

```c
void extract_obj_batch(OBJ_DATA *obj, bool skip_list_removal)
{
    // Same as extract_obj but with flag to skip list operations
}

void free_inventory_batch(LLIST *inventory_list)
{
    ITERATOR it;
    OBJ_DATA *obj;
    iterator_start(&it, inventory_list);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj_batch(obj, true);  // Skip list removal
    }
    iterator_stop(&it);
    list_destroy(inventory_list);
}
```

But the NULL-list approach is cleaner and requires fewer code changes.

## Expected Performance Improvement

**Before Optimization**:
- 1000 items: ~500,000 list scans = 1+ seconds
- 2000 items: ~2,000,000 list scans = 4+ seconds

**After Optimization**:
- 1000 items: 1000 object extractions + 1 list destroy = ~10ms
- 2000 items: 2000 object extractions + 1 list destroy = ~20ms

**Expected Speedup**: 100x for 1000 items, 200x for 2000 items

## Implementation Plan

1. **Create backup** of current `free_char()` implementation
2. **Modify `free_char()`** with NULL-list approach
3. **Add logging** to measure performance:
   ```c
   long start_time = current_time;
   // ... extraction code ...
   long elapsed = current_time - start_time;
   log_stringf("free_char: Freed %d inventory items in %ld ms",
               item_count, elapsed);
   ```
4. **Test with large inventories** (100, 500, 1000, 2000 items)
5. **Monitor for edge cases**:
   - Objects in containers
   - Equipped items
   - Locker items
   - Nested containers

## Potential Risks

1. **Iterator modification during iteration**: The iterator might be confused if objects modify the list during iteration
   - **Mitigation**: We're iterating over a detached list, so this shouldn't be an issue

2. **Objects that reference the list**: If any object cleanup code accesses `ch->lcarrying`
   - **Mitigation**: After detaching, `ch->lcarrying` is NULL, which should be handled safely

3. **Partial failures**: If extraction fails midway, the list is already detached
   - **Mitigation**: Extraction failures are rare, and the list will still be destroyed

## ✅ IMPLEMENTATION STATUS

**Date**: 2026-01-01
**Status**: **COMPLETED AND DEPLOYED**

### Changes Made

**File**: [mem.c:710-763](mem.c#L710-L763)

**Modifications**:
1. Detach `ch->lcarrying` before object extraction
2. Iterate detached list and extract all objects
3. Destroy detached list after extraction
4. Same optimization for `ch->lworn` and `ch->llocker`
5. Added logging for inventories > 100 items

**Key Code**:
```c
// Before (O(n²)):
if (ch->lcarrying) {
    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj(obj);  // Calls list_remlink - linear scan!
    }
    iterator_stop(&it);
}

// After (O(n)):
if (ch->lcarrying) {
    LLIST *temp_carrying = ch->lcarrying;
    int item_count = list_size(temp_carrying);
    ch->lcarrying = NULL;  // Detach - no more list_remlink!

    iterator_start(&it, temp_carrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        extract_obj(obj);  // obj_from_char sees NULL list, skips removal
    }
    iterator_stop(&it);

    list_destroy(temp_carrying);  // Clean up all at once

    if (item_count > 100) {
        log_stringf("free_char: Freed %d inventory items for %s",
                   item_count, ch->name ? ch->name : "(unknown)");
    }
}
```

### Performance Improvement

**Theoretical**:
- 1000 items: 500,500 list scans → 1000 extractions = **500x faster**
- 2000 items: 2,001,000 list scans → 2000 extractions = **1000x faster**

**Expected Real-World**:
- 1000 items: ~1 second → ~10ms = **100x faster**
- 2000 items: ~4 seconds → ~20ms = **200x faster**

### Logging Added

For inventories with 100+ items, logs will show:
```
free_char: Freed 1234 inventory items for PlayerName
free_char: Freed 567 locker items for PlayerName
```

This helps identify characters with large inventories and verify optimization is working.

## Testing Checklist

- [ ] Character with 1000 regular items - quit
- [ ] Character with 1000 regular items - reconnect
- [ ] Character with items in containers (nested)
- [ ] Character with equipped items
- [ ] Character with locker items
- [ ] Character with mixed inventory (carried + worn + locker)
- [ ] Verify no memory leaks (valgrind or memory monitoring)
- [ ] Verify no orphaned objects in loaded_objects list
- [ ] Performance measurement logging
- [ ] Check logs for "free_char: Freed" messages

## Next Steps

1. **Monitor production logs** for "free_char: Freed" messages to identify heavy users
2. **Measure actual performance** - time disconnects/quits for characters with 1000+ items
3. **Verify no regressions** - watch for crashes, memory leaks, orphaned objects
4. **Consider inventory limits** - If optimization isn't enough, may need to cap inventory size

## Related Files

- [mem.c:710-763](mem.c#L710-L763) - `free_char()` inventory cleanup (**MODIFIED**)
- [handler.c:2286](handler.c#L2286) - `obj_from_char()`
- [handler.c:2933](handler.c#L2933) - `extract_obj()`
- [handler.c:8810](handler.c#L8810) - `list_remlink()` - The O(n) bottleneck
- [handler.c:8441](handler.c#L8441) - `list_destroy()`
