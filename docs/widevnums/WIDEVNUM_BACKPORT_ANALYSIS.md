# Wide Vnum (WVNUM) Backport Analysis

## Executive Summary

The "widevnum" or "wvnum" feature is a fundamental architectural change implemented in `/sentience/src_20_dev` that transitions from a **global vnum namespace** to **area-scoped vnums**. This change affects nearly every aspect of the codebase and represents one of the largest architectural improvements in the 2.0 development branch.

**Status:** Not present in current `/sentience/src` - requires backport
**Complexity:** VERY HIGH - touches 82+ files, 20k+ lines of changes
**Impact:** Fundamental - affects areas, objects, mobiles, rooms, scripts, dungeons, ships, blueprints

## What is WVNUM?

### Current System (src/)
- **Global Vnum Space**: All vnums are globally unique across the entire MUD
- **Format**: Single number (e.g., `6511`)
- **Range**: 1 to MAX_INT (2,147,483,647) globally
- **Problems**:
  - Vnum conflicts between areas
  - Difficulty moving/copying areas
  - Can't have same vnum in different areas
  - Hard to manage large numbers of areas
  - Area isolation is poor

### Wide Vnum System (src_20_dev/)
- **Area-Scoped Vnum Space**: Each area has its own vnum range (1 to MAX_INT)
- **Format Options**:
  - `#1234` - Relative (uses current/local area being worked on)
  - `923#1234` - Absolute with area UID
  - `Plith#1234` - Area name (builder-friendly!)
  - `'Multi Word Area'#1234` - Quoted area name
- **Structure**:
  ```c
  typedef struct {
      AREA_DATA *pArea;  // Pointer to the area
      long vnum;         // Local vnum within that area
  } WNUM;
  ```
- **Load Format**:
  ```c
  typedef struct {
      long auid;  // Area UID for persistent storage
      long vnum;  // Local vnum
  } WNUM_LOAD;
  ```

### Benefits of WVNUM
1. **Area Portability**: Areas can be moved/copied without vnum conflicts
2. **Vnum Reuse**: Same local vnum can exist in different areas
3. **Scalability**: Effectively unlimited vnums (MAX_INT per area)
4. **Isolation**: Areas are more self-contained
5. **Flexible Syntax**: 
   - `#1000` (relative to current/local area being worked on)
   - `1#1000` (area UID 1)
   - `Plith#1000` (area name - no need to remember UIDs!)
6. **Builder-Friendly**: Use familiar area names instead of numeric UIDs
7. **Organization**: Simplifies area management and OLC editors

## Core Structure Changes

### WNUM Structure
```c
// Runtime representation
typedef struct {
    AREA_DATA *pArea;    // Area pointer (runtime)
    long vnum;           // Local vnum within area
} WNUM;

// Persistent storage representation
typedef struct {
    long auid;           // Area UID (for save files)
    long vnum;           // Local vnum
} WNUM_LOAD;
```

### Key Functions
```c
// Parsing
bool parse_widevnum(char *argument, AREA_DATA *current_area, WNUM *wnum);

// Formatting for display/save
const char *widevnum_string(AREA_DATA *pArea, long vnum, AREA_DATA *pRefArea);
const char *widevnum_string_wnum(WNUM wnum, AREA_DATA *pRefArea);
const char *widevnum_string_mobile(MOB_INDEX_DATA *mob, AREA_DATA *pRefArea);
const char *widevnum_string_object(OBJ_INDEX_DATA *obj, AREA_DATA *pRefArea);
const char *widevnum_string_room(ROOM_INDEX_DATA *room, AREA_DATA *pRefArea);

// Loading from files
WNUM fread_widevnum(FILE *fp, AREA_DATA *pArea);
WNUM_LOAD *fread_widevnumptr(FILE *fp, long refAuid);

// Index lookups
MOB_INDEX_DATA *get_mob_index(AREA_DATA *pArea, long vnum);
OBJ_INDEX_DATA *get_obj_index(AREA_DATA *pArea, long vnum);
ROOM_INDEX_DATA *get_room_index(AREA_DATA *pArea, long vnum);
SCRIPT_DATA *get_script_index(AREA_DATA *pArea, long vnum, int type);

// Convenience wrappers
OBJ_INDEX_DATA *get_obj_index_wnum(WNUM wnum);
MOB_INDEX_DATA *get_mob_index_wnum(WNUM wnum);
ROOM_INDEX_DATA *get_room_index_wnum(WNUM wnum);
```

## Affected Systems

### 1. Index Data Structures
**Impact:** CRITICAL - Core data model changes

**Changes:**
- All `*_INDEX_DATA` structures now include `AREA_DATA *area`
- Hash tables moved from global to per-area:
  ```c
  // OLD (src/):
  extern MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
  extern OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
  extern ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];
  
  // NEW (src_20_dev/):
  struct area_data {
      // ... other fields ...
      MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
      OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
      ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];
      SCRIPT_DATA *mprog_hash[MAX_KEY_HASH];
      SCRIPT_DATA *oprog_hash[MAX_KEY_HASH];
      SCRIPT_DATA *rprog_hash[MAX_KEY_HASH];
      SCRIPT_DATA *tprog_hash[MAX_KEY_HASH];
      SCRIPT_DATA *iprog_hash[MAX_KEY_HASH];
      SCRIPT_DATA *dprog_hash[MAX_KEY_HASH];
      BLUEPRINT *blueprint_hash[MAX_KEY_HASH];
      BLUEPRINT_SECTION *blueprint_section_hash[MAX_KEY_HASH];
      DUNGEON_INDEX_DATA *dungeon_index_hash[MAX_KEY_HASH];
      SHIP_INDEX_DATA *ship_index_hash[MAX_KEY_HASH];
      // ... more ...
  };
  ```

### 2. Area Data Structure
**Impact:** CRITICAL - Adds per-area vnum tracking

**New Fields:**
```c
struct area_data {
    // Per-area hash tables (instead of global)
    MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
    OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
    ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];
    TOKEN_INDEX_DATA *token_index_hash[MAX_KEY_HASH];
    
    // Vnum range tracking for efficient searches
    // CRITICAL: Prevents scanning 1 to MAX_INT per area!
    long top_vnum_mob, bottom_vnum_mob;
    long top_vnum_obj, bottom_vnum_obj;
    long top_vnum_room, bottom_vnum_room;
    long top_vnum_token, bottom_vnum_token;
    
    // Script prog tracking
    long top_mprog_index, bottom_mprog_index;
    long top_oprog_index, bottom_oprog_index;
    long top_rprog_index, bottom_rprog_index;
    long top_tprog_index, bottom_tprog_index;
    long top_aprog_index, bottom_aprog_index;
    long top_iprog_index, bottom_iprog_index;
    long top_dprog_index, bottom_dprog_index;
    
    // Other entity tracking
    long top_blueprint_vnum, bottom_blueprint_vnum;
    long top_blueprint_section_vnum, bottom_blueprint_section_vnum;
    long top_dungeon_vnum, bottom_dungeon_vnum;
    long top_ship_vnum, bottom_ship_vnum;
    long top_reputation_vnum, bottom_reputation_vnum;
    long top_quest_vnum, bottom_quest_vnum;
    long top_region_uid;
```

**Performance & Security Critical Notes:**

### ⚠️ SECURITY VULNERABILITY: Vnum Range Tracking

**DO NOT IMPLEMENT** the `top_vnum_*` / `bottom_vnum_*` tracking from src_20_dev. This approach creates a **denial-of-service vulnerability** that malicious builders can exploit.

#### The Sparse Vnum Attack

A builder with `redit` access can weaponize the vnum range tracker:

```c
// Malicious builder creates:
redit 1              // Room vnum 1
redit 2000000000     // Room vnum 2,000,000,000

// Result: area->bottom_vnum_room = 1, area->top_vnum_room = 2000000000
// Any vnum search now iterates 2 BILLION vnums for just 2 rooms!
for (vnum = 1; vnum <= 2000000000; vnum++) {
    if ((room = get_room_index(vnum)))  // 2 billion hash lookups
        // Server hangs for minutes/hours
}
```

**Why it's unfixable:**
- Can't validate vnum gaps (legitimate areas might have sparse vnums)
- Can't restrict vnum choices (defeats widevnum purpose)
- Any two vnums create a range that must be scanned
- Attack requires only basic builder access

#### Secure Solution: Hash Table Iteration

Instead of iterating vnum ranges, **iterate the actual hash table**:

```c
// SECURE - Iterates actual entities, immune to sparse attacks
void do_vnum_obj(CHAR_DATA *ch, char *argument) {
    OBJ_INDEX_DATA *obj;
    int hash, found = 0;
    
    // Iterate hash table buckets (O(actual objects))
    for (hash = 0; hash < MAX_KEY_HASH; hash++) {
        for (obj = obj_index_hash[hash]; obj; obj = obj->next) {
            // Filter by area if needed
            if (target_area && obj->area != target_area)
                continue;
            
            if (is_name(argument, obj->name))
                found++;
        }
    }
}
```

**Benefits:**
- ✓ O(actual_entities) not O(vnum_range)
- ✓ Immune to sparse vnum attacks
- ✓ No range tracking needed (simpler code)
- ✓ Works with any vnum allocation pattern
- ✓ Consistent performance regardless of vnum distribution

**Trade-offs:**
- Iterates all entities in hash table (add area filter: `obj->area == target_area`)
- Still very fast: O(thousands) not O(billions)

#### Implementation Requirements

1. **Update vnum search commands** (Phase 4):
   - `do_vnum_obj()` - Iterate `obj_index_hash[]`
   - `do_vnum_mob()` - Iterate `mob_index_hash[]`
   - `do_vnum_room()` - Iterate `room_index_hash[]`

2. **Update OLC validation** (Phase 5):
   - Use hash iteration for duplicate checks
   - Filter by `entity->area` when needed

3. **Reference existing patterns**:
   - Current code already uses hash iteration (e.g., `do_areas()`)
   - Apply same pattern to vnum searches
    long bottom_ship_vnum, top_ship_vnum;
    
    // Per-area hash tables (see above)
    // ...
};
```

### 3. Blueprints
**Impact:** HIGH - Moved from global to area-scoped

**Changes:**
- `BLUEPRINT` now has `AREA_DATA *area` field
- Blueprint hash tables moved to per-area
- Blueprint loading/saving uses widevnums
- Commands updated: `bpedit`, `bsedit`

### 4. Dungeons
**Impact:** HIGH - Moved from global to area-scoped

**Changes:**
- `DUNGEON_INDEX_DATA` now has `AREA_DATA *area` field
- Dungeon hash tables moved to per-area
- Dungeon loading/saving uses widevnums
- Commands updated: `dungeonedit`, instance management

### 5. Ships/Boats
**Impact:** HIGH - Moved from global to area-scoped

**Changes:**
- `SHIP_INDEX_DATA` now has `AREA_DATA *area` field
- Ship hash tables moved to per-area
- Ship loading/saving uses widevnums
- Commands updated: `shedit`

### 6. Scripts (MProgs, OProgs, RProgs, etc.)
**Impact:** CRITICAL - Extensive changes

**Changes:**
- All script types now area-scoped
- New entity type in scripting: `ENT_WIDEVNUM`
- Parsing functions accept WVNUM format
- Script commands updated:
  - `MLOAD`, `OLOAD`, `GOTO`, etc. now use widevnums
  - Can reference cross-area entities with `auid#vnum`
  - Can use relative references with `#vnum`
- Script IFC (conditionals):
  - `ISWNUM $ENTITY $AREA $NUMBER[$NUMBER]` - Check if entity is in area/vnum range
  - `WNUMVALID $WIDEVNUM` - Validate widevnum
- Script variables can store WVNUM types

**Example Script Changes:**
```
# OLD format (src/):
MLOAD 1001

# NEW format (src_20_dev/):
MLOAD #1001           # Relative to current area
MLOAD 1#1001          # Absolute (area 1, vnum 1001)
MLOAD $some_var       # Variable containing WVNUM
```

### 7. OLC Editors
**Impact:** CRITICAL - All editors updated

**Affected Editors:**
- `aedit` - Area editor (base system)
- `redit` - Room editor (uses widevnums for exits)
- `medit` - Mobile editor (area-scoped creation)
- `oedit` - Object editor (area-scoped creation)
- `mpedit`, `opedit`, `rpedit`, `tpedit`, `ipedit`, `dpedit` - Script editors
- `bpedit`, `bsedit` - Blueprint editors
- `shedit` - Ship editor
- `dedit` - Dungeon editor

**Key Changes:**
- Vnum input accepts `auid#vnum` or `#vnum` format
- Creation commands auto-assign from area's vnum space
- List commands show relative vnums when in same area
- Cross-area references show full `auid#vnum`

### 8. Database Loading/Saving
**Impact:** CRITICAL - File format changes

**Changes:**
- Area files now save/load widevnums for cross-references
- Format: `auid#vnum` in files
- Backward compatibility considerations needed
- New bootstrap process when no areas exist

**Example File Format:**
```
# OLD (src/):
#OBJECT
Vnum 6511
#RESET
M 0 1001 1 1000     # Mob 1001 in room 1000

# NEW (src_20_dev/):
#OBJECT
Vnum 6511           # Local vnum
#RESET
M 0 #1001 1 1#1000  # Mob from this area, room from area 1
```

### 9. Commands
**Impact:** HIGH - Many commands updated

**Affected Commands:**
- `goto <wvnum>` - Travel to room by widevnum
- `transfer <player> <wvnum>` - Transfer to widevnum room
- `at <wvnum> <command>` - Execute at widevnum room
- `mload <wvnum>` - Load mobile by widevnum
- `oload <wvnum>` - Load object by widevnum
- `vnum <type> <name>` - Search (now area-aware)
- `stat <type> <wvnum>` - Stat by widevnum
- All OLC commands (see section 7)

### 10. Exits and Portals
**Impact:** HIGH - Cross-area linking

**Changes:**
- Exit destinations use WNUM
- Portal destinations use WNUM
- Wilderness vlinks use WNUM
- Can link rooms across areas: `link <dir> <auid#vnum>`

### 11. Reserved Entities
**Impact:** MEDIUM - Architecture difference between src_20_dev and legacy

**IMPORTANT: Legacy uses different system than src_20_dev**

**src_20_dev approach (NOT to be backported):**
```c
// Global pointers resolved at boot
OBJ_INDEX_DATA *obj_index_portal = NULL;
MOB_INDEX_DATA *mob_index_death = NULL;
ROOM_INDEX_DATA *room_index_limbo = NULL;
// ... 80+ global pointers

// RESERVED_WNUM structure for configuration
typedef struct {
    const char *name;
    long auid;
    long vnum;
    WNUM *wnum;
    void *data;  // Pointer to global variable
} RESERVED_WNUM;

// Resolved during boot
void resolve_reserved_objs() {
    for(i = 0; reserved_obj_wnums[i].name; i++) {
        wnum->pArea = get_area_from_uid(reserved[i].auid);
        wnum->vnum = reserved[i].vnum;
        if (reserved[i].data) {
            OBJ_INDEX_DATA **ppObj = (OBJ_INDEX_DATA **)reserved[i].data;
            *ppObj = get_obj_index(wnum->pArea, wnum->vnum);
        }
    }
}
```

**Legacy approach (ALREADY IMPLEMENTED - keep this):**
```c
// Name-based lookup system using LLIST
typedef struct reserved_data {
    char *name;            // e.g., "obj_skull_normal"
    int type;              // RESERVED_OBJ, RESERVED_MOB, etc.
    int id;                // The vnum (or uid for areas)
    bool removable;
    char *description;
} RESERVED_DATA;

extern LLIST *reserved_vnums;  // Global list

// Lookup functions
int get_reserved_vnum(const char *name);
OBJ_INDEX_DATA *get_reserved_obj_index(const char *name);
MOB_INDEX_DATA *get_reserved_mob_index(const char *name);
ROOM_INDEX_DATA *get_reserved_room_index(const char *name);

// Usage in code:
if (obj->pIndexData->vnum == get_reserved_vnum("obj_skull_normal")) {
    // Handle skull
}
```

**Migration Strategy:**

1. **DO NOT backport src_20_dev's RESERVED_WNUM system**
   - Legacy's name-based system is superior
   - Avoids 80+ global pointer variables
   - More flexible (can add reserved items in-game via editor)
   - Persists to file, no code changes needed

2. **Update legacy's reserved system for widevnums:**
   ```c
   // Change RESERVED_DATA structure
   typedef struct reserved_data {
       char *name;
       int type;
       WNUM wnum;           // ADD THIS - replace 'int id'
       bool removable;
       char *description;
   } RESERVED_DATA;
   
   // Update lookup functions to return WNUM
   WNUM get_reserved_wnum(const char *name);
   OBJ_INDEX_DATA *get_reserved_obj_index(const char *name) {
       WNUM wnum = get_reserved_wnum(name);
       if (wnum.pArea) {
           return get_obj_index(wnum.pArea, wnum.vnum);
       }
       return NULL;
   }
   ```

3. **Update reserved file format:**
   ```
   #RESERVED
   Name obj_skull_normal~
   Type obj
   AreaUID 923
   Vnum 1234
   Removable 0
   Description Normal skull item~
   #-RESERVED
   ```

4. **Benefits of legacy approach:**
   - No global variables cluttering namespace
   - Reserved items manageable via in-game editor
   - Easy to add new reserved items without code changes
   - Can relocate reserved items to different areas
   - Better encapsulation

**Key Difference:**
- src_20_dev: Compile-time resolution via global pointers
- Legacy: Runtime resolution via name lookup
- **Legacy approach is more flexible and maintainable**

**Implementation:**
- Phase 3 (Database): Update RESERVED_DATA structure to use WNUM
- Phase 3 (Database): Update reserved file format to store area UID + vnum
- Phase 3 (Database): Update all get_reserved_*() functions
- Phase 6 (OLC): Update reserved editor for widevnum syntax
- Keep all existing name-based lookup infrastructure

### 12. Churches
**Impact:** MEDIUM - Made area-relocatable

**Changes:**
- Churches can be in any area with `AREA_CHURCH` flag
- Church data references use WVNUM
- Can relocate entire church to different area

## File-by-File Analysis

### Critical Files (Major Changes)
1. **merc.h** - Core structure definitions
   - Added WNUM and WNUM_LOAD structures
   - Updated AREA_DATA with per-area hash tables
   - Updated all index structures with area pointers

2. **db.c** - Database loading
   - New parse functions: `parse_widevnum()`, `fread_widevnum()`, `fread_widevnumptr()`
   - New format functions: `widevnum_string()` family
   - Updated all index getters: `get_*_index()` now take AREA_DATA parameter
   - Bootstrap process for empty database

3. **db2.c** - Secondary database functions
   - Updated quest system for widevnums
   - Updated random object/mob selection

4. **olc.c** - OLC core
   - All editors updated for widevnum input
   - List commands show relative vnums
   - Creation uses area vnum space

5. **olc_save.c** - OLC saving
   - Save format uses widevnums
   - Area migration/versioning support

6. **scripts.c** - Scripting engine
   - ENT_WIDEVNUM entity type
   - WVNUM parsing and handling
   - All script commands updated

7. **script_*.c** - Script command files
   - MLOAD, OLOAD, TLOAD, GOTO, etc. updated
   - Cross-area entity references

8. **handler.c** - Core handlers
   - Updated object/mob lookup functions
   - Room finding functions

9. **act_wiz.c** - Wizard commands
   - GOTO, TRANSFER, AT, STAT updated
   - MLOAD, OLOAD, MWHERE, OWHERE updated

10. **blueprint.c** - Blueprint system
    - Moved to per-area system
    - Hash tables per-area
    - WVNUM references

11. **dungeon.c** - Dungeon system
    - Moved to per-area system
    - Hash tables per-area
    - WVNUM references

12. **boat.c** - Ship system
    - Moved to per-area system
    - Hash tables per-area
    - WVNUM references

### Medium Impact Files
- **act_enter.c** - Portal/boat entry (WVNUM destinations)
- **act_move.c** - Movement (WVNUM room references)
- **act_info.c** - Info commands (WVNUM display)
- **act_obj.c** - Object commands (WVNUM creation)
- **fight.c** - Combat (corpse vnums)
- **save.c** - Player save (WVNUM equipment/inventory)
- **update.c** - Area resets (WVNUM mob/obj loading)
- **wilds.c** - Wilderness (WVNUM vlinks)

### Lower Impact Files
- Various spell files (magic_*.c) - Object creation
- **quest.c** - Quest targets
- **music.c** - Song mob references
- **house.c** - House room references
- **church.c** - Church room references
- **autowar.c** - War room references

## Implementation Challenges

### 1. Data Migration
**Challenge:** Converting existing data from global to area-scoped vnums

**Issues:**
- All existing area files need conversion
- Player files reference vnums (equipment, corpses, etc.)
- Scripts reference vnums
- Quests reference vnums
- Persistent objects (houses, containers, etc.)

**Solution Approach:**
- Migration tool to convert area files
- Player file migration on load
- Script auto-conversion or manual review
- Phased migration with compatibility layer

### 2. Backward Compatibility
**Challenge:** Supporting old format during transition

**Issues:**
- Old clients/scripts expect old format
- Mixed old/new areas during transition
- Player confusion

**Solution Approach:**
- Parse both formats (simple number = global, `#num` or `uid#num` = wide)
- Conversion tool for areas
- Documentation and player notification
- Test server for validation

### 3. Cross-Area References
**Challenge:** Managing references between areas

**Issues:**
- What if referenced area isn't loaded?
- Area load order matters
- Circular dependencies

**Solution Approach:**
- Lazy loading of referenced areas
- Deferred reference resolution
- Error handling for missing references
- Area dependency tracking

### 4. Hash Table Migration
**Challenge:** Moving global hash tables to per-area

**Issues:**
- Performance implications
- Search algorithms need updates
- Memory layout changes

**Solution Approach:**
- Benchmarking before/after
- Optimized per-area hash sizing
- Cache frequently accessed areas
- Iterator updates

### 5. Script Compatibility
**Challenge:** Thousands of existing scripts

**Issues:**
- Scripts use vnum format extensively
- Hard to auto-convert some references
- Testing is difficult

**Solution Approach:**
- Script analyzer tool
- Compatibility mode for old format
- Gradual migration per area
- Testing framework

### 6. OLC User Training
**Challenge:** Builders need to learn new format

**Issues:**
- Confusion about relative vs absolute
- When to use `#vnum` vs `uid#vnum`
- Cross-area references

**Solution Approach:**
- Clear documentation
- Help files with examples
- OLC hints and error messages
- Training sessions

## Migration Strategy

### Phase 1: Core Infrastructure (Weeks 1-4)
**Goal:** Implement WNUM structures and core functions

**Tasks:**
1. Add WNUM and WNUM_LOAD structures to merc.h
2. Implement parsing: `parse_widevnum()`
3. Implement formatting: `widevnum_string()` family
4. Implement loading: `fread_widevnum()`, `fread_widevnumptr()`
5. Update AREA_DATA structure with per-area hash tables
6. Create area UID assignment system
7. Write unit tests for WNUM functions using JSON testing framework (see [testing documentation](../testing/))

**Validation:**
- Code compiles
- WNUM parsing works correctly
- Format conversion works
- Area UIDs assigned correctly

### Phase 2: Index System Migration (Weeks 5-8)
**Goal:** Move index lookups to area-scoped

**Tasks:**
1. Update get_mob_index() to take AREA_DATA parameter
2. Update get_obj_index() to take AREA_DATA parameter
3. Update get_room_index() to take AREA_DATA parameter
4. Add get_*_index_wnum() convenience wrappers
5. Move hash tables from global to per-area
6. Update all callers (100+ files)
7. Add area UID validation

**Validation:**
- All index lookups work
- Hash tables populate correctly
- Performance acceptable
- No crashes

### Phase 3: Database Loading (Weeks 9-12)
**Goal:** Support WVNUM in area files

**Tasks:**
1. Update area file parser for WVNUM format
2. Add backward compatibility for old format
3. Update reset system for WVNUM
4. Update exit loading for WVNUM
5. Update script loading for WVNUM
6. Create area file conversion tool
7. Test with sample areas

**Validation:**
- Old format areas still load
- New format areas load correctly
- Conversion tool works
- No data loss

### Phase 4: Blueprint/Dungeon/Ship Migration (Weeks 13-16)
**Goal:** Move these systems to area-scoped

**Tasks:**
1. Update BLUEPRINT structures with area pointer
2. Move blueprint hash tables to per-area
3. Update DUNGEON_INDEX_DATA with area pointer
4. Move dungeon hash tables to per-area
5. Update SHIP_INDEX_DATA with area pointer
6. Move ship hash tables to per-area
7. Update all related commands
8. Update save/load functions

**Validation:**
- Blueprints work in-area and cross-area
- Dungeons load/save correctly
- Ships accessible from all areas
- No conflicts

### Phase 5: Scripting System (Weeks 17-22)
**Goal:** Add WVNUM support to scripting

**Tasks:**
1. Add ENT_WIDEVNUM entity type
2. Update script parser for WVNUM
3. Update MLOAD, OLOAD, TLOAD commands
4. Update GOTO, TRANSFER commands
5. Add ISWNUM, WNUMVALID conditionals
6. Update all script commands (100+)
7. Create script migration tool
8. Test extensively

**Validation:**
- Scripts parse WVNUM correctly
- Cross-area references work
- Old scripts still function
- New scripts more flexible

### Phase 6: OLC Editors (Weeks 23-28)
**Goal:** Update all OLC editors for WVNUM

**Tasks:**
1. Update REDIT for exit widevnums
2. Update MEDIT for area-scoped creation
3. Update OEDIT for area-scoped creation
4. Update script editors (MPEDIT, OPEDIT, RPEDIT, TPEDIT, IPEDIT, DPEDIT)
5. Update BPEDIT, BSEDIT
6. Update SHEDIT
7. Update DEDIT
8. Add help documentation
9. Train builders

**Validation:**
- All editors work with WVNUM
- Cross-area references possible
- Auto-vnum assignment works
- Builders can use effectively

### Phase 7: Commands (Weeks 29-32)
**Goal:** Update wizard and player commands

**Tasks:**
1. Update GOTO, TRANSFER, AT
2. Update MLOAD, OLOAD, TLOAD
3. Update STAT, MSTAT, OSTAT, RSTAT
4. Update VNUM search commands
5. Update MWHERE, OWHERE
6. Update portal/teleport code
7. Test all commands

**Validation:**
- Commands accept WVNUM format
- Cross-area commands work
- Error messages clear
- No crashes

### Phase 8: Reserved Entities (Weeks 33-34)
**Goal:** Implement dynamic reserved entities

**Tasks:**
1. Add RESERVED_WNUM system
2. Create reserved entity management commands
3. Update bootstrap process
4. Update death room, limbo, etc.
5. Document reserved entity system

**Validation:**
- Reserved entities configurable
- Bootstrap works on empty DB
- No hardcoded vnums remain

### Phase 9: Data Migration (Weeks 35-40)
**Goal:** Migrate existing game data

**Tasks:**
1. Backup entire database
2. Run area conversion tool on all areas
3. Validate converted areas
4. Create player file migration
5. Run player migration
6. Validate player data
7. Update live server

**Validation:**
- No data loss
- All areas load correctly
- Players have correct equipment
- No crashes
- Performance acceptable

### Phase 10: Documentation & Training (Weeks 41-44)
**Goal:** Document and train users

**Tasks:**
1. Write builder documentation
2. Write script migration guide
3. Create training materials
4. Hold builder training sessions
5. Update help files
6. Create FAQ
7. Monitor for issues

**Validation:**
- Builders understand system
- Documentation complete
- Few support requests
- System stable

## Estimated Effort

**Total Estimated Time:** 44 weeks (11 months)

**Breakdown:**
- Core implementation: 16 weeks
- Scripting & OLC: 12 weeks
- Commands & Migration: 12 weeks
- Documentation & Training: 4 weeks

**Team Requirements:**
- 1-2 senior C developers (full time)
- 1 QA tester (part time)
- 1 technical writer (part time)
- Builder volunteers for testing

**Risk Factors:**
- Data corruption during migration (HIGH)
- Performance degradation (MEDIUM)
- Builder resistance to change (MEDIUM)
- Script compatibility issues (HIGH)
- Timeline overrun (HIGH - complex project)

## Alternatives to Full Backport

### Option 1: Partial Implementation
Implement WVNUM for new systems only, leave old systems as-is.

**Pros:**
- Less risky
- Faster to implement
- Less disruption

**Cons:**
- Mixed system (confusing)
- Limited benefits
- Still need migration eventually

### Option 2: Incremental Migration
Migrate one system at a time over years.

**Pros:**
- Very low risk
- Can be done alongside other work
- Easy to rollback individual pieces

**Cons:**
- Very long timeline
- Inconsistent system for years
- Harder to maintain dual systems

### Option 3: Fresh Start
Start new codebase from src_20_dev.

**Pros:**
- Already has WVNUM
- Many other improvements
- Clean slate

**Cons:**
- May have other unwanted changes
- Requires full validation
- Historical changes may be lost

### Option 4: Do Nothing
Accept current global vnum system.

**Pros:**
- No effort required
- No risk
- Works currently

**Cons:**
- Miss out on benefits
- Scalability issues remain
- Area management stays difficult

## Recommendation

**UPDATED: User Decision - Fresh Start with New Areas**

**Recommended Approach:** Aggressive Full Backport with Clean Slate

**Key Decision:** User is willing to start with new areas rather than migrating existing ones. This dramatically simplifies the migration by eliminating backward compatibility concerns.

**Rationale:**
1. **Simplified Migration:** No need for dual-format support or conversion tools
2. **Clean Implementation:** Can focus on getting widevnum right, not compatibility layers
3. **Faster Timeline:** Eliminates most risky migration phases
4. **Better Architecture:** No technical debt from compatibility code
5. **Testing:** Can validate with fresh test areas before any data migration

**Revised Approach:**
1. **Phase 1-3:** Core infrastructure (Weeks 1-12) - CRITICAL PATH
2. **Phase 4:** Subsystems migration (Weeks 13-16) - Blueprints, Dungeons, Ships
3. **Phase 5-6:** Scripting & OLC (Weeks 17-28) - Enable building
4. **Phase 7:** Commands (Weeks 29-32) - Complete functionality
5. **Phase 8:** Reserved entities & Bootstrap (Weeks 33-34) - Polish
6. **Optional Phase 9:** Old area migration tool (can be done later if desired)

**Timeline:** 8-9 months to full functionality (vs 2-3 years incremental)

**Migration Strategy for Old Areas:**
- Keep old areas in archive
- Create migration tool as separate project (optional, can be done anytime)
- Builders can manually recreate important areas in new format (better than auto-conversion)
- Focus on NEW area creation with proper widevnum from start

**Success Criteria:**
- System works with widevnum from day one
- Can create new areas with full widevnum support
- OLC fully functional
- Scripting fully functional
- Bootstrap process works (can start from empty DB)
- Performance acceptable
- (Optional) Migration tool for old areas works when needed

## Next Steps

1. **Review this document** with development team
2. **Decide on approach** (full backport, incremental, or alternative)
3. **If proceeding:**
   - Create detailed Phase 1 technical spec
   - Set up test environment
   - Create backup/rollback procedures
   - Identify test areas/characters
   - Begin Phase 1 implementation
4. **Create testing framework** (see [testing framework documentation](../testing/))
5. **Establish code review process**
6. **Set up monitoring** for performance metrics

## References

### Key Commits in src_20_dev
- `aea1d28` - Initial widevnum implementation (2023-04-25)
- `d90fe38` - Miscellaneous widevnum changes (2023-05-06)
- `1a375d0` - Widevnum scripting fixes (2023-05-07)
- `adcaa01` - Custom trigger types with widevnum (2023-07-02)
- `930bf55` - AUTOOLC toggle (2023-07-05)
- `53a4f4f` - Dungeon changes with widevnum (2023-07-09)

### Related Documentation
- [testing/](../testing/) - JSON-driven unit and integration testing framework
- CODE_MIGRATION_PATTERNS.md - Code migration guidelines
- REFACTORING_STATUS.md - Current refactoring state

### External Resources
- ROM codebase documentation
- Smaug area format documentation
- MUD development forums (widevnum discussions)
