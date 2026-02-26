# Wilderness System Analysis

**Date:** January 28, 2026  
**Purpose:** Analyze the current wilderness system implementation, identify complexity issues, and propose improvements to restore lost features (weather, boat system, NPC pirates).

---

## Executive Summary

The wilderness system uses a virtual room (vroom) approach where rooms are dynamically loaded/unloaded as players move through generated map areas. While this saves memory, it has introduced significant complexity and broken integration with weather, boats, and NPC systems that relied on persistent room states.

**Key Issues:**
1. Virtual rooms are ephemeral - loaded on-demand, destroyed when empty
2. Weather system exists but is disconnected from wilderness
3. Boat system works but lacks NPC pirates and environmental interactions
4. Room lifecycle management is complex and fragile
5. No persistence for dynamic wilderness state changes

---

## Current System Architecture

### 1. Map Generation (wildgen Tool)

**Location:** `/sentience/tools/wildgen/`

**Process:**
1. Takes PNG image files from `/sentience/tools/maps/` (e.g., `wilds_200612_0_0.png`)
2. Uses `config.ini` files to map RGB colors to terrain types
3. Generates `.are` files containing wilderness definitions
4. Current main wilderness: `nibswilds.are` (35MB) with multiple map sections

**Configuration Example:** (`smallwilds.ini`)
```ini
[terrain_0]
tile=" "
color=#FFFF00
name=The Great Western Desert
sector=desert
maptile="."
mapcolor="Y"

[terrain_2]
tile="$"
color=#00C000
name=Blackmoore Forest
sector=forest
maptile="*"
mapcolor=G
```

**Key Features:**
- Maps terrain colors to room templates
- Supports vlinks (connections between wilderness and static areas)
- Generates room flags, sector types, movement costs per terrain
- Can define special properties (underwater, dark, alchemy zones, etc.)

### 2. Runtime Virtual Room System (wilds.c)

**Core Data Structures:**

```c
struct wilds_data {
    WILDS_DATA      *next;
    AREA_DATA       *pArea;
    long            uid;
    char            *staticmap;      // Permanent map definition
    char            *map;            // Working map (can be modified)
    int             map_size_x;
    int             map_size_y;
    WILDS_TERRAIN   *pTerrain;       // Terrain templates
    WILDS_VLINK     *pVLink;         // Links to/from static areas
    int             loaded_rooms;    // Current loaded vroom count
    LLIST           *loaded_vrooms;  // List of active vrooms
    int             nplayer;         // Player count
    bool            empty;
    int             age;             // Current age
    int             repop;           // Age to repop at
}
```

**Virtual Room Lifecycle:**

```
Player enters coordinates (x,y)
    ↓
get_wilds_vroom() called
    ↓
Room exists? → Yes → Return existing room
    ↓ No
create_vroom() called
    ↓
- Allocate ROOM_INDEX_DATA
- Copy from terrain template
- Set ROOM_VIRTUAL_ROOM flag
- Add to loaded_vrooms list
- Create exits to adjacent rooms
- Link any vlinks
    ↓
Room in use
    ↓
All players leave
    ↓
destroy_wilds_vroom() called
    ↓
- Check persist flag (keeps room alive)
- Check for vlinks (keeps room alive)
- Check for active scripts (delays destruction)
- Remove from loaded_vrooms
- Add to garbage collection
```

**Problem:** Rooms are destroyed when empty, losing:
- Weather state
- Spawn timers
- Dynamic objects
- NPC ship locations
- Environmental effects

### 3. Weather System (weather.c)

**Status:** Partially implemented but **disconnected** from wilderness

**Core Structure:**
```c
STORM_DATA* create_storm(AREA_DATA *pArea, int storm_type, 
                         int x, int y, int radius, 
                         float dx, float dy, int speed, int life)
```

**Storm Types:**
- `WEATHER_TORNADO`
- `WEATHER_HURRICANE`
- `WEATHER_SNOW_STORM`
- `WEATHER_LIGHTNING_STORM`
- `WEATHER_RAIN_STORM`

**What's Broken:**
1. Storm update code exists (`update_weather()`) but appears commented out or inactive
2. Storms stored on `AREA_DATA` but wilderness is virtual
3. Areas get `affected_by_storm` and `storm_close` pointers
4. Weather affects visibility in `get_squares_to_show_x/y()` in wilds.c:
   ```c
   case SKY_CLOUDLESS: squares_to_show_x = 16 + bonus_view; break;
   case SKY_RAINING:   squares_to_show_x = 12 + bonus_view; break;
   case SKY_LIGHTNING: squares_to_show_x = 10 + bonus_view; break;
   ```
5. No integration with ship movement or wilderness room effects

**Evidence of Old System:**
- Comments in act_info.c: `/* MOVED: weather/weather.c */`
- `do_weather()` function exists but notes "Whisp's weather system isn't yet functional"
- Storm movement/tracking logic present but not called

### 4. Boat System (boat.c - 7372 lines)

**Status:** Functional but **limited**

**Current Features:**
- Ships move through wilderness using coordinate system
- Navigation with waypoints and routes
- Crew skills (scouting, gunning, oarring, mechanics, navigation, leadership)
- Steering/heading calculations using Bresenham-like algorithm
- Ship types defined in `boat_table[]`

**Ship Steering System:**
```c
void steering_calc_heading(SHIP_DATA *ship) {
    ship->steering.dx = (int)(1000 * sin(3.14159 * ship->steering.heading / 180));
    ship->steering.dy = -(int)(1000 * cos(3.14159 * ship->steering.heading / 180));
    ship->steering.compass = bearing_door[2 * ship->steering.heading / 45];
}

bool ship_seek_point(SHIP_DATA *ship) {
    // Calculate distance to destination
    // Adjust heading to target
    // Use crew navigation skill
}
```

**What's Missing:**
1. **NPC Pirates:** No evidence of NPC-controlled ships
   - Searched for `pirate.*npc`, `npc.*ship`, `SHIP_NPC` - nothing found
   - Pirate reputation system exists in player stats:
     ```c
     if (ch->pcdata->rank[CONT_PIRATE] > NPC_SHIP_RANK_NONE)
     ```
   - But no NPC pirate ships spawn or patrol

2. **Weather Integration:**
   - No wind effects on sailing speed/direction
   - No storm hazards
   - No visibility reduction in fog

3. **Environmental Interactions:**
   - Ships listed in room descriptions (act_info.c lines 1561-1609)
   - But no dynamic spawn/patrol system
   - No encounters between ships

**Ship Display Code (works):**
```c
// From act_info.c - shows ships in same room
for (ship = ((AREA_DATA *) get_sailing_boat_area())->ship_list;
     ship != NULL; ship = ship->next) {
    sprintf(buf, "{MThe %s '%s', flying the flag '%s' is %s to the ",
            boat_table[ship->ship_type].name, 
            ship->ship_name, 
            ship->flag,
            ship->speed != SHIP_SPEED_STOPPED ? "sailing" : "anchored");
}
```

---

## Root Causes of Complexity

### 1. Virtual Room Paradox
- **Pro:** Saves memory (wilderness can be millions of rooms)
- **Con:** Loses state when rooms unload
- **Result:** Can't have persistent weather, spawns, or dynamic content

### 2. Dual Map System
- `staticmap` - permanent terrain definition
- `map` - working copy for runtime changes
- **Issue:** Changes to `map` lost when area resets or server restarts
- **No persistence:** Dynamic flooding, terrain damage, etc. not saved

### 3. Weather-Room Mismatch
- Weather operates on coordinate-based storms (x, y, radius)
- Rooms are virtual and ephemeral
- No way to apply storm effects to non-existent rooms
- Even when rooms exist, they're destroyed before weather can affect them

### 4. NPC Ship Problem
- NPC ships need rooms to exist in
- Can't path through wilderness if rooms don't exist
- Movement updates would need to load/unload hundreds of rooms
- Too expensive computationally

### 5. VLINK Complexity
- Virtual links connect wilderness to static areas
- Bidirectional linking requires careful state management
- Room destruction must check for vlinks (keeps room alive)
- Creates "pinned" virtual rooms that never unload

---

## Wildgen Architecture Decision

### Current System: Separate Tool

**Location:** `/sentience/tools/wildgen/` (1160 lines + PNG utilities)  
**Dependencies:** libpng16  
**Binary Size:** 76KB  
**Process:** PNG → INI config → .are file (offline)

**How It Works:**
```
1. Designer creates PNG map in image editor (Photoshop, GIMP, etc.)
2. Color = Terrain Type (defined in .ini file)
3. Run: wildgen from maps/wilds/ directory
4. Generates .are file (e.g., nibswilds.are - 35MB)
5. Server loads .are on boot
```

**Current PNG Handling:**
```c
// pngutil.c - Custom wrapper around libpng
bool png_load(char *fname, U8 **bits, U32 *bpp, U32 *width, U32 *height);
bool png_save(char *fname, U8 *bits, U32 width, U32 height);
```

### Options Analysis

#### Option 1: Keep Separate (Current) ✅ Recommended for Now

**Pros:**
- ✅ **Separation of concerns** - Map design is design-time, not runtime
- ✅ **No PNG dependency in game** - Smaller binary, fewer security risks
- ✅ **Faster boot** - Pre-processed data loads quickly
- ✅ **Offline editing** - Can work on maps without running server
- ✅ **Standard workflow** - PNG editing tools are mature and powerful
- ✅ **Version control** - PNG files are binary but .are files show changes
- ✅ **Independent compilation** - wildgen can be updated without game recompile

**Cons:**
- ❌ Manual regeneration step required
- ❌ Can't dynamically expand wilderness at runtime
- ❌ No in-game map editing tools
- ❌ Changes require server restart (or reload command)

**Best For:**
- Static world design
- Large wilderness areas
- Careful, planned terrain layouts
- Your **current needs** (runtime modifications to existing maps)

#### Option 2: Integrate PNG Handling Into Game

**Pros:**
- ✅ Could regenerate maps on demand
- ✅ Could expand wilderness dynamically (new areas on the fly)
- ✅ Possible OLC integration for terrain editing
- ✅ Could load PNG sections lazily (stream terrain as needed)
- ✅ Procedural generation possibilities

**Cons:**
- ❌ **libpng dependency** in game server (security surface)
- ❌ **Slower boot** - PNG parsing on every startup
- ❌ **Larger binary** - +libpng + image processing code
- ❌ **More complexity** - PNG handling + existing .are handling
- ❌ **Memory overhead** - Would need to keep RGB→terrain mappings in memory
- ❌ **PNG security risks** - Parsing vulnerabilities (historic libpng issues)
- ❌ **Less flexible** - PNG editing tools better than in-game editors

**Best For:**
- Procedurally generated worlds
- MMORPGs with constantly expanding territories
- Games where players create/modify terrain
- Not your current use case

#### Option 3: Hybrid Approach (Future Consideration)

**Implementation:**
1. Keep wildgen as external tool for initial generation
2. Add **runtime expansion** using procedural generation (no PNG)
3. Use **wilderness_state** for runtime modifications
4. Optional: Add `.png` reload command for development

**Example Development Command:**
```c
void do_wildgen_reload(CHAR_DATA *ch, char *argument) {
    if (ch->level < MAX_LEVEL) return;
    
    char wilds_name[256];
    one_argument(argument, wilds_name);
    
    // Validate name
    WILDS_DATA *pWilds = get_wilds_from_name(wilds_name);
    if (!pWilds) {
        send_to_char("Wilderness not found.\n\r", ch);
        return;
    }
    
    // Call external wildgen process
    char cmd[512];
    sprintf(cmd, "cd /sentience/tools/maps/wilds && ./wildgen %s.ini", wilds_name);
    int result = system(cmd);
    
    if (result == 0) {
        // Reload the .are file
        reload_wilderness_area(pWilds);
        send_to_char("Wilderness regenerated and reloaded.\n\r", ch);
    } else {
        send_to_char("Wildgen failed.\n\r", ch);
    }
}
```

**Benefits:**
- Keep clean separation
- Fast development iteration
- No PNG dependency in production
- Optional dev-mode convenience

### Recommendation for Your Goals

**Stick with separate wildgen tool**, but enhance runtime capabilities:

#### Phase 1: Runtime Modifications (3-4 weeks)
As detailed in the "Runtime Map Modifications" section:
- Modify `pWilds->map[]` directly (doesn't touch PNG or .are files)
- Add dynamic features (bandit camps, floods, etc.)
- Persist changes via JSON state files
- **No PNG handling needed**

#### Phase 2: OLC-Style Terrain Editing (Future)
If you want in-game editing later:
```c
// Edit working map directly, no PNG involved
void do_setterrain(CHAR_DATA *ch, char *argument) {
    char arg[MAX_INPUT_LENGTH];
    argument = one_argument(argument, arg);
    
    if (!IN_WILDERNESS(ch) || !IS_BUILDER(ch)) return;
    
    char terrain_char = arg[0];
    WILDS_DATA *pWilds = ch->in_room->wilds;
    
    if (!get_terrain_by_token(pWilds, terrain_char)) {
        send_to_char("Invalid terrain type.\n\r", ch);
        return;
    }
    
    // Modify working map
    set_wilds_terrain(pWilds, ch->in_room->x, ch->in_room->y, terrain_char);
    
    // Optionally save to custom .are (not PNG)
    if (!str_cmp(argument, "save")) {
        save_wilds(pWilds);  // Saves modified map to .are file
    }
    
    send_to_char("Terrain changed.\n\r", ch);
}
```

This gives you:
- Runtime modification ✅
- OLC-style editing ✅
- No PNG dependency ✅
- Changes persist via `.are` or JSON ✅

#### Phase 3: Procedural Expansion (Far Future)
If you want to expand wilderness dynamically:
```c
// Generate new wilderness sections procedurally (no PNG)
WILDS_DATA *expand_wilderness(WILDS_DATA *existing, int direction) {
    WILDS_DATA *new_section = new_wilds();
    
    // Use noise functions, not PNG
    generate_terrain_procedural(new_section, 
                                existing->seed + direction,
                                existing->biome_type);
    
    // Link to existing wilderness
    connect_wilderness_sections(existing, new_section, direction);
    
    return new_section;
}
```

**No PNG needed - pure algorithmic generation.**

### When PNG Integration Makes Sense

Only integrate PNG handling if you need:
1. **Player-created maps** - Players upload PNG files as custom areas
2. **Streaming world** - Load terrain from PNG tiles as players explore
3. **External map services** - Integration with web-based map editors
4. **Real-world data** - Import from GIS/satellite imagery

**Your use case doesn't require any of these.**

### Technical Debt Consideration

If you do integrate PNG later, use modern alternatives:

```c
// Instead of libpng directly, use stb_image.h (single-header, public domain)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char *img = stbi_load("map.png", &width, &height, &channels, 4);
// ... process ...
stbi_image_free(img);
```

**Benefits:**
- No external dependency
- 1 header file (~7500 lines, but self-contained)
- Public domain license
- Used by many game engines

### Summary

| Aspect | Separate wildgen | Integrated PNG |
|--------|------------------|----------------|
| **Your runtime mods** | ✅ Sufficient | ❌ Overkill |
| **Complexity** | ✅ Low | ❌ High |
| **Security** | ✅ No PNG parsing | ❌ Attack surface |
| **Boot time** | ✅ Fast | ❌ Slower |
| **Dependencies** | ✅ None in game | ❌ libpng/stb_image |
| **Editing tools** | ✅ Photoshop/GIMP | ❌ Custom needed |
| **Version control** | ✅ .are files diff-able | ❌ Binary PNGs |
| **Development speed** | ✅ Mature workflow | ❌ New tooling |

**Recommendation:** Keep wildgen separate. Focus development effort on:
1. Runtime modification API (affects `map[]`)
2. Wilderness state persistence
3. Weather/feature integration
4. Optional: OLC terrain editor (edits `map[]`, saves to `.are`)

This gives you all the dynamic world features you want without the complexity/risk of PNG handling in the game server.

---

## Wilderness Persistence in JSON World

### The Challenge

You're migrating to JSON zones, which raises the question: **How do wilderness areas reference their map data?**

**Current System:**
```
/sentience/area/nibswilds.are  (35MB monolithic file)
  - Contains area metadata
  - Embeds full terrain map (embedded in .are)
  - Includes terrain definitions
  - Contains vlink data
```

**Problem with JSON Migration:**
```json
// data/areas/wilderness.json
{
  "uid": 6,
  "name": "The Wilderness",
  "map_data": "..................................."  // 35MB of text? 😱
}
```

This is unwieldy. You need a **clean separation**.

### Recommended Architecture

**Separate map data from area metadata:**

```
data/
  areas/
    wilderness.json          (Area metadata - 5KB)
  maps/
    wilderness_6.wmap        (Binary map data - 1-2MB compressed)
    wilderness_6.wterr       (Terrain definitions - 10KB)
  wilderness_state/
    wilderness_6_state.json  (Runtime modifications - variable size)
```

### Proposed JSON Area Format

```json
{
  "format_version": 1,
  "area": {
    "uid": 6,
    "name": "The Wilderness",
    "filename": "wilderness.json",
    "type": "wilderness",
    "min_vnum": 20002,
    "max_vnum": 20002,
    "security": 9,
    "builders": "Nibelung Tieryo",
    
    "wilderness": {
      "uid": 6,
      "map_file": "wilderness_6.wmap",
      "terrain_file": "wilderness_6.wterr",
      "state_file": "wilderness_6_state.json",
      "map_size": {
        "x": 1538,
        "y": 1250
      },
      "start_position": {
        "x": 769,
        "y": 625
      },
      "repop": 15
    }
  },
  
  "vlinks": [
    {
      "uid": 1,
      "origin": {"x": 450, "y": 230},
      "direction": "north",
      "destination": {"area": 123, "vnum": 5001},
      "map_tile": "{YO",
      "linkage": "two_way"
    }
  ]
}
```

### Map File Format (.wmap)

**Option 1: Binary Compressed (Recommended)**

```c
// wilderness_map.h
#define WMAP_MAGIC      0x504D4157  // "WMAP"
#define WMAP_VERSION    1

typedef struct wmap_header {
    uint32_t magic;         // WMAP_MAGIC
    uint16_t version;       // WMAP_VERSION
    uint16_t flags;         // Reserved
    uint32_t width;         // Map width
    uint32_t height;        // Map height
    uint32_t map_offset;    // Offset to map data
    uint32_t map_size;      // Compressed size
    uint32_t map_uncompressed;  // Uncompressed size
} WMAP_HEADER;

// Structure:
// [WMAP_HEADER]
// [Compressed map data (zlib)]

bool load_wilderness_map(WILDS_DATA *pWilds, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return false;
    
    WMAP_HEADER header;
    fread(&header, sizeof(header), 1, fp);
    
    if (header.magic != WMAP_MAGIC) {
        fclose(fp);
        return false;
    }
    
    // Read compressed data
    unsigned char *compressed = malloc(header.map_size);
    fseek(fp, header.map_offset, SEEK_SET);
    fread(compressed, header.map_size, 1, fp);
    fclose(fp);
    
    // Decompress
    pWilds->staticmap = malloc(header.map_uncompressed);
    uncompress(pWilds->staticmap, &header.map_uncompressed,
               compressed, header.map_size);
    free(compressed);
    
    pWilds->map_size_x = header.width;
    pWilds->map_size_y = header.height;
    
    // Create working copy
    pWilds->map = malloc(header.map_uncompressed);
    memcpy(pWilds->map, pWilds->staticmap, header.map_uncompressed);
    
    return true;
}

bool save_wilderness_map(WILDS_DATA *pWilds, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return false;
    
    // Prepare header
    WMAP_HEADER header = {
        .magic = WMAP_MAGIC,
        .version = WMAP_VERSION,
        .flags = 0,
        .width = pWilds->map_size_x,
        .height = pWilds->map_size_y,
        .map_offset = sizeof(WMAP_HEADER),
        .map_uncompressed = pWilds->map_size_x * pWilds->map_size_y
    };
    
    // Compress map
    uLongf compressed_size = compressBound(header.map_uncompressed);
    unsigned char *compressed = malloc(compressed_size);
    compress2(compressed, &compressed_size, 
              (unsigned char*)pWilds->staticmap,
              header.map_uncompressed, 9);  // Max compression
    
    header.map_size = compressed_size;
    
    // Write file
    fwrite(&header, sizeof(header), 1, fp);
    fwrite(compressed, compressed_size, 1, fp);
    fclose(fp);
    
    free(compressed);
    return true;
}
```

**Benefits:**
- ✅ Fast loading (zlib uncompress is quick)
- ✅ Compact (35MB → 1-2MB compressed)
- ✅ No PNG dependency
- ✅ Simple format

**Option 2: Text Format (Development)**

For human-readable debugging/diffs during development:

```
# Wilderness Map v1
# Size: 1538x1250
# Generated by wildgen from wilds_200612.png
FFFFFFFF...  # Row 0
FFFFFFFF...  # Row 1
...
```

Can compress with gzip for storage.

### Terrain Definition File (.wterr)

**JSON format for terrain templates:**

```json
{
  "format_version": 1,
  "terrains": [
    {
      "tile": "F",
      "name": "Blackmoore Forest",
      "showchar": "{g*",
      "showname": "Blackmoore Forest",
      "briefdesc": "Towering trees surround you.",
      "sector": "forest",
      "room_flags": ["dark"],
      "move_cost": 2
    },
    {
      "tile": "~",
      "name": "Bay of Plenty",
      "showchar": "{B~",
      "showname": "Bay of Plenty",
      "briefdesc": "You are floating in deep water.",
      "sector": "water_noswim",
      "room_flags": ["no_mob"],
      "move_cost": 4
    }
  ]
}
```

**Why separate from map data:**
- Terrain definitions are small (10-20KB)
- Can be edited without touching map data
- Can be shared across multiple maps
- Easy to version control (JSON diffs nicely)

### Wilderness State File (Runtime Mods)

```json
{
  "format_version": 1,
  "wilderness_uid": 6,
  "last_modified": 1738147200,
  
  "features": [
    {
      "uid": 1001,
      "type": "bandit_camp",
      "position": {"x": 450, "y": 678},
      "radius": 2,
      "terrain_override": "C",
      "duration": -1,
      "created": 1738000000,
      "data": {
        "bandit_count": 8,
        "difficulty": 5,
        "treasure_level": 3
      }
    },
    {
      "uid": 1002,
      "type": "flood",
      "position": {"x": 890, "y": 234},
      "radius": 5,
      "terrain_override": "~",
      "duration": 32,
      "created": 1738100000
    }
  ],
  
  "storms": [
    {
      "type": "rain_storm",
      "position": {"x": 700, "y": 400},
      "radius": 15,
      "velocity": {"dx": 0.5, "dy": -0.3},
      "life": 75,
      "water_accumulation": 8
    }
  ],
  
  "npc_ships": [
    {
      "uid": 5001,
      "type": "pirate_sloop",
      "name": "Black Pearl",
      "position": {"x": 1200, "y": 800},
      "heading": 45,
      "speed": 3,
      "patrol_route": [
        {"x": 1200, "y": 800},
        {"x": 1300, "y": 900},
        {"x": 1250, "y": 1000}
      ]
    }
  ]
}
```

### Integration with wildgen

**Modify wildgen output:**

```c
// wildgen.c - new output format
void output_wilderness_files(void) {
    char mapfile[256], terrfile[256], jsonfile[256];
    
    sprintf(mapfile, "%s.wmap", areaname);
    sprintf(terrfile, "%s.wterr", areaname);
    sprintf(jsonfile, "%s.json", areaname);
    
    // Write binary map
    save_wilderness_map_binary(mapfile);
    
    // Write terrain definitions as JSON
    save_terrain_definitions_json(terrfile);
    
    // Write area metadata as JSON
    save_area_metadata_json(jsonfile);
    
    printf("Generated:\n");
    printf("  %s (map data)\n", mapfile);
    printf("  %s (terrain defs)\n", terrfile);
    printf("  %s (area metadata)\n", jsonfile);
}
```

**No wildgen changes needed immediately:**
- Can still output .are files
- Add new format as option: `wildgen --json`
- Conversion tool: `./tools/convert_wilderness nibswilds.are`

### Loading Sequence

```c
// db.c - modified area loading
AREA_DATA *load_area_json(const char *filename) {
    json_t *root = json_load_file(filename, 0, NULL);
    if (!root) return NULL;
    
    AREA_DATA *area = json_to_area(root);
    
    // Check if wilderness area
    json_t *wilderness = json_object_get(json_object_get(root, "area"), 
                                         "wilderness");
    if (wilderness) {
        const char *map_file = json_string_value(
            json_object_get(wilderness, "map_file"));
        const char *terrain_file = json_string_value(
            json_object_get(wilderness, "terrain_file"));
        const char *state_file = json_string_value(
            json_object_get(wilderness, "state_file"));
        
        // Load wilderness components
        WILDS_DATA *pWilds = new_wilds();
        pWilds->pArea = area;
        pWilds->uid = json_integer_value(
            json_object_get(wilderness, "uid"));
        
        // Load map data
        char map_path[512];
        sprintf(map_path, "data/maps/%s", map_file);
        if (!load_wilderness_map(pWilds, map_path)) {
            plogf(LOG_ERROR, "Failed to load wilderness map: %s", map_path);
            free_wilds(pWilds);
            return area;
        }
        
        // Load terrain definitions
        char terr_path[512];
        sprintf(terr_path, "data/maps/%s", terrain_file);
        load_terrain_definitions_json(pWilds, terr_path);
        
        // Load runtime state (if exists)
        if (state_file) {
            char state_path[512];
            sprintf(state_path, "data/wilderness_state/%s", state_file);
            load_wilderness_state_json(pWilds, state_path);
        }
        
        area->wilds = pWilds;
        
        // Add to loaded wilds list
        LLIST_WILDS_DATA *data = alloc_mem(sizeof(*data));
        data->wilds = pWilds;
        data->uid = pWilds->uid;
        list_appendlink(loaded_wilds, data);
    }
    
    json_decref(root);
    return area;
}
```

### Migration Path

**Phase 1: Add Binary Map Format (1 week)**
1. Implement `.wmap` file format
2. Add load/save functions to `wilds.c`
3. Test with nibswilds conversion
4. Keep .are format working

**Phase 2: Separate Terrain Definitions (3 days)**
1. Extract terrain to JSON format
2. Modify wildgen to output `.wterr` files
3. Add terrain loading from JSON

**Phase 3: JSON Area Integration (1 week)**
1. Add wilderness field to JSON area format
2. Modify area loading to handle wilderness
3. Test with converted nibswilds
4. Update OLC to save to new format

**Phase 4: Cleanup (3 days)**
1. Convert all wilderness areas
2. Update area.lst
3. Remove old .are parsing for wilderness
4. Documentation

### File Size Comparison

**Current System:**
```
area/nibswilds.are              35.0 MB  (monolithic)
```

**New System:**
```
data/areas/wilderness.json       5.0 KB  (metadata)
data/maps/wilderness_6.wmap      1.8 MB  (compressed map)
data/maps/wilderness_6.wterr    12.0 KB  (terrain defs)
data/wilderness_state/
  wilderness_6_state.json      Variable  (runtime state)
--------------------------------
Total static data:             ~1.82 MB  (95% reduction!)
```

### Benefits

1. **Clean Separation**
   - ✅ Metadata in JSON (version controlled, diffable)
   - ✅ Map data in binary (compact, fast)
   - ✅ Runtime state separate (can be reset independently)

2. **Performance**
   - ✅ Faster loading (compressed binary)
   - ✅ Less memory (better compression)
   - ✅ Cacheable (Redis can cache metadata, not 35MB)

3. **Development**
   - ✅ Edit area settings without touching map
   - ✅ Regenerate map without losing runtime state
   - ✅ Multiple maps can share terrain definitions

4. **Runtime Modifications**
   - ✅ State saved separately from base map
   - ✅ Can reset to staticmap easily
   - ✅ Can backup/restore states independently

5. **Version Control**
   - ✅ JSON metadata diffs cleanly
   - ✅ Binary maps are opaque (expected)
   - ✅ State files can be .gitignored

### Wildgen Integration

**No immediate changes needed:**
- Wildgen continues generating as-is
- Add conversion tool post-generation:
  ```bash
  cd /sentience/tools/maps/wilds
  ./wildgen                    # Generates nibswilds.are
  ../../convert_wilderness nibswilds.are  # Converts to new format
  ```

**Future enhancement (optional):**
```bash
./wildgen --format=json nibswilds.ini
# Generates:
#   wilderness_6.json
#   wilderness_6.wmap
#   wilderness_6.wterr
```

### Conversion Tool

```c
// tools/convert_wilderness.c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: convert_wilderness <input.are>\n");
        return 1;
    }
    
    // Load old .are format
    AREA_DATA *area = read_area_old_format(argv[1]);
    if (!area) {
        fprintf(stderr, "Failed to read %s\n", argv[1]);
        return 1;
    }
    
    WILDS_DATA *pWilds = area->wilds;
    if (!pWilds) {
        fprintf(stderr, "Not a wilderness area\n");
        return 1;
    }
    
    char basename[256];
    get_basename(argv[1], basename);
    
    // Save new format
    char mapfile[512], terrfile[512], jsonfile[512];
    sprintf(mapfile, "data/maps/%s.wmap", basename);
    sprintf(terrfile, "data/maps/%s.wterr", basename);
    sprintf(jsonfile, "data/areas/%s.json", basename);
    
    save_wilderness_map(pWilds, mapfile);
    save_terrain_definitions_json(pWilds, terrfile);
    save_area_json(area, pWilds, jsonfile);
    
    printf("Converted %s:\n", argv[1]);
    printf("  -> %s\n", mapfile);
    printf("  -> %s\n", terrfile);
    printf("  -> %s\n", jsonfile);
    
    return 0;
}
```

---

## Runtime Modifications vs. Map Regeneration

### The Problem

**Scenario:**
1. Designer adds bandit camps via in-game commands
2. State saved to `wilderness_6_state.json` ✅
3. Designer edits PNG map in Photoshop (adds new forest area)
4. Designer runs wildgen → regenerates `.wmap` and `.wterr`
5. **Runtime modifications lost!** ❌

**Root Cause:** No merge strategy between source data (PNG/wildgen) and runtime state.

### Solution: Layered Data Architecture

**Principle:** Separate what comes from wildgen (source) from what's added in-game (derived).

```
Source Data (from wildgen):
  wilderness_6.wmap        ← Base terrain map
  wilderness_6.wterr       ← Terrain definitions
  
Runtime State (from game):
  wilderness_6_state.json  ← Bandit camps, floods, etc.
  wilderness_6_mods.json   ← Permanent terrain edits (optional)
```

### Three Types of Modifications

**1. Temporary Features (weather, spawns)**
- Stored in `_state.json`
- **Discarded on wildgen regeneration** (by design)
- Examples: floods, fires, temporary camps
- Rationale: These are meant to be temporary anyway

**2. Permanent Features (quest-triggered changes)**
- Stored in `_state.json` with `permanent: true` flag
- **Preserved across wildgen regeneration**
- Examples: quest-cleared dark forests, permanent settlements
- Rationale: Player actions should persist

**3. Terrain Edits (OLC-style changes)**
- Stored in `_mods.json` (separate file)
- **Applied after loading base map**
- Examples: Builder adds custom terrain patches
- Rationale: Design iterations without re-running wildgen

### Enhanced State File Format

```json
{
  "format_version": 1,
  "wilderness_uid": 6,
  "base_map_checksum": "a3f9b2c1...",  ← Track source version
  "last_modified": 1738147200,
  
  "features": [
    {
      "uid": 1001,
      "type": "bandit_camp",
      "position": {"x": 450, "y": 678},
      "radius": 2,
      "permanent": false,  ← Temporary (cleared on regen)
      "duration": -1,
      "created": 1738000000
    },
    {
      "uid": 1002,
      "type": "settlement",
      "position": {"x": 890, "y": 234},
      "radius": 5,
      "permanent": true,  ← Permanent (preserved on regen)
      "created": 1738000000,
      "reason": "Quest: Rebuild the Village completed"
    }
  ],
  
  "storms": [
    // Always temporary
  ],
  
  "npc_ships": [
    // Always temporary
  ]
}
```

### Terrain Modifications File

```json
{
  "format_version": 1,
  "wilderness_uid": 6,
  "base_map_checksum": "a3f9b2c1...",
  "modifications": [
    {
      "position": {"x": 500, "y": 600},
      "old_terrain": "F",
      "new_terrain": "C",
      "reason": "Added custom camp location",
      "author": "Tieryo",
      "timestamp": 1738100000,
      "permanent": true
    },
    {
      "area": {
        "start": {"x": 700, "y": 800},
        "end": {"x": 720, "y": 820}
      },
      "new_terrain": ".",
      "reason": "Created custom path",
      "author": "Nibelung",
      "timestamp": 1738110000,
      "permanent": true
    }
  ]
}
```

### Loading Sequence with Merging

```c
bool load_wilderness_with_state(AREA_DATA *area, json_t *json) {
    WILDS_DATA *pWilds = new_wilds();
    
    // 1. Load base map from wildgen output
    const char *map_file = json_string_value(
        json_object_get(wilderness, "map_file"));
    char map_path[512];
    sprintf(map_path, "data/maps/%s", map_file);
    
    if (!load_wilderness_map(pWilds, map_path)) {
        plogf(LOG_ERROR, "Failed to load base map: %s", map_path);
        return false;
    }
    
    // Calculate checksum of base map
    char base_checksum[33];
    calculate_map_checksum(pWilds->staticmap, 
                          pWilds->map_size_x * pWilds->map_size_y,
                          base_checksum);
    
    // 2. Load terrain modifications (if any)
    char mods_path[512];
    sprintf(mods_path, "data/wilderness_state/wilderness_%ld_mods.json", 
            pWilds->uid);
    
    if (file_exists(mods_path)) {
        json_t *mods = json_load_file(mods_path, 0, NULL);
        if (mods) {
            const char *saved_checksum = json_string_value(
                json_object_get(mods, "base_map_checksum"));
            
            if (strcmp(saved_checksum, base_checksum) == 0) {
                // Checksums match - apply modifications
                apply_terrain_modifications(pWilds, mods);
                plogf(LOG_INFO, "Applied %d terrain modifications",
                     json_array_size(json_object_get(mods, "modifications")));
            } else {
                // Base map changed - warn and skip
                plogf(LOG_WARN, 
                     "Base map changed (checksum mismatch), "
                     "terrain modifications NOT applied. "
                     "Backed up to %s.backup", mods_path);
                
                // Backup old mods file
                char backup_path[512];
                sprintf(backup_path, "%s.backup.%ld", 
                       mods_path, current_time);
                rename(mods_path, backup_path);
            }
            json_decref(mods);
        }
    }
    
    // 3. Load runtime state (features, storms, ships)
    char state_path[512];
    sprintf(state_path, "data/wilderness_state/wilderness_%ld_state.json",
           pWilds->uid);
    
    if (file_exists(state_path)) {
        json_t *state = json_load_file(state_path, 0, NULL);
        if (state) {
            const char *saved_checksum = json_string_value(
                json_object_get(state, "base_map_checksum"));
            
            if (strcmp(saved_checksum, base_checksum) == 0) {
                // Checksums match - apply all state
                apply_wilderness_state(pWilds, state);
            } else {
                // Base map changed - only apply permanent features
                plogf(LOG_WARN,
                     "Base map changed, applying only permanent features");
                apply_permanent_features_only(pWilds, state);
                
                // Update checksum and save cleaned state
                json_object_set_new(state, "base_map_checksum",
                                   json_string(base_checksum));
                json_dump_file(state, state_path, JSON_INDENT(2));
            }
            json_decref(state);
        }
    }
    
    return true;
}
```

### Builder Commands for Permanent Changes

```c
// Mark a feature as permanent
void do_makepermanent(CHAR_DATA *ch, char *argument) {
    char arg[MAX_INPUT_LENGTH];
    argument = one_argument(argument, arg);
    
    if (!IS_BUILDER(ch, ch->in_room->area)) {
        send_to_char("You must be a builder here.\n\r", ch);
        return;
    }
    
    if (!IN_WILDERNESS(ch)) {
        send_to_char("You must be in the wilderness.\n\r", ch);
        return;
    }
    
    long feature_uid = atol(arg);
    WILDERNESS_FEATURE *feat = find_feature_by_uid(
        ch->in_room->wilds, feature_uid);
    
    if (!feat) {
        send_to_char("Feature not found.\n\r", ch);
        return;
    }
    
    feat->permanent = true;
    save_wilderness_state(ch->in_room->wilds);
    
    send_to_char("Feature marked as permanent.\n\r", ch);
    plogf(LOG_OLC, "%s marked feature %ld as permanent at (%d,%d)",
         ch->name, feature_uid, ch->in_room->x, ch->in_room->y);
}

// Edit terrain permanently (saved to _mods.json)
void do_setterrain_permanent(CHAR_DATA *ch, char *argument) {
    char terrain_arg[MAX_INPUT_LENGTH];
    argument = one_argument(argument, terrain_arg);
    
    if (!IS_BUILDER(ch, ch->in_room->area)) {
        send_to_char("You must be a builder here.\n\r", ch);
        return;
    }
    
    if (!IN_WILDERNESS(ch)) {
        send_to_char("You must be in the wilderness.\n\r", ch);
        return;
    }
    
    char terrain_char = terrain_arg[0];
    WILDS_DATA *pWilds = ch->in_room->wilds;
    
    if (!get_terrain_by_token(pWilds, terrain_char)) {
        send_to_char("Invalid terrain type.\n\r", ch);
        return;
    }
    
    // Get old terrain
    long index = ch->in_room->y * pWilds->map_size_x + ch->in_room->x;
    char old_terrain = pWilds->map[index];
    
    // Apply change
    set_wilds_terrain(pWilds, ch->in_room->x, ch->in_room->y, terrain_char);
    
    // Save to modifications file
    save_terrain_modification(pWilds, 
                             ch->in_room->x, ch->in_room->y,
                             old_terrain, terrain_char,
                             ch->name, argument);  // reason
    
    send_to_char("Terrain changed permanently.\n\r", ch);
    plogf(LOG_OLC, "%s permanently changed terrain at (%d,%d) from '%c' to '%c'",
         ch->name, ch->in_room->x, ch->in_room->y, old_terrain, terrain_char);
}
```

### Wildgen Regeneration Workflow

**Best Practice Process:**

```bash
#!/bin/bash
# tools/maps/wilds/regenerate_map.sh

MAP_NAME="wilderness_6"
PNG_FILE="wilds_200612"
CONFIG="nibswilds.ini"

echo "Regenerating wilderness map..."

# 1. Backup current state
echo "Backing up current state..."
cp ../../data/wilderness_state/${MAP_NAME}_state.json \
   ../../data/wilderness_state/${MAP_NAME}_state.json.backup

cp ../../data/wilderness_state/${MAP_NAME}_mods.json \
   ../../data/wilderness_state/${MAP_NAME}_mods.json.backup

# 2. Run wildgen to regenerate base map
echo "Running wildgen..."
./wildgen ${CONFIG}

# 3. Convert to new format
echo "Converting to binary format..."
../../convert_wilderness nibswilds.are

# 4. Calculate new checksum
echo "Calculating checksum..."
NEW_CHECKSUM=$(../../calc_map_checksum ../../data/maps/${MAP_NAME}.wmap)

# 5. Update state file with new checksum
echo "Updating state file..."
jq ".base_map_checksum = \"${NEW_CHECKSUM}\"" \
   ../../data/wilderness_state/${MAP_NAME}_state.json > temp.json
mv temp.json ../../data/wilderness_state/${MAP_NAME}_state.json

# 6. Warn about modifications file
echo ""
echo "WARNING: Terrain modifications file needs manual review!"
echo "  Old: ${MAP_NAME}_mods.json.backup"
echo "  Current: ${MAP_NAME}_mods.json"
echo ""
echo "The game will detect the checksum mismatch and:"
echo "  1. Skip applying old terrain modifications"
echo "  2. Create a backup of the mods file"
echo "  3. Apply only permanent features from state file"
echo ""
echo "Review the backup and manually re-apply any needed modifications."
```

### State Management Commands

```c
// In-game admin commands for state management

void do_wildstate(CHAR_DATA *ch, char *argument) {
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    
    if (arg1[0] == '\0') {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  wildstate list               - List all features\n\r", ch);
        send_to_char("  wildstate clear temp         - Clear temporary features\n\r", ch);
        send_to_char("  wildstate clear all          - Clear ALL features (permanent too)\n\r", ch);
        send_to_char("  wildstate save               - Force save state\n\r", ch);
        send_to_char("  wildstate reload             - Reload from disk\n\r", ch);
        send_to_char("  wildstate checksum           - Show base map checksum\n\r", ch);
        return;
    }
    
    if (!IN_WILDERNESS(ch)) {
        send_to_char("You must be in the wilderness.\n\r", ch);
        return;
    }
    
    WILDS_DATA *pWilds = ch->in_room->wilds;
    
    if (!str_cmp(arg1, "list")) {
        list_wilderness_features(ch, pWilds);
    }
    else if (!str_cmp(arg1, "clear")) {
        if (!str_cmp(arg2, "temp")) {
            clear_temporary_features(pWilds);
            send_to_char("Temporary features cleared.\n\r", ch);
        }
        else if (!str_cmp(arg2, "all")) {
            clear_all_features(pWilds);
            send_to_char("All features cleared.\n\r", ch);
        }
    }
    else if (!str_cmp(arg1, "save")) {
        save_wilderness_state(pWilds);
        send_to_char("State saved.\n\r", ch);
    }
    else if (!str_cmp(arg1, "reload")) {
        reload_wilderness_state(pWilds);
        send_to_char("State reloaded.\n\r", ch);
    }
    else if (!str_cmp(arg1, "checksum")) {
        char checksum[33];
        calculate_map_checksum(pWilds->staticmap,
                              pWilds->map_size_x * pWilds->map_size_y,
                              checksum);
        char buf[256];
        sprintf(buf, "Base map checksum: %s\n\r", checksum);
        send_to_char(buf, ch);
    }
}
```

### Summary: Data Flow

**Initial Setup (wildgen):**
```
PNG files → wildgen → wilderness_6.wmap (source of truth)
                   ↓
              wilderness_6.wterr (terrain defs)
                   ↓
              wilderness_6.json (area metadata)
```

**Runtime (game server):**
```
Load wilderness_6.wmap
  ↓
Apply wilderness_6_mods.json (if checksum matches)
  ↓
Apply wilderness_6_state.json (permanent features always, temp if checksum matches)
  ↓
Players interact, create features
  ↓
Save to wilderness_6_state.json periodically
```

**Map Regeneration (designer):**
```
Edit PNG → wildgen → new wilderness_6.wmap
  ↓
Checksum changes
  ↓
Game detects mismatch on next boot:
  - Backs up old _mods.json
  - Applies only permanent features from _state.json
  - Updates checksum in _state.json
  ↓
Designer reviews backed-up mods
  ↓
Designer manually re-applies needed modifications
```

### Benefits of This Approach

1. **Clear Separation**
   - ✅ Source data (wildgen) separate from derived data (game state)
   - ✅ Wildgen can regenerate freely
   - ✅ Runtime modifications tracked explicitly

2. **Flexible Persistence**
   - ✅ Temporary features automatically cleared (floods, fires)
   - ✅ Permanent features survive regeneration (quest changes)
   - ✅ Terrain edits tracked separately with rationale

3. **Safety**
   - ✅ Checksum detection prevents applying stale modifications
   - ✅ Automatic backups on mismatch
   - ✅ Manual review process for conflicts

4. **Developer-Friendly**
   - ✅ Iterate on PNG maps freely
   - ✅ State preserved where appropriate
   - ✅ Clear audit trail of modifications

---

## Comparison: Static vs Virtual Wilderness

| Aspect | Static Rooms | Virtual Rooms (Current) |
|--------|-------------|------------------------|
| Memory | High (all loaded) | Low (on-demand) |
| Performance | Fast lookups | Room creation overhead |
| Persistence | Full | None (lost on unload) |
| Weather | Easy integration | Broken |
| NPC Movement | Simple | Complex/broken |
| Dynamic Content | Fully supported | Limited/fragile |
| Complexity | Low | High |

---

## Runtime Map Modifications

### Current Infrastructure

The system **already has** the foundation for runtime map changes:

**Dual Map System:**
```c
char *staticmap;  // Permanent terrain from .are file (never changes)
char *map;        // Working copy - modified at runtime
```

**On Load:**
```c
// wilds.c line 498
memcpy(pWilds->map, pWilds->staticmap, pWilds->map_size_x * pWilds->map_size_y);
```

**Comment Evidence:**
```c
/* Vizz - map a copy of the staticmap to apply things like vlinks, 
   flooding etc to */
```

### What's Missing

**Problem:** Changes to `pWilds->map[]` are:
1. ❌ Not persisted (lost on server restart)
2. ❌ Not saved to disk
3. ❌ Reset on area repop
4. ❌ No API for safe modification

**Result:** The infrastructure exists but isn't used for dynamic content.

### Proposed Runtime Modification System

#### 1. Map Modification API

Add to `wilds.c`:

```c
// Set terrain at coordinate (modifies working map only)
bool set_wilds_terrain(WILDS_DATA *pWilds, int x, int y, char terrain_char) {
    if (!pWilds || x < 0 || y < 0 || 
        x >= pWilds->map_size_x || y >= pWilds->map_size_y)
        return false;
    
    long index = y * pWilds->map_size_x + x;
    char old_terrain = pWilds->map[index];
    pWilds->map[index] = terrain_char;
    
    // If room is loaded, update it
    ROOM_INDEX_DATA *room = get_wilds_vroom(pWilds, x, y);
    if (room && IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) {
        WILDS_TERRAIN *new_terrain = get_terrain_by_token(pWilds, terrain_char);
        if (new_terrain && new_terrain->template) {
            // Update room properties from new terrain
            free_string(room->name);
            room->name = str_dup(new_terrain->template->name);
            room->sector_type = new_terrain->template->sector_type;
            room->room_flag[0] = new_terrain->template->room_flag[0];
            room->parent_template = new_terrain;
        }
    }
    
    // Log the change for persistence
    log_map_change(pWilds, x, y, old_terrain, terrain_char, "manual");
    
    return true;
}

// Restore terrain to original (from staticmap)
bool restore_wilds_terrain(WILDS_DATA *pWilds, int x, int y) {
    if (!pWilds || x < 0 || y < 0 || 
        x >= pWilds->map_size_x || y >= pWilds->map_size_y)
        return false;
    
    long index = y * pWilds->map_size_x + x;
    char original = pWilds->staticmap[index];
    return set_wilds_terrain(pWilds, x, y, original);
}

// Fill area with terrain (for floods, fires, etc.)
int fill_wilds_area(WILDS_DATA *pWilds, int cx, int cy, int radius, 
                    char terrain_char, int chance) {
    int count = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            int dist_sq = (x-cx)*(x-cx) + (y-cy)*(y-cy);
            if (dist_sq <= radius*radius && number_percent() < chance) {
                if (set_wilds_terrain(pWilds, x, y, terrain_char))
                    count++;
            }
        }
    }
    return count;
}
```

#### 2. Dynamic Feature System

**Structure for Temporary/Persistent Features:**

```c
// In wilderness_state.h (NEW)
typedef struct wilderness_feature {
    struct wilderness_feature *next;
    long uid;
    WILDS_DATA *wilds;
    int x, y;                    // Center position
    int radius;                  // Affected area
    char terrain_override;       // What terrain to show
    int duration;                // -1 = permanent, 0+ = ticks remaining
    int type;                    // FEATURE_BANDIT_CAMP, FEATURE_FLOOD, etc.
    void *data;                  // Type-specific data
    time_t created;
    char *description;
} WILDERNESS_FEATURE;

#define FEATURE_BANDIT_CAMP      1
#define FEATURE_FLOOD            2
#define FEATURE_FIRE_DAMAGE      3
#define FEATURE_LANDSLIDE        4
#define FEATURE_EARTHQUAKE_CRACK 5
#define FEATURE_ICE_SHEET        6
#define FEATURE_QUICKSAND        7
#define FEATURE_TREASURE_SITE    8
```

**Feature Application:**

```c
WILDERNESS_FEATURE *create_wilderness_feature(WILDS_DATA *pWilds, 
                                               int x, int y, int type) {
    WILDERNESS_FEATURE *feat = alloc_mem(sizeof(*feat));
    feat->uid = next_feature_uid++;
    feat->wilds = pWilds;
    feat->x = x;
    feat->y = y;
    feat->type = type;
    feat->created = current_time;
    
    switch (type) {
        case FEATURE_BANDIT_CAMP:
            feat->radius = 2;
            feat->terrain_override = 'C';  // Camp terrain
            feat->duration = -1;  // Permanent until cleared
            feat->data = create_bandit_camp_data();
            feat->description = str_dup("Bandit Camp");
            // Apply terrain changes
            fill_wilds_area(pWilds, x, y, feat->radius, 'C', 100);
            // Spawn bandits
            spawn_bandit_npcs(pWilds, x, y, feat->radius);
            break;
            
        case FEATURE_FLOOD:
            feat->radius = 5;
            feat->terrain_override = '~';  // Water
            feat->duration = 48;  // 48 ticks (~24 game hours)
            feat->description = str_dup("Flooded Area");
            // Convert passable terrain to shallow water
            flood_terrain(pWilds, x, y, feat->radius);
            break;
            
        case FEATURE_FIRE_DAMAGE:
            feat->radius = 3;
            feat->terrain_override = '.';  // Burnt ground
            feat->duration = 100;  // Slow regeneration
            feat->description = str_dup("Fire Damage");
            // Convert forests to burnt terrain
            burn_terrain(pWilds, x, y, feat->radius);
            break;
    }
    
    // Add to wilderness state
    feat->next = pWilds->state->features;
    pWilds->state->features = feat;
    
    return feat;
}

void remove_wilderness_feature(WILDERNESS_FEATURE *feat) {
    // Restore original terrain
    for (int y = feat->y - feat->radius; y <= feat->y + feat->radius; y++) {
        for (int x = feat->x - feat->radius; x <= feat->x + feat->radius; x++) {
            restore_wilds_terrain(feat->wilds, x, y);
        }
    }
    
    // Remove NPCs if applicable
    if (feat->type == FEATURE_BANDIT_CAMP) {
        despawn_feature_npcs(feat);
    }
    
    // Free feature data
    if (feat->data) free_mem(feat->data);
    free_string(feat->description);
    free_mem(feat);
}
```

#### 3. Weather-Based Modifications

**Integration with Weather System:**

```c
// In weather.c - when storm moves
void apply_storm_effects(STORM_DATA *storm) {
    AREA_DATA *pArea = storm->area;
    WILDS_DATA *pWilds = pArea->wilds;
    
    if (!pWilds) return;
    
    switch (storm->storm_type) {
        case WEATHER_TORNADO:
            // Damage terrain in path
            if (number_percent() < 30) {
                int damage_x = storm->x + number_range(-2, 2);
                int damage_y = storm->y + number_range(-2, 2);
                create_wilderness_feature(pWilds, damage_x, damage_y, 
                                         FEATURE_FIRE_DAMAGE);
            }
            break;
            
        case WEATHER_RAIN_STORM:
            // Accumulate water - flood if too much
            storm->water_accumulation++;
            if (storm->water_accumulation > 10) {
                create_wilderness_feature(pWilds, storm->x, storm->y, 
                                         FEATURE_FLOOD);
                storm->water_accumulation = 0;
            }
            break;
            
        case WEATHER_LIGHTNING_STORM:
            // Random fires
            if (number_percent() < 5) {
                int fire_x = storm->x + number_range(-storm->radius, storm->radius);
                int fire_y = storm->y + number_range(-storm->radius, storm->radius);
                // Check if forest terrain
                WILDS_TERRAIN *terrain = get_terrain_by_coors(pWilds, fire_x, fire_y);
                if (terrain && terrain->template->sector_type == SECT_FOREST) {
                    create_wilderness_feature(pWilds, fire_x, fire_y, 
                                             FEATURE_FIRE_DAMAGE);
                }
            }
            break;
            
        case WEATHER_SNOW_STORM:
            // Temporary ice sheets
            fill_wilds_area(pWilds, storm->x, storm->y, 3, 'I', 50);
            break;
    }
}
```

#### 4. Event-Driven Modifications

**Quest/Plot Events:**

```c
// Triggered by quest completion, world events, etc.
void apply_world_event(int event_type, WILDS_DATA *pWilds, int x, int y) {
    switch (event_type) {
        case EVENT_EARTHQUAKE:
            // Create cracks and landslides
            for (int i = 0; i < number_range(5, 10); i++) {
                int cx = x + number_range(-20, 20);
                int cy = y + number_range(-20, 20);
                create_wilderness_feature(pWilds, cx, cy, 
                                         FEATURE_EARTHQUAKE_CRACK);
            }
            break;
            
        case EVENT_BANDIT_INVASION:
            // Spawn multiple bandit camps
            for (int i = 0; i < number_range(3, 6); i++) {
                int cx = x + number_range(-50, 50);
                int cy = y + number_range(-50, 50);
                create_wilderness_feature(pWilds, cx, cy, 
                                         FEATURE_BANDIT_CAMP);
            }
            break;
            
        case EVENT_TREASURE_DISCOVERED:
            // Mark location for treasure hunters
            create_wilderness_feature(pWilds, x, y, 
                                     FEATURE_TREASURE_SITE);
            break;
    }
}
```

#### 5. Persistence System

**Save Map Changes:**

```c
// In wilderness_state.c
void save_wilderness_features(WILDS_DATA *pWilds) {
    char filename[256];
    sprintf(filename, "data/wilderness_features_%ld.json", pWilds->uid);
    
    json_t *root = json_object();
    json_t *features = json_array();
    
    for (WILDERNESS_FEATURE *feat = pWilds->state->features; 
         feat; feat = feat->next) {
        json_t *f = json_object();
        json_object_set_new(f, "uid", json_integer(feat->uid));
        json_object_set_new(f, "type", json_integer(feat->type));
        json_object_set_new(f, "x", json_integer(feat->x));
        json_object_set_new(f, "y", json_integer(feat->y));
        json_object_set_new(f, "radius", json_integer(feat->radius));
        json_object_set_new(f, "terrain", json_integer(feat->terrain_override));
        json_object_set_new(f, "duration", json_integer(feat->duration));
        json_object_set_new(f, "created", json_integer(feat->created));
        json_object_set_new(f, "description", json_string(feat->description));
        
        // Type-specific data
        switch (feat->type) {
            case FEATURE_BANDIT_CAMP:
                save_bandit_camp_data(f, feat->data);
                break;
            // ... other types
        }
        
        json_array_append_new(features, f);
    }
    
    json_object_set_new(root, "features", features);
    json_dump_file(root, filename, JSON_INDENT(2));
    json_decref(root);
}

void load_wilderness_features(WILDS_DATA *pWilds) {
    char filename[256];
    sprintf(filename, "data/wilderness_features_%ld.json", pWilds->uid);
    
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root) return;
    
    json_t *features = json_object_get(root, "features");
    if (!json_is_array(features)) {
        json_decref(root);
        return;
    }
    
    size_t index;
    json_t *feat_json;
    json_array_foreach(features, index, feat_json) {
        WILDERNESS_FEATURE *feat = alloc_mem(sizeof(*feat));
        feat->uid = json_integer_value(json_object_get(feat_json, "uid"));
        feat->type = json_integer_value(json_object_get(feat_json, "type"));
        feat->x = json_integer_value(json_object_get(feat_json, "x"));
        feat->y = json_integer_value(json_object_get(feat_json, "y"));
        feat->radius = json_integer_value(json_object_get(feat_json, "radius"));
        feat->terrain_override = json_integer_value(json_object_get(feat_json, "terrain"));
        feat->duration = json_integer_value(json_object_get(feat_json, "duration"));
        feat->created = json_integer_value(json_object_get(feat_json, "created"));
        feat->description = str_dup(json_string_value(json_object_get(feat_json, "description")));
        
        // Apply terrain changes
        fill_wilds_area(pWilds, feat->x, feat->y, feat->radius, 
                       feat->terrain_override, 100);
        
        // Load type-specific data
        switch (feat->type) {
            case FEATURE_BANDIT_CAMP:
                feat->data = load_bandit_camp_data(feat_json);
                spawn_bandit_npcs(pWilds, feat->x, feat->y, feat->radius);
                break;
            // ... other types
        }
        
        // Add to list
        feat->wilds = pWilds;
        feat->next = pWilds->state->features;
        pWilds->state->features = feat;
    }
    
    json_decref(root);
}
```

#### 6. Update Cycle

**Periodic Feature Updates:**

```c
// In update.c - called every game tick
void update_wilderness_features(void) {
    LLIST_WILDS_DATA *data;
    LLIST_ITERATOR it;
    
    iterator_start(&it, loaded_wilds);
    while ((data = iterator_nextdata(&it))) {
        WILDS_DATA *pWilds = data->wilds;
        WILDERNESS_FEATURE *feat, *feat_next;
        
        for (feat = pWilds->state->features; feat; feat = feat_next) {
            feat_next = feat->next;
            
            // Tick down duration
            if (feat->duration > 0) {
                feat->duration--;
                if (feat->duration <= 0) {
                    // Feature expired - remove it
                    remove_wilderness_feature(feat);
                    continue;
                }
            }
            
            // Type-specific updates
            switch (feat->type) {
                case FEATURE_FLOOD:
                    // Slowly drain
                    if (feat->duration % 10 == 0 && feat->radius > 1) {
                        feat->radius--;
                        // Restore outer edge
                        restore_flooded_edge(feat);
                    }
                    break;
                    
                case FEATURE_FIRE_DAMAGE:
                    // Slowly regrow
                    if (feat->duration % 20 == 0) {
                        regrow_burnt_terrain(feat);
                    }
                    break;
                    
                case FEATURE_BANDIT_CAMP:
                    // Check if cleared by players
                    if (bandit_camp_cleared(feat)) {
                        remove_wilderness_feature(feat);
                        // Maybe reward or trigger event
                    }
                    break;
            }
        }
    }
    iterator_stop(&it);
}
```

### Example Use Cases

#### Bandit Camp Creation

```c
// Builder command
void do_createcamp(CHAR_DATA *ch, char *argument) {
    if (!IN_WILDERNESS(ch)) {
        send_to_char("You must be in the wilderness.\n\r", ch);
        return;
    }
    
    WILDERNESS_FEATURE *camp = create_wilderness_feature(
        ch->in_room->wilds,
        ch->in_room->x,
        ch->in_room->y,
        FEATURE_BANDIT_CAMP
    );
    
    if (camp) {
        send_to_char("Bandit camp created.\n\r", ch);
        act("A bandit camp appears in the area!", ch, NULL, NULL, TO_ROOM);
    }
}
```

#### Weather Flooding

```c
// Automatic based on heavy rain
if (storm->storm_type == WEATHER_RAIN_STORM && 
    storm->life > 50) {  // Long-lasting storm
    
    // Find low-lying areas
    for (int y = storm->y - 10; y <= storm->y + 10; y++) {
        for (int x = storm->x - 10; x <= storm->x + 10; x++) {
            WILDS_TERRAIN *terrain = get_terrain_by_coors(pWilds, x, y);
            if (terrain && terrain->template->sector_type == SECT_FIELD) {
                // Fields flood easily
                if (number_percent() < 20) {
                    set_wilds_terrain(pWilds, x, y, '~');  // Temporary water
                }
            }
        }
    }
}
```

#### Quest-Triggered Changes

```c
// When player completes "Banish the Darkness" quest
void quest_complete_banish_darkness(CHAR_DATA *ch) {
    WILDS_DATA *pWilds = get_wilds_from_uid(NULL, 6);  // Main wilderness
    
    // Clear dark forest
    for (int y = 450; y <= 470; y++) {
        for (int x = 230; x <= 250; x++) {
            WILDS_TERRAIN *terrain = get_terrain_by_coors(pWilds, x, y);
            if (terrain && terrain->mapchar == 'D') {  // Dark forest
                set_wilds_terrain(pWilds, x, y, '$');  // Normal forest
            }
        }
    }
    
    send_to_char("The darkness lifts from the forest!\n\r", ch);
    // Notify all players in wilderness
    notify_wilderness_change(pWilds, "The forest to the east brightens!");
}
```

### Benefits of This System

1. ✅ **Dynamic World**: Wilderness changes based on events
2. ✅ **Weather Integration**: Storms have visible, lasting effects
3. ✅ **Player Impact**: Quests and actions change the world
4. ✅ **Persistence**: Changes saved and restored across reboots
5. ✅ **Performance**: Only affects `map`, not `staticmap` (can always revert)
6. ✅ **Flexible**: Easy to add new feature types

### Migration Notes

**Existing Code Compatibility:**
- Virtual rooms already read from `pWilds->map[]`
- Display functions already use `pWilds->map[]` for terrain
- No changes needed to core rendering
- VLINKs already override display with `map_tile`

**Implementation Priority:**
1. Add modification API functions (1 day)
2. Add feature system structures (2 days)
3. Integrate with weather (1 week)
4. Add persistence (3 days)
5. Create example features (1 week)
6. Builder commands (2 days)

**Estimated Effort:** 3-4 weeks for full system

---

## Proposed Solutions

### Option 1: Hybrid Persistence (Recommended)

**Concept:** Keep virtual rooms, but add persistent state layer

**Implementation:**
1. **Wilderness State Manager:**
   ```c
   struct wilderness_state {
       LLIST *active_storms;        // Storm positions/data
       LLIST *npc_ships;            // NPC ship positions
       LLIST *dynamic_objects;      // Spawned objects by coord
       LLIST *room_modifications;   // Terrain changes
       time_t last_update;
   };
   ```

2. **Room Creation Enhancement:**
   - When creating vroom, check `wilderness_state` for:
     - Active storms affecting coordinates
     - NPC ships in location
     - Dynamic objects to load
     - Terrain modifications
   - Apply state to new room

3. **Periodic Updates:**
   - Storm movement runs independently of room existence
   - NPC ships update positions in abstract coordinate space
   - Only load rooms when players nearby

4. **Save/Load System:**
   - Persist wilderness_state to JSON
   - Load at boot, save periodically
   - Much smaller than full room data

**Advantages:**
- Keeps memory benefits of virtual rooms
- Restores weather functionality
- Enables NPC ships without constant room loading
- Relatively low complexity increase

**Files to Modify:**
- `src/wilds.c` - Add state manager
- `src/weather.c` - Hook into state updates
- `src/boat.c` - Abstract ship positions
- `src/update.c` - Add periodic state updates

### Option 2: Sectored Static Rooms

**Concept:** Divide wilderness into sectors, load entire sectors as static

**Implementation:**
1. Define sector size (e.g., 50x50 tiles)
2. Load sectors when any player enters
3. Unload entire sector when empty
4. Reduces room count while maintaining persistence

**Advantages:**
- Simple room state management
- Weather/NPCs work normally
- Familiar to existing codebase

**Disadvantages:**
- Higher memory usage
- More room objects to manage
- Sector boundary edge cases

### Option 3: Lightweight Virtual Rooms

**Concept:** Keep rooms virtual but much lighter weight

**Implementation:**
1. Reduce `ROOM_INDEX_DATA` size for vrooms
2. Share terrain templates (already done)
3. Never destroy rooms with active state
4. Add `ROOM_HAS_STATE` flag to keep rooms alive

**Advantages:**
- Minimal code changes
- Keeps existing architecture

**Disadvantages:**
- Memory creep over time
- Doesn't fully solve the problem

---

## Recommended Implementation Plan

### Phase 1: Wilderness State Manager (2-3 weeks)

**Goal:** Decouple weather/NPCs from room existence

**Tasks:**
1. Create `wilderness_state.c` and `wilderness_state.h`
2. Define state structures for storms, ships, objects
3. Implement state save/load to JSON
4. Hook state into `create_vroom()` to apply on creation

**Files:**
```
src/wilderness_state.c      (NEW)
src/wilderness_state.h      (NEW)
src/wilds.c                 (MODIFY - room creation)
data/wilderness_state.json  (NEW - runtime data)
```

### Phase 2: Weather Integration (1-2 weeks)

**Goal:** Restore functional weather system

**Tasks:**
1. Uncomment/fix `update_weather()` in weather.c
2. Store storms in wilderness_state, not AREA_DATA
3. Apply storm effects when rooms are created
4. Add visual effects (rain messages, lightning)
5. Test weather impact on visibility

**Files:**
```
src/weather.c               (MODIFY - enable updates)
src/wilds.c                 (MODIFY - apply effects)
src/wilderness_state.c      (MODIFY - storm tracking)
```

### Phase 3: NPC Ships (2-3 weeks)

**Goal:** Add patrolling pirate ships

**Tasks:**
1. Design NPC ship AI (patrol routes, attacking)
2. Store NPC ship positions in wilderness_state
3. Implement ship-to-ship combat
4. Add treasure/loot system
5. Create pirate encounter generation

**Files:**
```
src/boat.c                  (MODIFY - NPC logic)
src/wilderness_state.c      (MODIFY - ship tracking)
src/fight.c                 (MODIFY - ship combat)
data/npc_ships.json         (NEW - ship definitions)
```

### Phase 4: Testing & Optimization (1-2 weeks)

**Tasks:**
1. Performance testing with multiple players
2. Memory leak detection
3. State persistence verification
4. Balance weather effects
5. Tune NPC ship behavior

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Memory creep from state | Medium | Medium | Regular cleanup, limits on state size |
| Performance degradation | Low | High | Profile before/after, optimize updates |
| Save/load corruption | Medium | High | JSON schema validation, backups |
| NPC ship pathfinding bugs | High | Medium | Extensive testing, fallback behaviors |
| Weather becoming annoying | Medium | Low | Player toggles, safe zones |

---

## Alternative: Simplified Approach

If full implementation is too complex, consider:

### Minimal Weather Restoration
1. Run weather updates only in "active sectors" (where players are)
2. Apply effects to loaded rooms only
3. Don't persist weather state across restarts
4. **Effort:** 1 week, **Impact:** Medium

### Static NPC Ships
1. Place NPC ships at fixed spawn points
2. They move in small local areas only
3. No long-distance patrolling
4. **Effort:** 1 week, **Impact:** Low-Medium

---

## Code Examples

### Example 1: State-based Storm Application

```c
// In wilds.c - when creating vroom
ROOM_INDEX_DATA *create_vroom(WILDS_DATA *pWilds, int x, int y, WILDS_TERRAIN *pTerrain) {
    ROOM_INDEX_DATA *pRoomIndex = new_room_index();
    
    // ... existing setup ...
    
    // NEW: Apply wilderness state
    STORM_DATA *storm = get_active_storm(pWilds, x, y);
    if (storm) {
        pRoomIndex->weather_effect = storm->storm_type;
        pRoomIndex->visibility_modifier = get_storm_visibility(storm);
    }
    
    // NEW: Load NPC ships in this location
    NPC_SHIP_DATA *npc_ship = get_npc_ship_at(pWilds, x, y);
    if (npc_ship) {
        spawn_ship_in_room(npc_ship, pRoomIndex);
    }
    
    return pRoomIndex;
}
```

### Example 2: Abstract Ship Position Updates

```c
// In boat.c - update ship without loading rooms
void update_npc_ship_position(NPC_SHIP_DATA *ship) {
    // Update coordinates in abstract space
    ship->coord.x += ship->velocity_x;
    ship->coord.y += ship->velocity_y;
    
    // Check for collisions with player ships
    LLIST_ITERATOR it;
    iterator_start(&it, loaded_player_ships);
    while((player_ship = iterator_nextdata(&it))) {
        if (abs(player_ship->coord.x - ship->coord.x) < 3 &&
            abs(player_ship->coord.y - ship->coord.y) < 3) {
            // Potential encounter - load rooms if not already loaded
            initiate_ship_encounter(ship, player_ship);
        }
    }
    iterator_stop(&it);
}
```

### Example 3: State Persistence

```c
// In wilderness_state.c
void save_wilderness_state(WILDS_DATA *pWilds) {
    json_t *root = json_object();
    json_t *storms = json_array();
    
    for (STORM_DATA *storm = pWilds->state->storms; storm; storm = storm->next) {
        json_t *storm_obj = json_object();
        json_object_set_new(storm_obj, "type", json_integer(storm->storm_type));
        json_object_set_new(storm_obj, "x", json_integer(storm->x));
        json_object_set_new(storm_obj, "y", json_integer(storm->y));
        json_object_set_new(storm_obj, "radius", json_integer(storm->radius));
        json_object_set_new(storm_obj, "life", json_integer(storm->life));
        json_array_append_new(storms, storm_obj);
    }
    
    json_object_set_new(root, "storms", storms);
    // ... save NPC ships, objects, etc ...
    
    char filename[256];
    sprintf(filename, "data/wilderness_state_%ld.json", pWilds->uid);
    json_dump_file(root, filename, JSON_INDENT(2));
    json_decref(root);
}
```

---

## Part 10: Web-Based Editor Considerations

### 10.1 Impact on Design Decisions

**What Changes**:
- ✅ **Visualization** - Graphical map display instead of ASCII art
- ✅ **UI/UX** - Point-and-click editing instead of text commands
- ✅ **Real-time preview** - See changes immediately with visual feedback
- ✅ **Tile painting** - Click/drag to paint terrain types
- ✅ **Multi-user editing** - Collaborative map building with live updates
- ✅ **Import/export** - Upload/download entire maps as files

**What Stays the Same**:
- ✅ **Data structures** - Backend storage format unchanged (or enhanced)
- ✅ **API layer** - Same commands/operations, different interface
- ✅ **Validation logic** - Rules enforced server-side
- ✅ **Performance** - Backend optimizations still critical
- ✅ **Game logic** - Movement, spawning, weather remain identical

**Architecture**: Web frontend is a **client** consuming the same backend APIs that the in-game editor uses.

### 10.2 Visual Map Editor UI

**Example Web Interface**:
```
┌─────────────────────────────────────────────────────────────┐
│ Wilderness Editor - Mystwood Forest                    [✕]  │
├─────────────────────────────────────────────────────────────┤
│ Toolbar: [🌲 Forest] [🌾 Plains] [⛰️ Mountain] [💧 Water]   │
│          [🏘️ Road] [🏰 City] [👤 Spawn] [📜 Script]        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─ Map View (100x100) ─────────────────────────┐        │
│   │ 🌲🌲🌲🌲🌲🌲🌾🌾🌾🌾                              │        │
│   │ 🌲🌲🌲🌲🌾🌾🌾🌾🌾🌾                              │        │
│   │ 🌲🌲⛰️⛰️🌾🌾🏘️🏘️🌾🌾                              │        │
│   │ 🌲⛰️⛰️⛰️⛰️🏘️🏘️🌾🌾🌾       [Selected: (45,32)]    │        │
│   │ ⛰️⛰️⛰️⛰️⛰️🏘️🏰🏘️🌾🌾       Terrain: Forest      │        │
│   │ 💧💧⛰️⛰️⛰️🌾🏘️🏘️🌾🌾       Flags: TREE, SHADE   │        │
│   │ 💧💧💧⛰️🌾🌾🌾🌾🌾🌾                              │        │
│   │                                                │        │
│   │ [Zoom: 100%] [Grid: On] [Labels: Off]         │        │
│   └────────────────────────────────────────────────┘        │
│                                                             │
│ ┌─ Properties ───────┐  ┌─ Layers ──────────────┐         │
│ │ Terrain: Forest    │  │ ☑ Terrain              │         │
│ │ Flags:             │  │ ☑ Flags                │         │
│ │  ☑ TREE            │  │ ☑ Spawns               │         │
│ │  ☑ SHADE           │  │ ☑ Scripts              │         │
│ │  ☐ SPARSE          │  │ ☐ Grid                 │         │
│ │                    │  │ ☐ Coordinates          │         │
│ │ Elevation: 5       │  └────────────────────────┘         │
│ │ Scripts: 2         │                                     │
│ │  - Entry (1001)    │  [Save] [Revert] [Export JSON]     │
│ │  - Random (1002)   │                                     │
│ └────────────────────┘                                     │
└─────────────────────────────────────────────────────────────┘
```

**Tile Painting Features**:
- Click to place single tile
- Click-and-drag to paint regions
- Fill tool for large areas
- Brush size selector (1x1, 3x3, 5x5, 10x10, 20x20)
- Undo/redo stack (client-side for responsiveness, synced to server)

**Visual Script Editor**:
- Drag-and-drop script assignment to tiles
- Visual trigger indicators on map (colored overlays)
- Script debugging with tile highlighting
- Error markers on problematic tiles

**Collaborative Editing**:
- Real-time cursors showing other editors
- Region-level locking during edits (prevent conflicts)
- Change history with author attribution
- Chat/comments on map regions

### 10.3 Backend API Requirements

**RESTful API** (for web frontend):
```javascript
// Get wilderness map metadata
GET /api/wilderness/{uid}
Response: {
  uid: 1234,
  name: "Mystwood Forest",
  size_x: 400,
  size_y: 400,
  default_terrain: "forest",
  tile_count: 160000,
  modified: "2026-01-28T14:30:00Z"
}

// Get tile data for region (chunked for efficiency)
GET /api/wilderness/{uid}/region?x=0&y=0&width=100&height=100
Response: {
  tiles: [
    { x: 0, y: 0, terrain: "forest", flags: ["TREE", "SHADE"], elev: 5 },
    { x: 0, y: 1, terrain: "forest", flags: ["TREE"], elev: 5 },
    // Only returns tiles that differ from default
  ],
  default_terrain: "forest",
  default_flags: ["TREE"]
}

// Update single tile
PATCH /api/wilderness/{uid}/tile/{x}/{y}
Body: {
  terrain: "plains",
  flags: ["GRASS"],
  elevation: 3
}
Response: { success: true, checksum: "abc123..." }

// Batch update (for drag operations, fill tool)
PATCH /api/wilderness/{uid}/tiles
Body: {
  tiles: [
    { x: 10, y: 10, terrain: "plains", flags: ["GRASS"] },
    { x: 10, y: 11, terrain: "plains", flags: ["GRASS"] },
    { x: 11, y: 10, terrain: "plains", flags: ["GRASS"] }
  ]
}
Response: { success: true, updated: 3, checksum: "abc123..." }

// Fill operation (server-side flood fill)
POST /api/wilderness/{uid}/fill
Body: {
  start_x: 45,
  start_y: 32,
  terrain: "plains",
  flags: ["GRASS"],
  match_terrain: "forest",  // Optional: only fill matching terrain
  max_tiles: 1000           // Safety limit
}
Response: { success: true, filled: 427, checksum: "abc123..." }

// Add spawn point
POST /api/wilderness/{uid}/spawns
Body: {
  x: 45,
  y: 32,
  mob_vnum: 3001,
  frequency: 5,
  radius: 3
}

// Get changes since timestamp (for live updates)
GET /api/wilderness/{uid}/changes?since=1706468400
Response: {
  changes: [
    { timestamp: 1706468405, user: "Bob", x: 10, y: 10, terrain: "plains" },
    { timestamp: 1706468410, user: "Alice", x: 15, y: 20, spawn_added: {...} }
  ],
  current_checksum: "abc123..."
}

// Export entire wilderness
GET /api/wilderness/{uid}/export?format=json
Response: { /* Full map data in JSON */ }
Downloads: wilderness_1234_export.json

// Import wilderness
POST /api/wilderness/import
Body: FormData with wilderness_export.json file
Response: { uid: 5678, name: "Imported Map", success: true }
```

**WebSocket API** (for real-time collaboration):
```javascript
// Connect to editing session
ws://server/wilderness/{uid}/edit
Headers: { Authorization: "Bearer <token>" }

// Client → Server messages
{
  "type": "tile_update",
  "x": 10,
  "y": 10,
  "terrain": "plains",
  "flags": ["GRASS"]
}

{
  "type": "cursor_move",
  "x": 25,
  "y": 30
}

{
  "type": "lock_region",
  "x1": 10,
  "y1": 10,
  "x2": 20,
  "y2": 20,
  "lock": true
}

// Server → Client messages
{
  "type": "tile_updated",
  "user": "Bob",
  "x": 10,
  "y": 10,
  "terrain": "plains",
  "flags": ["GRASS"],
  "timestamp": 1706468405
}

{
  "type": "user_cursor",
  "user": "Alice",
  "x": 25,
  "y": 30
}

{
  "type": "region_locked",
  "user": "Bob",
  "x1": 10,
  "y1": 10,
  "x2": 20,
  "y2": 20
}

{
  "type": "error",
  "message": "Cannot edit locked region"
}
```

### 10.4 Data Format Optimization

**Current .wmap Format** (binary, efficient but opaque):
```
[Binary tile data: terrain_id + flags + elevation]
400x400 map = 160,000 bytes minimum
```

**Web-Optimized JSON Format** (sparse, readable):
```json
{
  "uid": 1234,
  "name": "Mystwood Forest",
  "size": [400, 400],
  "default": {
    "terrain": "forest",
    "flags": ["TREE"],
    "elevation": 5
  },
  "tiles": {
    "0,0": { "flags": ["TREE", "SHADE"] },
    "10,10": { "terrain": "plains", "flags": ["GRASS"], "elev": 3 },
    "50,50": { "terrain": "mountain", "elev": 20 }
    // Only non-default tiles stored (sparse)
  },
  "regions": [
    {
      "name": "Dark Grove",
      "bounds": [10, 10, 20, 20],
      "terrain": "forest",
      "flags": ["TREE", "DARK", "SHADE"]
    },
    {
      "name": "Northern Plains",
      "bounds": [100, 0, 200, 50],
      "terrain": "plains",
      "flags": ["GRASS"]
    }
  ],
  "spawns": [
    { "x": 45, "y": 32, "vnum": 3001, "freq": 5, "radius": 3 }
  ],
  "scripts": [
    { "x": 10, "y": 10, "vnum": 1001, "trigger": "entry" }
  ]
}
```

**Hybrid Approach** (best of both):
- **Backend storage**: Binary .wmap for game server (fast loading, compact)
- **Web transmission**: JSON for API responses (human-readable, debuggable)
- **Caching layer**: Redis stores hot regions in JSON format

**Benefits of Sparse Format**:
- ✅ **Reduced size**: 400x400 map with 90% forest → ~16,000 entries instead of 160,000
- ✅ **Faster loading**: Web client loads only non-default data
- ✅ **Efficient updates**: Only changed tiles transmitted
- ✅ **Version control friendly**: Smaller diffs in Git
- ✅ **Readable**: Humans can edit JSON directly if needed

### 10.5 Revised Priorities with Web Editor

**Critical** (enables web editor):
1. **Backend API layer** - RESTful endpoints for wilderness CRUD operations
2. **JSON serialization** - Convert .wmap ↔ JSON for web transmission
3. **Validation layer** - Server-side rule enforcement (prevent invalid edits)
4. **Authentication** - Secure builder access to editing endpoints
5. **Chunked loading** - Send map data in 100x100 regions (not entire 400x400)

**High Priority** (improves web UX):
6. **WebSocket support** - Real-time collaborative editing
7. **Caching layer** - Fast map serving (Redis for hot regions)
8. **Change history** - Audit trail for web edits (who changed what when)
9. **Undo/redo** - Server-side for multi-user consistency
10. **Fill tool** - Server-side flood fill algorithm

**Medium Priority** (nice-to-have):
11. **Region templates** - Predefined terrain patterns (forest grove, mountain range)
12. **Import/export** - Upload/download entire maps
13. **Visual script editor** - Drag-drop script assignment
14. **Conflict resolution** - Smart merging of simultaneous edits

**Low Priority** (in-game editor features):
15. **ASCII visualization** - Web has better visuals
16. **Text-based commands** - Web has point-and-click
17. **In-game help system** - Web has built-in UI tooltips

### 10.6 Implementation Strategy

**Phase 1: Backend Foundation** (2-3 weeks)
```
Week 1:
- Design RESTful API endpoints
- Implement JSON ↔ .wmap conversion
- Add validation layer
- Create wilderness service layer

Week 2-3:
- Implement chunked region loading
- Add batch update endpoints
- Create fill tool algorithm
- Add authentication/authorization
```

**Phase 2: Web Frontend** (4-6 weeks)
```
Week 1-2:
- Visual map renderer (Canvas API or WebGL)
- Tile editor with painting tools
- Property inspector panel
- Terrain/flag selection UI

Week 3-4:
- Brush size selector
- Fill tool integration
- Undo/redo stack
- Import/export functionality

Week 5-6:
- Script assignment UI
- Spawn point editor
- Region templates
- Polish and bug fixing
```

**Phase 3: Real-Time Features** (2-3 weeks)
```
Week 1:
- WebSocket integration
- Live cursor tracking
- Region locking

Week 2-3:
- Multi-user editing
- Conflict detection/resolution
- Change notifications
- User presence indicators
```

**Phase 4: Advanced Features** (2-3 weeks)
```
Week 1:
- Advanced selection tools (lasso, magic wand)
- Layer system
- Copy/paste regions

Week 2-3:
- History/versioning UI
- Diff visualization
- Rollback functionality
```

**Total Effort**: 10-14 weeks for full web-based wilderness editor

### 10.7 Multi-Interface Strategy

**Three Equal Interfaces**:

1. **Web Editor** - For visual designers
   - Graphical map display with drag-and-drop
   - Real-time collaboration with multiple builders
   - Large-scale terrain painting and region tools
   - Import/export capabilities
   - Best for: Initial map design, bulk editing, team projects

2. **In-Game Text Editor** - For MUD purists
   - Text commands within the game world
   - Immediate testing (you're physically there)
   - Works with any MUD client
   - No additional software needed
   - Best for: Quick fixes, on-the-fly adjustments, traditionalists

3. **GMCP-Enhanced MUD Clients** - Best of both worlds
   - Visual map display within traditional MUD client
   - Graphical overlays on text interface
   - Supports graphics without leaving terminal
   - Examples: Mudlet, MUSHclient with GMCP plugins
   - Best for: Players who want visuals but prefer MUD clients

**GMCP Wilderness Protocol**:
```json
// Server → Client: Send map data via GMCP
GMCP.Wilderness.Map {
  "center": [45, 32],
  "radius": 10,
  "tiles": [
    {"x": 45, "y": 32, "terrain": "forest", "flags": ["TREE"], "elev": 5},
    {"x": 46, "y": 32, "terrain": "plains", "flags": ["GRASS"], "elev": 3}
  ],
  "player_position": [45, 32],
  "visible_mobs": [
    {"name": "a wolf", "x": 47, "y": 33}
  ]
}

// Client displays this as a graphical minimap or full map
// while keeping text interface for commands/interactions
```

**Architecture**: All three interfaces use the **same backend API**

```
Web Browser ────┐
                │
MUD Client ─────┼─→ Backend API ─→ Data Layer ─→ Game Server
                │
GMCP Client ────┘
```

**Why Keep All Three**:
- ✅ **Accessibility** - Different users, different preferences
- ✅ **Reliability** - If web is down, in-game still works
- ✅ **Context** - Edit from within the world (in-game) or outside (web)
- ✅ **Performance** - Text interface uses less bandwidth
- ✅ **Tradition** - MUD community values text interfaces
- ✅ **Innovation** - GMCP brings graphics to traditional clients

All interfaces get **equal development priority** - improvements to one benefit all.

### 10.8 Key Architectural Decisions

**Backend-First Approach**:
```
┌─────────────────┐
│  Web Frontend   │ ← Visual editor, real-time collaboration
│  (React/Vue)    │
└────────┬────────┘
         │ REST API / WebSocket
┌────────▼────────┐
│  Backend API    │ ← Validation, permissions, data access
│  Service Layer  │   (Express/FastAPI/embedded)
└────────┬────────┘
         │
┌────────▼────────┐
│  Data Layer     │ ← Binary .wmap + JSON cache
│  (Redis cache)  │   Sparse format, optimized storage
└────────┬────────┘
         │
┌────────▼────────┐
│  Game Server    │ ← Uses same data structures
│  (MUD process)  │   Loads .wmap files directly
└─────────────────┘
```

**Benefits**:
- ✅ Web editor and in-game editor share backend
- ✅ Single source of truth for data
- ✅ Validation enforced consistently
- ✅ Easy to add more clients (mobile app, desktop tool)
- ✅ Game server remains independent (can run without web)

**Design Principles**:
1. **API-first**: Design backend API before UI
2. **Sparse data**: Only store/transmit non-default tiles
3. **Chunked loading**: Send 100x100 regions at a time
4. **Real-time sync**: WebSocket for live collaboration
5. **Offline capable**: Web editor can cache and queue changes
6. **Version control**: Track who changed what and when

### 10.9 Performance Considerations

**Web-Specific Optimizations**:
```javascript
// Client-side tile caching
const tileCache = new Map();  // LRU cache of loaded regions

// Load visible region + buffer
function loadVisibleRegion(centerX, centerY, viewportWidth, viewportHeight) {
  const buffer = 50;  // Load 50 tiles beyond viewport
  const x1 = Math.max(0, centerX - viewportWidth/2 - buffer);
  const y1 = Math.max(0, centerY - viewportHeight/2 - buffer);
  const x2 = Math.min(mapWidth, centerX + viewportWidth/2 + buffer);
  const y2 = Math.min(mapHeight, centerY + viewportHeight/2 + buffer);
  
  return fetchRegion(x1, y1, x2 - x1, y2 - y1);
}

// Batch tile updates
const pendingUpdates = [];
function updateTile(x, y, terrain, flags) {
  pendingUpdates.push({ x, y, terrain, flags });
  
  // Debounce: send batch after 500ms of inactivity
  clearTimeout(batchTimer);
  batchTimer = setTimeout(() => {
    api.patch('/wilderness/1234/tiles', { tiles: pendingUpdates });
    pendingUpdates.length = 0;
  }, 500);
}

// Canvas rendering optimization
function renderMap(context, tiles, viewport) {
  // Only render visible tiles + small buffer
  const visibleTiles = tiles.filter(t => 
    t.x >= viewport.x1 && t.x <= viewport.x2 &&
    t.y >= viewport.y1 && t.y <= viewport.y2
  );
  
  // Use requestAnimationFrame for smooth rendering
  requestAnimationFrame(() => {
    visibleTiles.forEach(tile => {
      const sprite = terrainSprites[tile.terrain];
      context.drawImage(sprite, tile.x * tileSize, tile.y * tileSize);
    });
  });
}
```

**Server-Side Optimizations**:
```c
// Region-based caching
struct wilderness_region {
    int x1, y1, x2, y2;
    char *json_data;      // Cached JSON for this region
    time_t cached_time;
    char checksum[33];
};

// Get cached region or generate
char *get_region_json(WILDS_DATA *wilds, int x, int y, int width, int height) {
    // Check cache
    wilderness_region *region = find_cached_region(wilds, x, y, width, height);
    if (region && current_time - region->cached_time < 300) {
        return region->json_data;  // Use cache if < 5 minutes old
    }
    
    // Generate JSON from binary map
    json_t *root = json_object();
    json_t *tiles = json_array();
    
    for (int cy = y; cy < y + height; cy++) {
        for (int cx = x; cx < x + width; cx++) {
            WILDS_TERRAIN *tile = get_terrain(wilds, cx, cy);
            
            // Only include non-default tiles (sparse)
            if (tile->terrain != wilds->default_terrain ||
                tile->flags != wilds->default_flags ||
                tile->elevation != wilds->default_elevation) {
                
                json_t *tile_obj = json_pack("{s:i, s:i, s:s}",
                    "x", cx,
                    "y", cy,
                    "terrain", terrain_name(tile->terrain));
                json_array_append_new(tiles, tile_obj);
            }
        }
    }
    
    json_object_set_new(root, "tiles", tiles);
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    // Cache for future requests
    cache_region(wilds, x, y, width, height, json_str);
    
    return json_str;
}
```

---

## Conclusion

The wilderness system analysis reveals sophisticated architecture with room for optimization. **With web-based editing planned**, priorities shift significantly:

### Without Web Editor (Original Analysis):
1. **Performance**: Tile lookup optimization, caching, string operations
2. **Memory**: Data structure compression, efficient storage
3. **In-game editing**: Text commands, ASCII visualization, help system
4. **Visualization**: Better map display within game

**Estimated effort**: 6-10 weeks

### With Multi-Interface Support (Revised Priorities):
1. **Backend API layer**: RESTful endpoints for CRUD operations **(Critical)**
2. **JSON serialization**: Efficient format conversion and transmission **(Critical)**
3. **Chunked loading**: Region-based data fetching (100x100 chunks) **(Critical)**
4. **Validation layer**: Server-side rule enforcement **(Critical)**
5. **In-game text editor**: Text commands for MUD purists **(Critical)**
6. **GMCP protocol**: Wilderness map data for GMCP-capable clients **(High)**
7. **WebSocket support**: Real-time collaborative editing (web) **(High)**
8. **Performance**: Backend optimizations serve all interfaces **(High)**

**Estimated effort**: 10-14 weeks (includes web frontend + GMCP support)

### Key Changes:

**Storage Format**:
- **Before**: Binary .wmap for in-game efficiency
- **After**: Binary .wmap + sparse JSON cache for web transmission
- **Benefit**: 90% size reduction for typical maps over network

**Editing Experience**:
- **Before**: Text commands only, ASCII art, sequential tile editing
- **After**: Three interfaces - web (visual), in-game (text), GMCP (hybrid)
- **Benefit**: Choose your style - web for speed, text for tradition, GMCP for both

**Data Architecture**:
```
Web Editor ────┐
               │
In-Game Text ──┼─→ Backend API ─→ Data Layer ─→ Game Server
               │
GMCP Clients ──┘
```

### Bottom Line:

**Three interfaces, one backend** - support all editing styles:

1. **Web Editor**: Visual design, collaboration, bulk operations
   - ✅ 10x faster for large-scale map creation
   - ✅ Real-time multi-user editing
   - ✅ Drag-and-drop, fill tools, undo/redo

2. **In-Game Text Editor**: Traditional MUD experience
   - ✅ Edit from within the game world
   - ✅ Immediate testing (you're physically there)
   - ✅ No additional software needed
   - ✅ Works with any MUD client

3. **GMCP-Enhanced Clients**: Graphics in traditional clients
   - ✅ Visual minimap/full map display
   - ✅ Keep familiar MUD client (Mudlet, MUSHclient)
   - ✅ Best of both worlds - text + graphics

**Critical Success Factors**:
- ✅ **API-first design**: All interfaces use same backend
- ✅ **Equal priority**: No interface is "backup" or "legacy"
- ✅ **Sparse JSON**: 90% data reduction for network transmission
- ✅ **GMCP protocol**: Bring graphics to MUD clients
- ✅ **Backend optimizations**: Benefit all interfaces equally

The backend data structures and performance optimizations remain critical, serving all three presentation layers. The recommended approach is **backend-first development**: build a robust API with GMCP support, then develop web and in-game editors in parallel.

**Total estimated effort**: 10-14 weeks for complete multi-interface wilderness editing system.

---

## 2026 JSON-Era Re-Review Addendum

This section revisits the plan after JSON zone loading was introduced and with new requirements:

- Dynamic weather systems
- Script-driven wilderness events
- Logical wilderness regions
- NPC ships of multiple behaviors/types
- Runtime obstacles/occlusion and light-level effects
- Accessibility-friendly wayfinding (not ASCII-only)
- Optional PNG ingest without blocking the main game loop

### What Changed Since Original Analysis

1. **Area loading now prefers JSON entries from `area.lst`** and falls back to legacy `.are` where needed.
2. **Wilderness core still uses virtual rooms** with `staticmap` + `map` dual arrays.
3. **Terrain lookup currently reads from `staticmap`** in `get_terrain_by_coors()`, so runtime tile edits in `map` are not yet authoritative for all systems.

### Updated Architecture Direction (Recommended)

Adopt a **layered wilderness state model** where base terrain and dynamic systems are independent:

1. **Base Layer (immutable at runtime)**
  - Source: wildgen output / imported map
  - Backed by `staticmap`

2. **Runtime Overlay Layer (mutable, optionally persisted)**
  - Source: weather effects, scripts, admin commands, live events
  - Backed by `map` + sparse overlay records
  - Supports temporary or permanent changes

3. **Systems Layer (dynamic entities, not terrain)**
  - Storm cells, region metadata, NPC ship fleets, event markers, hazards
  - Stored as state objects keyed by wilderness UID + coordinate chunks

### Required Engine Corrections

Before major feature work, fix two root issues:

1. **Unified terrain resolver**
  - Replace direct `staticmap` reads in gameplay paths with:
    - `get_wilds_base_tile()`
    - `get_wilds_effective_tile()` (overlay-aware)
  - Ensure map display, movement cost, weather effects, and pathing all use the same effective tile source.

2. **Chunked state container**
  - Store runtime overrides by chunk (for example 32x32 or 64x64) rather than scanning full maps.
  - Required for web editor, dynamic events, and accessibility APIs to scale.

### Dynamic Weather + Scripting + Regions + NPC Ships

Treat all four as first-class, coordinate-native systems:

- **Dynamic weather:** Storm cells move over coordinates; apply transient terrain/visibility/light modifiers via overlay.
- **Scripting hooks:** Trigger events on enter/leave tile, region transitions, weather crossings, and ship encounters.
- **Regions:** Add named region records (`uid`, name, polygon/rect bounds, tags, climate profile, script set).
- **NPC ships:** Maintain abstract ship positions/routes independent of room lifetime; only materialize room details when players are nearby.

### Obstacle Handling (Occlusion) + Light Levels

Add two data channels to terrain/overlay metadata:

1. **Occlusion cost / block value**
  - Example: dense forest, cliffs, walls, heavy fog tiles
  - Used by map visibility pass (line-of-sight/field-of-view) so tiles behind obstacles can be hidden or dimmed.

2. **Luminance modifier**
  - Per-tile and per-effect light contribution
  - Combined with global sunlight/weather to compute effective local visibility.

This complements existing `get_squares_to_show_*()` behavior by moving from radius-only visibility to **radius + obstruction + light**.

### Accessibility / Wayfinding Requirements

ASCII map remains useful, but accessibility requires **structured navigation outputs**:

1. **Path service API**
  - Input: origin, destination, movement profile (on foot, mounted, ship), risk preference
  - Output: step list + semantic cues (region crossings, hazards, water entry, low-light segments)

2. **Text-first route narration**
  - Example format:
    - "Go north 12 tiles through Blackmoore Forest"
    - "Turn northeast at river edge"
    - "Enter region: Broken Coast"
  - This supports screen readers and non-visual clients.

3. **Landmark graph**
  - Maintain discoverable waypoints (ports, roads, passes, camps, gates) and use them in route instructions.

### PNG Support Without Blocking Main Thread

Direct PNG support is viable if constrained to asynchronous workflows:

1. **Never decode PNG on main game tick.**
2. Run decode/import in a worker process or job queue.
3. Produce an intermediate map artifact (for example `.wmap`) and stage it.
4. Swap wilderness base map only at safe points (admin-triggered reload window).
5. On swap, re-apply persistent overlays and invalidate affected runtime caches/chunks.

If full in-process async workers are postponed, keep external wildgen as the first implementation of this same staged pipeline.

### Recommended Priority Order (Revised)

1. Build unified effective-tile resolver and chunked overlay store.
2. Add region model + script hook points.
3. Add coordinate-native NPC ship manager and weather cells on shared scheduler.
4. Add FOV/occlusion + luminance calculations for map rendering.
5. Add accessibility path service and narrated route outputs.
6. Add async PNG import pipeline (or external staged import equivalent).

### Bottom Line (Updated)

Keep wilderness **coordinate-first and state-layered**. With that foundation, dynamic weather, scripted events, regions, ship AI, obstacle-aware visibility, lighting, web editing, and accessibility wayfinding can all coexist without forcing persistent static rooms or blocking the main game thread.

---

## References

**Key Source Files:**
- `/sentience/src/wilds.c` (3216 lines) - Virtual room management
- `/sentience/src/wilds.h` (196 lines) - Wilderness structures
- `/sentience/src/weather.c` (1221 lines) - Weather system
- `/sentience/src/boat.c` (7372 lines) - Ship system
- `/sentience/tools/wildgen/wildgen.c` (1161 lines) - Map generator

**Data Files:**
- `/sentience/tools/maps/wilds/*.png` - Source map images
- `/sentience/tools/maps/wilds/*.ini` - Terrain configuration
- `/sentience/area/nibswilds.are` (35MB) - Generated wilderness

**Related Documentation:**
- See `/sentience/docs/TECH_DEBT.md` for other system issues
