# Architecture Changes Analysis: Index/Runtime Split & Editor Restructuring

**Date:** January 27, 2026  
**Author:** Copilot Analysis  
**Related To:** WIDEVNUM Implementation Plan

---

## Overview

Two major architectural changes need to be coordinated with the widevnum migration:

1. **Index/Runtime Split for Rooms & Areas** - Moving rooms and areas to follow the same pattern as objects/mobiles/tokens
2. **Editor Restructuring** - Main codebase has reorganized editors into subdirectories, while src_20_dev uses monolithic olc_act.c

---

## 1. Index/Runtime Split for Rooms & Areas

### Current State: What Objects/Mobiles/Tokens Already Have

**Pattern: Separate Index (persistent template) from Runtime (live instances)**

```c
// Objects use this pattern
struct obj_index_data {      // Template/blueprint
    long vnum;
    char *name;
    char *short_descr;
    // ... static data that never changes at runtime
    int count;               // How many exist
};

struct obj_data {            // Live instance
    OBJ_INDEX_DATA *pIndexData;   // Points to template
    CHAR_DATA *carried_by;        // Runtime state
    OBJ_DATA *in_obj;
    // ... dynamic runtime data
    pVARIABLE vars;              // Scripts can add variables
    unsigned long id[2];         // Unique instance ID
};
```

**Benefits:**
- Scripts can attach variables to runtime instances without affecting persistent data
- Multiple instances from same template share read-only data
- Clear separation between "what it is" (index) vs "current state" (runtime)
- Easier to persist runtime changes without corrupting templates

### Current State: Rooms (Partially Split)

Rooms are **already partially using** the index/runtime pattern:

```c
// From src/merc.h
struct room_index_data {
    // ... room template data ...
    long vnum;
    char *name;
    char *description;
    AREA_DATA *area;
    
    // OLC Reset data (template)
    long rs_room_flag[2];
    int  rs_sector_type;
    int  rs_heal_rate;
    
    // Live runtime data (ALREADY SPLIT!)
    long room_flag[2];
    int  sector_type;
    int  heal_rate;
    
    // Runtime instance tracking
    ROOM_INDEX_DATA *source;      // If clone, points to template
    unsigned long id[2];          // Clone instance ID
    ROOM_INDEX_DATA *clones;      // List of clones from this template
    
    // Script variables
    pVARIABLE index_vars;
    
    // ... more runtime state ...
};
```

**Current Situation:**
- Rooms use a **hybrid model** - one structure with both template (`rs_*`) and runtime fields
- Clone rooms already use the source/instance pattern
- But there's no clean `ROOM_DATA` vs `ROOM_INDEX_DATA` split like objects have

### What src_20_dev Does (From Search Results)

**Evidence from searches:**
```c
// src_20_dev still uses ROOM_INDEX_DATA (not a full split)
ROOM_INDEX_DATA *get_room_index(AREA_DATA *pArea, long vnum);
ROOM_INDEX_DATA *create_virtual_room(ROOM_INDEX_DATA *source, ...);
```

**From blueprint.c cloning:**
```c
ROOM_INDEX_DATA *clone_blueprint_section(BLUEPRINT_SECTION *parent) {
    ROOM_INDEX_DATA *room;
    // Clone rooms from section
    for(long vnum = parent->lower_vnum; vnum <= parent->upper_vnum; vnum++) {
        // Creates room instances...
    }
}
```

**Key Finding:** src_20_dev did **NOT** implement a full ROOM_DATA/ROOM_INDEX_DATA split like objects. It still uses the hybrid model.

### Current State: Areas (No Split)

Areas have **NO index/runtime split** currently:

```c
struct area_data {
    AREA_DATA *next;
    char *name;
    char *file_name;
    long anum;           // Area number (position in list)
    long uid;            // Unique ID
    long min_vnum;
    long max_vnum;
    int age;             // Runtime: how long since repop
    int nplayer;         // Runtime: player count
    bool empty;          // Runtime: no players present
    // Mix of template and runtime data
};
```

**Problem:** No way to attach script variables to area instances without persisting them.

### What SHOULD Be Done for Widevnums

**Recommendation: Do NOT add full index/runtime split as part of widevnum migration**

**Reasons:**
1. **Widevnum doesn't require it** - The WNUM structure works with either model
2. **src_20_dev didn't do it** - No proven implementation to reference
3. **Massive scope increase** - Would multiply migration complexity by 3-4x
4. **Different concerns** - Index/runtime split is about memory/scripting, widevnums is about addressing
5. **Can be done later** - Once widevnums are stable, index/runtime split can follow

**Instead:**
- Document that rooms/areas could benefit from index/runtime split in the future
- Design widevnum structures to be **compatible** with future split
- Add hooks/comments in code where split would naturally occur
- Defer actual split to Phase 9 or separate project

### Compatibility Design Considerations

When implementing WNUM, design for future split:

```c
// Current plan (Phase 1):
typedef struct {
    AREA_DATA *pArea;      // Works with current area structure
    long vnum;
} WNUM;

// Future-compatible (when area split happens):
// WNUM.pArea would point to AREA_INDEX_DATA instead
// All existing WNUM code would still work - just pointer retarget

// Same for rooms:
ROOM_INDEX_DATA *get_room_index(AREA_DATA *pArea, long vnum);
// Future: get_room_index(AREA_INDEX_DATA *pArea, long vnum)
// Widevnum functions don't need to change
```

---

## 2. Editor Restructuring

### Main Codebase (Current - Modular Structure)

**Directory Organization:**
```
src/editors/
├── common.c / common.h       # Shared editor functions
├── areas/aedit.c             # Area editor
├── blueprints/
│   ├── bpedit.c              # Blueprint editor
│   └── bsedit.c              # Blueprint section editor
├── commands/cmdedit.c        # Command editor
├── dungeons/dngedit.c        # Dungeon editor
├── game_settings/gameedit.c  # Game settings editor
├── help/hedit.c              # Help editor
├── mobiles/medit.c           # Mobile editor
├── objects/oedit.c           # Object editor
├── projects/pedit.c          # Project editor
├── random_strings/rsgedit.c  # Random string editor
├── reserved_vnums/reserved.c # Reserved vnum editor
├── rooms/redit.c             # Room editor
├── scripting/olc_mpcode.c    # Script editor
├── ships/shedit.c            # Ship editor
├── socials/socialedit.c      # Social editor
├── tokens/tedit.c            # Token editor
└── wilderness/
    └── wedit.c               # Wilderness editor (vledit folded in as VLinks tab)
```

**Characteristics:**
- Each editor in its own file
- Organized by what they edit (logical grouping)
- Common code shared via common.c
- Clear separation of concerns
- **This is the current working structure**

### src_20_dev (Monolithic Structure)

**File Organization:**
```
src_20_dev/
├── olc_act.c         # ~25,701 lines - main OLC commands
├── olc_act2.c        # Additional OLC commands
├── olc.c             # Core OLC infrastructure
├── olc_save.c        # Saving OLC data
├── olc_mpcode.c      # Script editor
├── olc_edit_rsg.c    # Random string editor
├── editor.c          # String editor
├── cmdedit.c         # Command editor
├── gameedit.c        # Game settings editor
└── olc.h             # OLC headers
```

**Characteristics:**
- Most editors crammed into olc_act.c
- Harder to navigate and maintain
- Functions like `AEDIT(aedit_show)`, `REDIT(redit_show)`, `MEDIT(medit_show)` all in one huge file
- **This is the old structure**

### Migration Strategy

**Challenge:** src_20_dev has widevnum changes scattered through olc_act.c, but main codebase has split those into separate files.

**Solution: Per-Editor Surgical Extraction**

Rather than trying to merge entire olc_act.c, extract changes for each editor individually:

1. **Identify widevnum changes in olc_act.c by editor:**
   ```bash
   # Find AEDIT functions with widevnum references
   grep -B5 -A50 "^AEDIT" olc_act.c | grep -C10 "wnum\|WNUM"
   ```

2. **Apply changes to corresponding modular file:**
   - olc_act.c `AEDIT` functions → src/editors/areas/aedit.c
   - olc_act.c `REDIT` functions → src/editors/rooms/redit.c
   - olc_act.c `MEDIT` functions → src/editors/mobiles/medit.c
   - etc.

3. **Benefits:**
   - Keep modern modular structure
   - Apply only widevnum-related changes
   - Easier to review changes per-editor
   - Less chance of merge conflicts

**Example Extraction Process:**

```bash
# Step 1: Extract all AEDIT functions from src_20_dev/olc_act.c
awk '/^AEDIT\(/,/^}/' /sentience/src_20_dev/olc_act.c > /tmp/aedit_functions.txt

# Step 2: Search for widevnum changes
grep -n "wnum\|WNUM\|get_.*_index.*pArea\|parse_widevnum" /tmp/aedit_functions.txt

# Step 3: Manually port those specific changes to src/editors/areas/aedit.c
```

### Widevnum Impact on Editors

**Areas Affected:**

1. **AEDIT (Area Editor):**
   - Display area UID
   - Manage per-area vnum ranges
   - Show area hash tables

2. **REDIT (Room Editor):**
   - Change: `redit <vnum>` → `redit [area#]vnum`
   - Link exits using widevnums
   - Display room's area context

3. **MEDIT (Mobile Editor):**
   - Change: `medit <vnum>` → `medit [area#]vnum`
   - Show/edit mobile in area context

4. **OEDIT (Object Editor):**
   - Change: `oedit <vnum>` → `oedit [area#]vnum`
   - Reference objects by widevnum in scripts

5. **Script Editors (olc_mpcode.c):**
   - Parse widevnum syntax in scripts
   - Validate area references
   - Show entity lookups using WNUM

6. **Reset Editors (in REDIT):**
   - Resets reference mobs/objs by WNUM
   - Must parse and validate area#vnum syntax

**Example Changes Needed:**

**Before (current):**
```c
// src/editors/rooms/redit.c
REDIT(redit_create) {
    // ...
    if (argument[0] == '\0' || atol(argument) == 0) {
        send_to_char("Syntax: redit create <vnum>\n\r", ch);
        return FALSE;
    }
    
    if (get_room_index(atol(argument)) != NULL) {
        send_to_char("Room vnum already exists.\n\r", ch);
        return FALSE;
    }
    // ...
}
```

**After (with widevnums):**
```c
// src/editors/rooms/redit.c
REDIT(redit_create) {
    WNUM wnum;
    AREA_DATA *pArea;
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: redit create [area#]vnum\n\r", ch);
        return FALSE;
    }
    
    // Parse widevnum (handles both "1234" and "924#45")
    if (!parse_widevnum(ch, argument, &wnum, &pArea, true)) {
        return FALSE;  // parse_widevnum sends error message
    }
    
    if (get_room_index(pArea, wnum.vnum) != NULL) {
        printf_to_char(ch, "Room %s already exists.\n\r", 
                      widevnum_string(&wnum));
        return FALSE;
    }
    // ...
}
```

---

## JSON Persistence for Areas

**CRITICAL STRATEGIC DECISION:** Migrate areas to JSON+Redis during widevnum implementation

### Current State

**Legacy codebase has JSON infrastructure:**
```c
// json_persist.h - Already implemented for rooms, mobiles, objects
json_t *json_persist_room_to_json(ROOM_INDEX_DATA *room);
ROOM_INDEX_DATA *json_persist_json_to_room(json_t *json);
bool json_persist_save_room_cached(ROOM_INDEX_DATA *room);  // Redis-backed
ROOM_INDEX_DATA *json_persist_load_room_cached(const char *room_id);

// Redis caching with async disk writes
bool redis_cache_persist_data(const char *key, const char *json_str);
char *redis_get_persist_data(const char *key);
```

**But areas still use old .are format:**
- `/sentience/area/*.are` - Old ROM format
- No JSON serialization for AREA_DATA
- No Redis caching for area lookups
- No area UID → area pointer fast lookup

**User's observation:** "Should that be part of this? I feel like it would make (at least) the id portion of looking things up a bit easier."

**Answer:** **YES - this is the perfect time to do it.**

### Why Combine JSON Migration with Widevnums

1. **Area UID Lookups Are Critical for WNUM:**
   ```c
   // Every widevnum operation requires area lookup:
   WNUM wnum;
   wnum.pArea = get_area_from_uid(area_uid);  // This becomes HOT path
   wnum.vnum = local_vnum;
   ```
   With Redis: `get_area_from_uid()` becomes O(1) hash lookup instead of linear scan

2. **Already Touching Area Loading Code (Phase 3):**
   - Phase 3 was going to update area loading for widevnums anyway
   - Changing file format at same time avoids touching code twice
   - Single migration event instead of two separate disruptions

3. **JSON Format Cleaner for WNUM Storage:**
   ```json
   {
     "area": {
       "uid": 923,
       "name": "Limbo",
       "vnum_range": [1, 30],
       "rooms": {
         "1": { "name": "Limbo", ... },
         "2": { "name": "Temple", ... }
       },
       "mobiles": {
         "5": { "name": "Death Mob", ... }
       },
       "objects": {
         "10": { "name": "Portal", ... }
       }
     }
   }
   ```
   vs. old .are format with global vnums mixed in

4. **Infrastructure Already Exists:**
   - JSON serialization functions for rooms/mobs/objs ✓
   - Redis caching layer ✓
   - Async disk writer ✓
   - Just need to extend to areas

5. **Performance Benefits Stack:**
   - Widevnum lookups faster (cached area pointers)
   - Area searches faster (Redis indexes)
   - Parsing faster (JSON vs custom .are format)
   - Concurrent access safer (Redis atomic operations)

### Implementation Strategy

**Extend existing json_persist.c infrastructure:**

```c
// Add to json_persist.h
json_t *json_persist_area_to_json(AREA_DATA *area);
AREA_DATA *json_persist_json_to_area(json_t *json);
bool json_persist_save_area(AREA_DATA *area);
AREA_DATA *json_persist_load_area(long uid);

// Redis cache for area lookups
bool redis_cache_area(AREA_DATA *area);
AREA_DATA *redis_get_area(long uid);
```

**Migration path:**

1. **Phase 3a: JSON Area Format (Week 9)**
   - Implement `area_to_json()` / `json_to_area()`
   - Include WNUM-aware room/mob/obj serialization
   - Store local vnums within area context
   - Test serialization without breaking existing .are loading

2. **Phase 3b: Redis Area Cache (Week 10)**
   - Add `redis_cache_area()` with UID-based keys
   - Add `get_area_from_uid()` fast path (check Redis first)
   - Warm cache at boot with all areas
   - Widevnum lookups now O(1)

3. **Phase 3c: Dual Format Support (Week 11)**
   - Load areas from either .are or .json
   - Auto-detect format: `.are` = old, `.json` = new
   - Conversion tool: `./convert_area oldfile.are newfile.json`
   - Save always uses JSON (write-through to Redis)

4. **Phase 3d: Migration & Cleanup (Week 12)**
   - Convert all `/sentience/area/*.are` → `/sentience/data/areas/*.json`
   - Update area.lst to point to JSON files
   - Remove old .are parsing code (keep in git history)
   - Document new JSON area format

### JSON Area Format Structure

```json
{
  "format_version": 1,
  "area": {
    "uid": 923,
    "name": "Limbo",
    "filename": "limbo.json",
    "credits": "Original DikuMUD",
    "min_vnum": 1,
    "max_vnum": 30,
    "vnum_local": true,
    "min_level": 1,
    "max_level": 10,
    "repop": 15,
    "security": 9,
    "builders": "None",
    "flags": ["NO_RECALL"],
    "place_flags": ["INDOOR"],
    "recall": {
      "type": "vnum",
      "value": 2
    }
  },
  "rooms": {
    "1": {
      "vnum": 1,
      "name": "The Void",
      "description": "You float in an endless void...",
      "sector": "inside",
      "flags": ["INDOORS", "NO_MOB"],
      "exits": {
        "north": {
          "to_area": 923,
          "to_vnum": 2,
          "description": "A portal shimmers..."
        }
      }
    }
  },
  "mobiles": {
    "5": { ... }
  },
  "objects": {
    "10": { ... }
  },
  "resets": [
    {
      "command": "M",
      "mob": {"area": 923, "vnum": 5},
      "room": {"area": 923, "vnum": 1},
      "count": 1
    }
  ]
}
```

**Key Features:**
- Area UID prominently stored
- Local vnums within area (1-30, not global)
- WNUM format for cross-area references
- Room/mob/obj data nested under area
- Clean JSON parsing (no custom format)
- Redis-cacheable structure

### Benefits Specific to Widevnums

1. **Faster Area Lookups:**
   ```c
   // Old way (linear scan):
   AREA_DATA *get_area_from_uid(long uid) {
       for (area = area_first; area; area = area->next) {
           if (area->uid == uid) return area;
       }
       return NULL;  // O(N) where N = number of areas
   }
   
   // New way (Redis cache):
   AREA_DATA *get_area_from_uid(long uid) {
       AREA_DATA *area = redis_get_area(uid);  // O(1) hash lookup
       if (area) return area;
       // Fall back to loading from disk if not cached
       area = json_persist_load_area(uid);
       if (area) redis_cache_area(area);
       return area;
   }
   ```

2. **Atomic Area Updates:**
   - OLC changes write through Redis immediately
   - Other processes see updates instantly
   - No file locking issues

3. **Easier Cross-Area References:**
   - JSON stores WNUMs naturally: `{"area": 923, "vnum": 15}`
   - No need to parse "923#15" syntax during load
   - Validation at load time (area must exist)

4. **Better Error Messages:**
   ```
   Old: "Invalid vnum 92315 in reset"
   New: "Invalid widevnum 923#15 in area Limbo: area 923 exists but vnum 15 not found"
   ```

5. **Simpler Reserved Entity System:**
   ```json
   {
     "reserved": {
       "obj_skull_normal": {"area": 923, "vnum": 1234}
     }
   }
   ```
   Legacy's name-based lookup becomes straightforward JSON deserialization

## Integration with Widevnum Implementation Plan

### Updated Phase Breakdown

**Phase 1: Core Infrastructure (Weeks 1-4)** - No change
- Add WNUM structures
- Add parsing/formatting functions
- No editor changes yet

**Phase 2: Index System Migration (Weeks 5-8)** - No change
- Update get_mob_index(), get_obj_index(), get_room_index()
- Add area pointers to index structures
- Still no editor changes (editors use index functions)

**Phase 3: Database & Persistence (Weeks 9-12)** - **MAJOR UPDATE**
- **Week 9:** JSON Area Format
  - Implement `json_persist_area_to_json()` / `json_persist_json_to_area()`
  - Design WNUM-aware serialization
  - Test round-trip conversion
- **Week 10:** Redis Area Cache
  - Add `redis_cache_area()` and `redis_get_area()`
  - Optimize `get_area_from_uid()` for O(1) lookups
  - Warm cache at boot
- **Week 11:** Dual Format Support
  - Load from both .are and .json
  - Conversion tool for migration
  - Save always uses JSON + Redis
- **Week 12:** Migration & Cleanup
  - Convert all areas to JSON
  - Remove old .are parsing (keep in git for reference)
  - Document new format
- **Document index/runtime split considerations** (but don't implement)

**Phase 4: Core Subsystems (Weeks 13-16)** - **MAJOR UPDATE**
- **Week 13:** Resets, Blueprints, Dungeons
  - Update reset system for WNUM references
  - Update blueprint/dungeon subsystems
  - Test cross-area entity spawning
- **Week 14:** Ships & Boats
  - Update ship system for WNUM
  - Test cross-area navigation
- **Week 15:** Auxiliary Data Files Migration to JSON
  - Mail system (`mail.dat` → `data/mail/*.json`)
  - Notes/boards (`notes.txt` → `data/notes/*.json`)
  - Bans (`ban.dat` → `data/bans.json`)
  - Social edits (`social.dat` → `data/socials.json`)
  - Any other `.dat`/`.txt` data files
- **Week 16:** Data Migration Testing
  - Conversion tools for all data formats
  - Validate round-trip conversions
  - Test with real production data

**Phase 5: Scripting Engine (Weeks 17-20)** - No change
- Update script parsing to handle widevnums
- Add entity type ENT_WIDEVNUM
- Update script variable system

**Phase 6: OLC Editors (Weeks 21-24)** - **MAJOR IMPACT**
- **Extract widevnum changes from src_20_dev/olc_act.c**
- **Port changes to individual editor files in src/editors/**
- Update each editor to use widevnum syntax:
  - Phase 6a: Core editors (aedit, redit, medit, oedit) - 1 week
  - Phase 6b: Script editors (olc_mpcode.c) - 1 week
  - Phase 6c: Specialized editors (blueprints, dungeons, ships) - 1 week
  - Phase 6d: Helper editors (tedit, hedit, socialedit, etc.) - 1 week

**Phase 7: Commands & User Interface (Weeks 25-28)** - No change
- Update lookup commands (goto, mload, oload, etc.)
- Update display commands (vlist, mlist, olist, etc.)

**Phase 8: Testing & Polish (Weeks 29-34)** - Update
- Full integration testing
- Performance optimization
- Documentation
- **Document future index/runtime split roadmap**

### Auxiliary Data Files Migration

**User's observation:** "We should probably also move our various data files over to json as well"

**Answer:** **Absolutely - complete the JSON migration uniformly.**

#### Currently Using JSON ✓
- Characters: `/accounts/{letter}/{name}.json`
- Accounts: (embedded in character files)
- Persistent entities: `/data/persist/{rooms|mobiles|objects}/*.json`
- Game settings: Already JSON

#### Still Using Old Formats ✗
1. **Mail System** - `mail.dat`
   ```c
   // Old format (custom parser):
   #MAIL
   Sender Tieryo~
   Recipient Elzamine~
   Sent 1706400000
   Message A message here~
   #O          // Object follows (old object format)
   ...
   #ENDMAIL
   ```
   
   **Should be:**
   ```json
   {
     "mail": [
       {
         "id": "mail_123456",
         "sender": "Tieryo",
         "recipient": "Elzamine",
         "sent_timestamp": 1706400000,
         "status": "delivered",
         "picked_up": false,
         "message": "A message here",
         "objects": [
           {"id": [12345, 67890], "vnum": 1234, ...}
         ]
       }
     ]
   }
   ```

2. **Notes/Boards** - `notes.txt`, `news.txt`, `changes.txt`
   ```c
   // Old format:
   Sender  Tieryo~
   Date    Mon Jan 27 14:30:00 2026~
   Stamp   1706400000
   To      all~
   Subject Update~
   Text
   Message body here~
   ```
   
   **Should be:**
   ```json
   {
     "notes": [
       {
         "id": "note_001",
         "sender": "Tieryo",
         "date": "Mon Jan 27 14:30:00 2026",
         "timestamp": 1706400000,
         "to_list": "all",
         "recipient_type": 0,
         "subject": "Update",
         "text": "Message body here"
       }
     ]
   }
   ```

3. **Bans** - `ban.dat` (if exists)
   - Character bans
   - Site bans
   - Should be JSON array with timestamps, reasons

4. **Socials** - `social.dat` (if custom socials editable)
   - Social definitions
   - Should be JSON for easier editing

5. **Help Files** - Currently `.txt` files
   - Could remain `.txt` (they're mostly static content)
   - Or migrate to JSON for better searching/indexing

#### Migration Benefits

1. **Consistency:**
   - All game data in same format
   - One set of serialization functions
   - Predictable file structure

2. **Better with Widevnums:**
   ```json
   // Mail can reference objects by WNUM:
   {
     "objects": [
       {
         "type": "reference",
         "area_uid": 923,
         "vnum": 1234
       }
     ]
   }
   ```

3. **Redis Caching:**
   - Can cache recent mail in Redis
   - Can cache active notes/boards
   - Faster lookups for "has_mail()" checks

4. **Easier Debugging:**
   - JSON viewers/editors widely available
   - grep/jq for searching
   - Version control friendly

#### Implementation in Phase 4 (Week 15)

**Mail System Migration:**
```c
// Add to json_persist.h
json_t *json_mail_to_json(MAIL_DATA *mail);
MAIL_DATA *json_json_to_mail(json_t *json);
bool json_save_mail_list(void);
bool json_load_mail_list(void);

// Redis cache mail list
bool redis_cache_mail_list(void);
MAIL_DATA *redis_get_user_mail(const char *recipient);
```

**Notes System Migration:**
```c
// Add to json_persist.h or new json_notes.h
json_t *json_note_to_json(NOTE_DATA *note);
NOTE_DATA *json_json_to_note(json_t *json);
bool json_save_notes(int type);  // NOTE_NOTE, NOTE_NEWS, NOTE_CHANGES
bool json_load_notes(int type);
```

**Conversion Tools:**
```bash
# Create migration utilities
./convert_mail mail.dat data/mail/mail.json
./convert_notes notes.txt data/notes/notes.json
./convert_notes news.txt data/notes/news.json
./convert_notes changes.txt data/notes/changes.json
```

**Dual Format Support (Transition Period):**
```c
// Load from either old or new format
bool load_mail(void) {
    if (file_exists("data/mail/mail.json")) {
        return json_load_mail_list();  // New format
    } else if (file_exists("mail.dat")) {
        return read_mail();  // Old format (legacy)
    }
    return false;
}

// Always save in new format
bool save_mail(void) {
    return json_save_mail_list();  // Always JSON
}
```

#### File Organization

```
/sentience/data/
├── mail/
│   └── mail.json           # All mail items
├── notes/
│   ├── notes.json          # Player notes
│   ├── news.json           # News items
│   └── changes.json        # Changelog
├── bans/
│   └── bans.json           # All ban entries
├── socials/
│   └── socials.json        # Custom socials (if editable)
└── system/
    └── game_settings.json  # Already JSON ✓
```

#### Object References in Mail/Notes

**Challenge:** Mail can contain objects with specific vnums

**Old way:**
```c
// Mail object embedded in mail.dat
// Uses global vnum - ambiguous after widevnums
```

**New way:**
```json
{
  "mail": {
    "objects": [
      {
        "persist_id": [12345, 67890],
        "template": {
          "area_uid": 923,
          "vnum": 1234
        }
      }
    ]
  }
}
```

Objects in mail use persist IDs (like cloned objects), but also store template WNUM for reference.

---

## Recommendations

### For Widevnum Implementation:

1. **Do NOT implement index/runtime split as part of widevnum**
   - Too much scope
   - Not required for widevnums to work
   - Can be done as separate Phase 9

2. **Do design WNUM to be compatible with future split**
   - Use `AREA_DATA *pArea` knowing it might become `AREA_INDEX_DATA *`
   - Add comments marking where split would occur
   - Keep functions generic enough to adapt

3. **Do keep modular editor structure**
   - Port changes from olc_act.c to individual files
   - Extract only widevnum-related changes
   - Preserve current logical organization

4. **Do plan Phase 6 carefully**
   - Budget full 4 weeks for editor updates
   - Test each editor individually before moving on
   - Create conversion tool to help find relevant changes in olc_act.c

### For Future Index/Runtime Split:

**When to do it:** After widevnums are stable (Phase 9 or separate project)

**How to approach it:**

1. **Phase 9a: Room Index/Runtime Split** (4-6 weeks)
   - Create separate `ROOM_DATA` structure
   - `ROOM_INDEX_DATA` becomes pure template (like `MOB_INDEX_DATA`)
   - `ROOM_DATA` becomes pure runtime (like `CHAR_DATA`)
   - Update all room references throughout codebase

2. **Phase 9b: Area Index/Runtime Split** (3-4 weeks)
   - Create `AREA_INDEX_DATA` structure
   - `AREA_DATA` becomes runtime instances
   - Update WNUM to use `AREA_INDEX_DATA *`
   - Most widevnum code unchanged (just retarget pointer type)

3. **Phase 9c: Script Variable Integration** (2-3 weeks)
   - Move index_vars to runtime structures
   - Update persist system to save runtime variables separately
   - Test script interactions with cloned rooms/areas

**Benefits of deferring:**
- Widevnum implementation is already 8+ months
- Adding index/runtime split would push to 12-15 months
- Can validate widevnum works before adding more changes
- Easier to isolate bugs (fewer moving parts)
- Community can start using widevnums sooner

---

## File Impact Summary

### Files Requiring Widevnum Changes (from WIDEVNUM_BACKPORT_ANALYSIS.md):
- **82 files total**

### Additional Files for Editor Restructuring:
- **20 editor files** in src/editors/ (to receive changes from olc_act.c)
- **1 source file** (olc_act.c in src_20_dev - extract from this)

### Files NOT Changing (due to deferring index/runtime split):
- Room structure remains hybrid
- Area structure remains single
- No new ROOM_DATA or AREA_INDEX_DATA structures (yet)
- No changes to room/area memory allocators

---

## Conclusion

**Priority 1: Widevnum Implementation**
- Follow existing WIDEVNUM_IMPLEMENTATION_PLAN.md
- Add Phase 6 details for modular editor porting
- Design for future compatibility with index/runtime split
- Do NOT implement split as part of widevnum

**Priority 2: Editor Restructuring**
- Extract widevnum changes from olc_act.c
- Port to individual editor files
- Preserve modern modular structure
- Test each editor independently

**Priority 3: Index/Runtime Split (Future)**
- Plan as Phase 9 or separate project
- Do AFTER widevnums are stable
- Coordinate with widevnum maintainers
- Document compatibility requirements

**Timeline:**
- Widevnum with editor porting: **34 weeks** (8.5 months)
- Add index/runtime split: **+9-13 weeks** (12-13 months total)
- **Recommended:** Do widevnums first, evaluate split later

---

## Next Steps

1. **Confirm with user:**
   - Agree to defer index/runtime split to Phase 9/future
   - Confirm keeping modular editor structure
   - Get approval to start Phase 1 of widevnum plan

2. **Update WIDEVNUM_IMPLEMENTATION_PLAN.md:**
   - Add detailed Phase 6 tasks for editor porting
   - Add note about index/runtime compatibility design
   - Reference this document for architectural context

3. **Create extraction tool:**
   - Script to find widevnum changes in olc_act.c
   - Map functions to target editor files
   - Generate checklist for Phase 6

4. **Begin Phase 1:**
   - Create feature branch
   - Add WNUM structures to merc.h
   - Proceed with implementation
