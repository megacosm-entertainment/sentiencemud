# Widevnum Migration Master Plan

**Date:** January 27, 2026  
**Last Updated:** February 18, 2026  
**Status:** Historical planning document (migration completed)  
**Estimated Timeline:** Historical estimate (no longer active)

> This document is preserved as the original planning baseline. The widevnum migration and post-completion cleanup are complete in `src`; see `WIDEVNUM_REMAINING_WORK.md` for final completion status.

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Performance Considerations](#performance-considerations)
3. [Architecture Modernization](#architecture-modernization)
4. [Implementation Phases](#implementation-phases)
5. [Risk Management](#risk-management)

---

## Executive Summary

This plan combines three major improvements into a single coordinated effort:

1. **Widevnum Migration:** Transition from global vnums to area-scoped vnums
2. **JSON Persistence:** Migrate all data files (areas, mail, notes) to JSON+Redis
3. **Editor Modernization:** Port widevnum changes to modular editor structure

**Why combine these?** They share overlapping code changes (especially Phase 3) and benefit from unified Redis caching infrastructure. Doing them together is ~40% faster than sequential migration.

---

## Performance Considerations

### The Performance Question

**Q: Why is performance degradation listed as a risk?**

**A: Area UID lookups become a critical hot-path operation.**

### Current System (No Widevnums)

```c
// Direct hash table lookup - O(1)
MOB_INDEX_DATA *get_mob_index(long vnum) {
    return mob_index_hash[vnum % MAX_KEY_HASH];
}
```

**Performance:** Single hash table lookup = O(1)

### Widevnum System (Naive Implementation)

```c
// Two-step process
MOB_INDEX_DATA *get_mob_index(AREA_DATA *pArea, long vnum) {
    return pArea->mob_index_hash[vnum % MAX_KEY_HASH];
}

// But to get pArea, you need:
AREA_DATA *get_area_from_uid(long uid) {
    for (area = area_first; area; area = area->next) {
        if (area->uid == uid) return area;
    }
    return NULL;  // O(N) where N = number of areas!
}
```

**Performance:** Linear scan + hash lookup = O(N) + O(1) = O(N)

**Impact:**
- With 84 areas (current count), every index lookup adds ~42 pointer checks average
- Frequent operations like combat, scripting, OLC become noticeably slower
- **This is WHY performance is a concern**

### Optimized Widevnum System (In-Memory Hash Table)

```c
// In-memory hash table for area UID lookups
AREA_DATA *area_uid_hash[MAX_KEY_HASH];  // Global hash table

AREA_DATA *get_area_from_uid(long uid) {
    AREA_DATA *area;
    int hash_key = uid % MAX_KEY_HASH;
    
    // O(1) hash table lookup (in-memory)
    for (area = area_uid_hash[hash_key]; area; area = area->next_hash) {
        if (area->uid == uid)
            return area;
    }
    return NULL;
}
```

**Performance:** Hash table lookup = O(1) average, O(k) worst case (k = collisions)

**Benefits:**
- All lookups: O(1) average (same as current get_mob_index)
- In-memory only: No Redis overhead
- With MAX_KEY_HASH=1024 and 84 areas: ~0.08 items per bucket
- **No performance degradation compared to current system**

**Redis Role:**
- Redis is ONLY for async disk writes (write-through cache)
- Editors update in-memory → Push to Redis → Background worker writes to disk
- NOT used for runtime area lookups

### Data Flow Architecture

**In-Memory → Redis → Disk (Current System)**

```c
// Example: OLC area edit
void aedit_save(CHAR_DATA *ch, AREA_DATA *pArea) {
    // 1. Update in-memory structure (immediate)
    pArea->name = str_dup(new_name);
    
    // 2. Push to Redis cache (fast, async)
    redis_cache_area(pArea);  // Serializes to JSON, adds to dirty queue
    
    // 3. Background worker writes to disk (non-blocking)
    //    Happens automatically via async_cache worker thread
}

// Load at boot
void load_areas() {
    // 1. Read from disk (startup only)
    AREA_DATA *pArea = load_area_from_json("data/world/limbo.json");
    
    // 2. Load into memory (linked list + hash table)
    LINK(pArea, area_first, area_last, next, prev);
    area_uid_hash[pArea->uid % MAX_KEY_HASH] = pArea;
    
    // 3. Warm Redis cache (optional, for faster saves)
    redis_cache_area(pArea);
}
```

**Key Points:**
- All game logic operates on **in-memory structures**
- Redis reduces disk I/O blocking (saves are async)
- Area lookups use **in-memory hash table** (O(1))
- Redis is NOT consulted during gameplay

### Performance Validation Plan

**Phase 2 (Week 8) - Benchmark Suite:**

```c
// Benchmark 1: Index lookups
void benchmark_index_lookups() {
    time_t start = current_time;
    for (int i = 0; i < 100000; i++) {
        get_mob_index(pArea, 1234);
    }
    time_t end = current_time;
    log_stringf("100k index lookups: %ld ms", (end - start));
}

// Benchmark 2: Area lookups
void benchmark_area_lookups() {
    time_t start = current_time;
    for (int i = 0; i < 100000; i++) {
        get_area_from_uid(923);
    }
    time_t end = current_time;
    log_stringf("100k area lookups: %ld ms", (end - start));
}
```

**Success Criteria:**
- Area lookups: <0.001ms average (Redis cached)
- Index lookups: No measurable difference from current
- Combat tick time: <5% increase
- OLC command response: <50ms average

### Data Flow Architecture

**In-Memory → Redis → Disk (Current System)**

```c
// Example: OLC area edit
void aedit_save(CHAR_DATA *ch, AREA_DATA *pArea) {
    // 1. Update in-memory structure (immediate)
    pArea->name = str_dup(new_name);
    
    // 2. Push to Redis cache (fast, async)
    redis_cache_area(pArea);  // Serializes to JSON, adds to dirty queue
    
    // 3. Background worker writes to disk (non-blocking)
    //    Happens automatically via async_cache worker thread
}

// Load at boot
void load_areas() {
    // 1. Read from disk (startup only)
    AREA_DATA *pArea = load_area_from_json("data/world/limbo.json");
    
    // 2. Load into memory (linked list + hash table)
    LINK(pArea, area_first, area_last, next, prev);
    area_uid_hash[pArea->uid % MAX_KEY_HASH] = pArea;
    
    // 3. Warm Redis cache (optional, for faster saves)
    redis_cache_area(pArea);
}
```

**Key Points:**
- All game logic operates on **in-memory structures**
- Redis reduces disk I/O blocking (saves are async)
- Area lookups use **in-memory hash table** (O(1))
- Redis is NOT consulted during gameplay

---

## Architecture Modernization

### 1. Index/Runtime Split (Deferred to Phase 9)

**Status:** NOT included in widevnum migration

**Reason:** src_20_dev did NOT implement this split. Rooms/areas still use hybrid model.

**Future Work:** After widevnums are stable, can add proper ROOM_DATA/ROOM_INDEX_DATA split similar to objects.

### 2. JSON Persistence (Included in Phase 3-4)

**Scope:**
- Phase 3: Areas (`.are` → `.json`)
- Phase 4: Auxiliary data (mail, notes, bans)

**Benefits:**
- Consistent data format across all systems
- Redis caching for fast lookups
- Better with widevnum references
- Version control friendly

**Example JSON Area:**
```json
{
  "format_version": 1,
  "area": {
    "uid": 923,
    "name": "Limbo",
    "min_vnum": 1,
    "max_vnum": 30,
    "rooms": {
      "1": {
        "name": "The Void",
        "exits": {
          "north": {"area": 923, "vnum": 2}
        }
      }
    }
  }
}
```

### 3. Editor Restructuring (Included in Phase 6)

**Challenge:** Main codebase uses modular structure, src_20_dev uses monolithic olc_act.c

**Solution:** Extract widevnum changes per-editor

```
src_20_dev/olc_act.c (25,701 lines)
    ↓ Extract AEDIT functions
src/editors/areas/aedit.c (1,219 lines)

    ↓ Extract REDIT functions  
src/editors/rooms/redit.c

    ↓ Extract MEDIT functions
src/editors/mobiles/medit.c
```

### 4. Reserved Entities (Enhanced in Phase 3)

**Architecture Decision:** Keep legacy's name-based system, enhance for widevnums

```c
// Current: RESERVED_DATA with int id
typedef struct reserved_data {
    char *name;
    int type;
    int id;              // Just vnum
} RESERVED_DATA;

// Enhanced: RESERVED_DATA with WNUM
typedef struct reserved_data {
    char *name;
    int type;
    WNUM wnum;           // Area + vnum
} RESERVED_DATA;

// Usage remains simple:
int skull_vnum = get_reserved_vnum("obj_skull_normal");
OBJ_INDEX_DATA *skull = get_reserved_obj_index("obj_skull_normal");
```

---

## Implementation Phases

### Phase 1: Core Infrastructure (Weeks 1-4)

**Goal:** Add WNUM structures without breaking anything

**Tasks:**
1. Add WNUM/WNUM_LOAD structures to merc.h
2. Add parse_widevnum() function
3. Add widevnum_string() formatting functions
4. Add get_area_from_uid() function
5. Update AREA_DATA with UID and hash tables
6. Update GLOBAL_DATA with next_area_uid counter
7. Add unit tests using JSON testing framework (see [testing documentation](../testing/))

**String Safety Note:**
While implementing widevnum functions, replace any `sprintf` with `snprintf` opportunistically:
```c
// Old: sprintf(buf, "format", args);
// New: snprintf(buf, sizeof(buf), "format", args);
```
This improves security without adding scope. Track remaining unsafe string operations for post-widevnum cleanup (see TECH_DEBT.md).

**Risk:** LOW - Additive only, no changes to existing code

**Success Criteria:**
- Code compiles
- All structures defined
- Functions tested independently
- No impact on running game

### Phase 2: Index System Migration (Weeks 5-8)

**Goal:** Move index lookups to area-scoped

**Tasks:**
1. Update get_mob_index(pArea, vnum)
2. Update get_obj_index(pArea, vnum)  
3. Update get_room_index(pArea, vnum)
4. Add get_*_index_wnum() wrappers
5. Move hash tables from global to per-area
6. Update all 100+ call sites
7. **Benchmark performance** - Critical validation

**Risk:** MEDIUM - Touches many files, performance critical

**Success Criteria:**
- All index lookups work
- Hash tables populate correctly
- Performance acceptable (<5% overhead)
- No crashes

### Phase 3: Database & JSON Migration (Weeks 9-12)

**Goal:** Migrate areas to JSON, add Redis caching

**Tasks:**
- **Week 9:** JSON area serialization
  - Implement json_persist_area_to_json()
  - Implement json_persist_json_to_area()
  - Test round-trip conversion
  
- **Week 10:** In-memory hash table optimization
  - Add area_uid_hash[MAX_KEY_HASH] global
  - Implement hash-based get_area_from_uid() for O(1)
  - Update area initialization to populate hash table
  - Implement redis_cache_area() for async disk writes
  - **This eliminates performance concerns**
  
- **Week 11:** Dual format support
  - Load from .are or .json
  - Conversion tool
  - Save always uses JSON
  
- **Week 12:** Migration & cleanup
  - Convert all areas
  - Remove old .are parsing
  - Document new format

**Risk:** MEDIUM - Data migration, backward compatibility

**Success Criteria:**
- Area UID hash table populates correctly
- Area lookups <0.001ms (in-memory hash lookup)
- Redis async writes work (no blocking)>99%
- Area lookups <0.001ms
- No data loss

### Phase 4: Core Subsystems & Aux Data (Weeks 13-16)

**Goal:** Update subsystems, migrate auxiliary data

**Tasks:**
- **Week 13:** Resets, blueprints, dungeons
- **Week 14:** Ships & boats (in-memory structures)
- **Week 15:** Auxiliary data to JSON (all vnum-containing files)
  - Mail system (mail.dat → JSON)
  - Notes/boards (notes.txt → JSON)
  - Bans (ban.dat → JSON)
  - Churches (.org files → JSON with area_uid references)
  - Ship persistence (save/load with WNUM)
  - Instances (instances.dat → JSON with WNUM)
  - Chat rooms (chat rooms with vnum references)
- **Week 16:** Data migration testing and validation

**Risk:** LOW-MEDIUM - Well-defined subsystems

### Phase 5: Scripting Engine (Weeks 17-20)

**Goal:** Update script parsing for widevnums

**Tasks:**
1. Add ENT_WIDEVNUM entity type
2. Update script command parsing
3. Update MLOAD/OLOAD/GOTO/TRANSFER
4. Update script variables
5. Update all prog types (mprog/oprog/rprog/tprog)

**Risk:** MEDIUM - Complex scripting system

### Phase 6: OLC Editors (Weeks 21-24)

**Goal:** Port widevnum changes to modular editors

**Tasks:**
- **Week 21:** Core editors (aedit, redit, medit, oedit)
- **Week 22:** Script editors (olc_mpcode.c)
- **Week 23:** Specialized editors (blueprints, dungeons, ships)
- **Week 24:** Helper editors (tedit, hedit, socialedit)

**Risk:** LOW - Surgical extraction from olc_act.c

### Phase 7: Commands & UI (Weeks 25-28)

**Goal:** Update user-facing commands

**Tasks:**
1. Update lookup commands (goto, mload, oload)
2. Update display commands (vlist, mlist, olist)
3. Update OLC commands (show, list, create)
4. Update help files

**Risk:** LOW - Straightforward updates

### Phase 8: Testing & Polish (Weeks 29-34)

**Goal:** Production readiness

**Tasks:**
- **Weeks 29-30:** Integration testing
- **Weeks 31-32:** Performance optimization
- **Weeks 33:** Documentation
- **Week 34:** Production deployment

**Risk:** LOW - Validation phase

---

## Risk Management

### Performance Risks

| Risk | Mitigation | Status |
|------|-----------|--------|
| In-memory hash table for O(1) lookups | Planned (Phase 3) |
| Hash collisions degrade performance | Use MAX_KEY_HASH=1024 (0.08 items/bucket) | Built-in |
| Redis unavailable blocks saves | Queue saves in memory, retry connection | Planned (Phase 3) 3) |
| Redis unavailable | Graceful fallback to linear scan | Built-in |

### Data Risks

| Risk | Mitigation | Status |
|------|-----------|--------|
| Area conversion data loss | Dual format support during transition | Planned (Phase 3) |
| Mail/notes corruption | Conversion tool with validation | Planned (Phase 4) |
| Reserved entity mismatches | Update RESERVED_DATA structure | Planned (Phase 3) |

### Code Risks

| Risk | Mitigation | Status |
|------|-----------|--------|
| 100+ file changes introduce bugs | Incremental testing per phase | Built-in |
| Editor extraction errors | Compare behavior with src_20_dev | Planned (Phase 6) |
| Script parsing breaks | Comprehensive script test suite | Planned (Phase 5) |

---

## Related Documents

- [WIDEVNUM_BACKPORT_ANALYSIS.md](WIDEVNUM_BACKPORT_ANALYSIS.md) - Detailed technical analysis of src_20_dev implementation
- [WIDEVNUM_IMPLEMENTATION_PLAN.md](WIDEVNUM_IMPLEMENTATION_PLAN.md) - Task-by-task breakdown with code examples
- [ARCHITECTURE_CHANGES_ANALYSIS.md](ARCHITECTURE_CHANGES_ANALYSIS.md) - Architectural improvements coordinated with widevnums
- [testing/](../testing/) - Comprehensive unit and integration testing framework

---

## Next Steps

**Ready to begin Phase 1, Task 1.1:**

1. Create feature branch: `git checkout -b feature/widevnum-migration`
2. Open `/sentience/src/merc.h`
3. Add WNUM structures (see WIDEVNUM_IMPLEMENTATION_PLAN.md Task 1.1)
4. Commit and proceed to Task 1.2

**Estimated completion:** September 2026 (8.5 months from now)
