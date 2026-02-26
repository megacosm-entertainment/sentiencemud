# Flagset Migration Completion Plan

## Current Status
✅ Phase 1-5: All flag constants converted to strings
✅ Phase 6 (mostly complete): Most array accesses converted to flagset pointers
⚠️ Remaining work: Church log categories and cleanup

## Remaining Tasks

### Task 1: Convert Church Log Categories (church.c)
**Estimated time:** 30-45 minutes

Church log categories currently use `flag_t` (unsigned long) bitmasks. These need to be converted to `flagset_t`.

#### Files to modify:
- `merc.h` - Update structures
- `church.c` - Update function signatures and implementations

#### Step-by-step:

1. **In merc.h:**
   - Find the CHURCH_LOG_ENTRY structure (search for "categories")
   - Change: `flag_t categories;` → `flagset_t categories;`
   - Find CHAR_DATA structure (search for "temp_log_category")
   - Change: `flag_t temp_log_category;` → `flagset_t temp_log_category;`

2. **In church.c:**

   a. Update function signatures:
   ```c
   // OLD:
   void add_church_log_entry(CHURCH_DATA *church, char *author, char *text, flag_t categories, bool system_generated);

   // NEW:
   void add_church_log_entry(CHURCH_DATA *church, char *author, char *text, const flagset_t *categories, bool system_generated);
   ```

   b. Update `is_meta_category()`:
   ```c
   // OLD:
   bool is_meta_category(flag_t category_flag)

   // NEW:
   bool is_meta_category(const char *category_name)
   {
       // Change implementation to check if category_name is one of the meta categories
       // by comparing strings instead of checking bits
   }
   ```

   c. Update search function:
   ```c
   // OLD:
   void do_church_log_search(CHAR_DATA *ch, char *search_text, char *search_author, flag_t search_categories)

   // NEW:
   void do_church_log_search(CHAR_DATA *ch, char *search_text, char *search_author, const flagset_t *search_categories)
   ```

   d. Update local variable declarations:
   ```c
   // OLD:
   flag_t category = flag_value(church_log_category_flags, arg2);
   flag_t search_categories = 0;

   // NEW:
   flagset_t search_categories;
   flagset_init(&search_categories);
   // Use flagset_set() to add categories
   ```

   e. Update flag comparisons:
   ```c
   // OLD:
   if (entry->categories & search_categories)

   // NEW:
   if (flagset_contains_any(&entry->categories, search_categories))
   ```

   f. Update flag setting:
   ```c
   // OLD:
   ch->temp_log_category = category;

   // NEW:
   flagset_init(&ch->temp_log_category);
   flagset_set(&ch->temp_log_category, category_name, church_log_category_flags);
   ```

3. **Initialization/Cleanup:**
   - Find where CHURCH_LOG_ENTRY structures are allocated
   - Add `flagset_init(&entry->categories);` after allocation
   - Find where they're freed
   - Add `flagset_free(&entry->categories);` before freeing
   - Do the same for CHAR_DATA's temp_log_category in nanny.c or wherever characters are created/freed

4. **Save/Load:**
   - In church save/load functions, convert:
   ```c
   // OLD:
   fprintf(fp, "Categories %ld\n", entry->categories);
   entry->categories = fread_number(fp);

   // NEW:
   fprintf(fp, "Categories %s~\n", flagset_to_string(&entry->categories));
   flagset_from_string(&entry->categories, fread_string(fp), church_log_category_flags);
   ```

### Task 2: Remove flag_t typedef (merc.h)
**Estimated time:** 5 minutes

Once church.c is converted:

1. Open `merc.h`
2. Find line 84: `typedef unsigned long flag_t;`
3. Delete it or comment it out
4. Compile to verify no errors: `make clean && make`

### Task 3: Clean up backup files
**Estimated time:** 2 minutes

```bash
cd /sentience/src
rm -f *.bak* editors/*/*.backup
git status
```

### Task 4: Final verification
**Estimated time:** 10 minutes

1. **Compile clean:**
   ```bash
   make clean
   make 2>&1 | tee build.log
   ```
   - Should have NO warnings or errors

2. **Search for any remaining old patterns:**
   ```bash
   # Should return NOTHING except tables.h meta-category definitions:
   grep -r "flag_t " --include="*.c" --include="*.h" . | grep -v "\.bak"

   # Should return NOTHING:
   grep -r "flag\[" --include="*.c" --include="*.h" .

   # Should return NOTHING (checking old bitvector operations):
   grep -r "IS_SET.*," --include="*.c" . | grep -v "//" | head -20
   ```

3. **Test the game:**
   - Start the server
   - Test character creation (uses many flags)
   - Test church log system
   - Test setting room/mob/obj flags in OLC
   - Save and reload data

### Task 5: Commit everything
**Estimated time:** 5 minutes

```bash
git add -A
git commit -m "Phase 6 complete: Flagset migration finished

- Converted church log categories from flag_t to flagset_t
- Removed flag_t typedef
- Cleaned up backup files
- All flag operations now use flagset_t with string-based flags"

# Optional: Merge to master if ready
git checkout master
git merge auth_refactor
```

## Common Patterns Reference

### Converting flag checks:
```c
// OLD:
if (IS_SET(obj->extra_flags[0], ITEM_GLOW))

// NEW (pointer):
if (flagset_isset(&obj->extra_flags, "glow", extra_flags))

// NEW (array element - rare):
if (flagset_isset(&array[i].flags, "glow", extra_flags))
```

### Converting flag setting:
```c
// OLD:
SET_BIT(obj->extra_flags[0], ITEM_GLOW);

// NEW:
flagset_set(&obj->extra_flags, "glow", extra_flags);
```

### Converting flag removal:
```c
// OLD:
REMOVE_BIT(obj->extra_flags[0], ITEM_GLOW);

// NEW:
flagset_remove(&obj->extra_flags, "glow", extra_flags);
```

### Converting flag initialization:
```c
// OLD:
obj->extra_flags[0] = 0;
obj->extra_flags[1] = 0;

// NEW:
flagset_init(&obj->extra_flags);
```

## Troubleshooting

### If you get compilation errors about flag_t:
- You missed converting something in church.c or tables.h
- Search for the error location and apply the patterns above

### If you get warnings about const:
- Add `const` to flagset_t* parameters that are only read, not modified
- Parameters to flagset_isset(), flagset_contains_any(), etc. should be `const flagset_t*`

### If the game crashes on startup:
- Likely missing flagset_init() somewhere
- Run under gdb: `gdb ./sentience` then `run` to see where it crashes
- Look for structures being allocated without flagset_init()

### If data doesn't save/load correctly:
- Check save/load functions use flagset_to_string() and flagset_from_string()
- Make sure the flag table pointer is correct (e.g., extra_flags, act_flags)

## Flag Tables Reference

Make sure you use the correct table for each flag type:

- **extra_flags** - Object flags (ITEM_*)
- **act_flags** - Mobile flags (ACT_*)
- **affect_flags** - Character affects (AFF_*)
- **room_flags** - Room flags (ROOM_*)
- **wear_flags** - Wear locations (ITEM_WEAR_*)
- **form_flags** - Body forms (FORM_*)
- **part_flags** - Body parts (PART_*)
- **imm_flags** - Immunities (IMM_*)
- **res_flags** - Resistances (RES_*)
- **vuln_flags** - Vulnerabilities (VULN_*)
- **off_flags** - Offensive behaviors (OFF_*)
- **church_log_category_flags** - Church log categories

All these tables are defined in tables.c and declared in tables.h.

## Validation Checklist

Before considering the migration complete:

- [ ] No files reference flag_t except possibly tables.h for meta-categories
- [ ] No files use flag[0] or flag[1] array syntax
- [ ] No files use IS_SET with comma-separated arguments
- [ ] No files use SET_BIT or REMOVE_BIT
- [ ] make clean && make produces no errors
- [ ] Game starts without crashes
- [ ] Characters can be created and saved
- [ ] OLC flag editing works
- [ ] Church log system works (if applicable)
- [ ] flag_t typedef is removed from merc.h
- [ ] All .bak files are deleted
- [ ] Changes are committed to git

## Time Estimate

Total time to complete: **1-2 hours**

- Task 1 (church.c): 30-45 minutes
- Task 2 (remove typedef): 5 minutes
- Task 3 (cleanup): 2 minutes
- Task 4 (verification): 10 minutes
- Task 5 (commit): 5 minutes
- Buffer for troubleshooting: 15-30 minutes

## Notes

- You don't need AI assistance for this - it's all mechanical search-and-replace
- Work methodically through one file at a time
- Compile frequently to catch errors early
- The patterns are consistent - once you do the first few, the rest are identical
- If you get stuck, refer to already-converted files like handler.c or save.c for examples
