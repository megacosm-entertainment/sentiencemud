# Persistence Layer - Technical Documentation

This document provides technical details about the MUD's persistence system for runtime world state (rooms, mobiles, and objects).

## Current System Overview

### File Locations
- **persist.dat**: `data/world/persist.dat` (defined as `PERSIST_FILE` in merc.h:8025)
- **instances.dat**: `data/world/instances.dat` (separate system for dungeons/ships)

### Core Data Structures

#### Persistence Lists (db.c ~line 600)
```c
LLIST *persist_mobs;    // Linked list of persistent mobiles
LLIST *persist_objs;    // Linked list of persistent objects
LLIST *persist_rooms;   // Linked list of persistent rooms
```

#### Persistence Flags
Each entity type has a `persist` boolean flag:
- `CHAR_DATA->persist` (mobiles)
- `OBJ_DATA->persist` (objects)
- `ROOM_INDEX_DATA->persist` (rooms)

### persist.dat Format

The file is a text-based key-value format with section markers:

```
#OBJECT <vnum>
Version <int>
UID <long>
UID2 <long>
Persist
Name <string>~
...
#-OBJECT

#MOBILE <vnum>
Version <int>
Name <string>~
UID <long>
UID2 <long>
Persist
...
#-MOBILE

#ROOM <vnum>
#VROOM <wilds_uid> <x> <y> <z>      (wilderness rooms)
#CROOM <source_vnum> <id0> <id1>    (cloned rooms)
...
#-ROOM

#END
```

### Key Functions (db.c)

#### Save Functions
| Function | Line | Purpose |
|----------|------|---------|
| `persist_save()` | 6229 | Main save entry point |
| `persist_save_object()` | 5585 | Serialize single object |
| `persist_save_mobile()` | 5841 | Serialize single mobile |
| `persist_save_room()` | 6119 | Serialize single room |
| `persist_save_token()` | ~5520 | Serialize tokens |
| `persist_save_scriptdata()` | - | Serialize script variables |

#### Load Functions
| Function | Line | Purpose |
|----------|------|---------|
| `persist_load()` | 8075 | Main load entry point |
| `persist_load_object()` | 6434 | Deserialize single object |
| `persist_load_mobile()` | 6996 | Deserialize single mobile |
| `persist_load_room()` | 7626 | Deserialize single room |
| `persist_load_token()` | ~6380 | Deserialize tokens |

#### List Management
| Function | Purpose |
|----------|---------|
| `persist_addmobile()` | Add NPC to persist_mobs list |
| `persist_addobject()` | Add object to persist_objs list |
| `persist_addroom()` | Add room to persist_rooms list |
| `persist_removemobile()` | Remove from persist_mobs |
| `persist_removeobject()` | Remove from persist_objs |
| `persist_removeroom()` | Remove from persist_rooms |

### Environment Checking

The `check_persist_environment()` function (db.c:6193) prevents double-saving nested entities. A persistent entity is only written to persist.dat if it's NOT inside another persistent container:
- Object in persistent object -> skip (saved with container)
- Object in persistent room -> skip (saved with room)
- Mobile in persistent room -> skip (saved with room)

### Unique Identifiers

- **Objects/Mobiles**: `id[0]` and `id[1]` form a 64-bit UID
- **Rooms**: Either vnum (static), wilds coords (wilderness), or clone ID (instanced)

Room types:
- `#ROOM <vnum>` - Static room by vnum
- `#VROOM <wilds_uid> <x> <y> <z>` - Wilderness room
- `#CROOM <source_vnum> <id0> <id1>` - Cloned/instanced room

---

## Object Persistence Data

Full list of persisted object fields (from persist_save_object):

```
Version         - Object version number
UID/UID2        - Unique identifier
Persist         - Persistence flag marker
Name            - Object name
ShortDesc       - Short description
LongDesc        - Long description
FullDesc        - Full description
Extra[1-4]      - Extra flags (4 arrays)
WearFlags       - Wear locations bitmask
ItemType        - Item type enum
PermExtra[1-4]  - Permanent extra flags
PermWeapon      - Permanent weapon flags (weapons only)
Room/Vroom/CloneRoom - Location data
Enchanted       - Number of enchantments
Weight          - Object weight
Cond            - Condition percentage
Fixed           - Times repaired
Owner           - Owner name
OldName/Short/Descr/FullDescr - Original values
LoadedBy        - Who loaded the object
Fragility       - Fragility rating
TimesAllowedFixed - Max repair count
Locker          - Is locker item flag
WearLoc         - Current wear location
LastWearLoc     - Previous wear location
Level           - Object level
Timer           - Decay timer
Cost            - Object value
Value[0-7]      - Item-type specific values
Lock            - Lock data if applicable
MapWaypoint     - Map waypoints
SpellNew        - Spell data
AffObjSk/AffObjNm/AffMob - Affects
Cata/CataA/CataN/CataNA - Catalysts
ExDe/ExDeEnv    - Extra descriptions
OwnerName/OwnerShort - Corpse owner info
(script variables via persist_save_scriptdata)
(tokens via persist_save_token)
(contained objects - recursive)
```

---

## Mobile Persistence Data

Full list of persisted mobile fields (from persist_save_mobile):

```
Version         - Mobile version number
Name            - Mobile name
UID/UID2        - Unique identifier
Persist         - Persistence flag
Dead            - Death state
DeathTimeLeft   - Respawn timer
RepopRoom*      - Respawn location
Owner           - Owner name
ShD/LnD/Desc    - Descriptions
Race            - Race name
Sex             - Gender
Levl/TLevl      - Level data
(continues with stats, position, inventory, etc.)
```

---

## Room Persistence Data

Full list of persisted room fields (from persist_save_room):

```
#ROOM/#VROOM/#CROOM - Room type and identifier
XYZ             - Coordinates
ViewWilds       - Wilderness view reference
Name            - Room name
Desc            - Room description
Owner           - Room owner
Persist         - Persistence flag
Locale          - Locale identifier
room_flags/2    - Room flags
Sector          - Sector type
HealRate/ManaRate/MoveRate - Regeneration rates
RoomRecall      - Recall location
Exits           - All exit data
(script variables via persist_save_scriptdata)
(tokens via persist_save_token)
(contained objects)
(contained NPCs)
```

---

## Existing Infrastructure

### JSON Library
The project uses **jansson** for JSON handling (see json_char.c).

### Redis Integration
Redis caching exists for character data (redis_cache.c/h). Key TTLs:
- `REDIS_TTL_CHAR_INFO` = 24 hours
- `REDIS_TTL_WORLD_STATE` = 7 days (reserved for persistence)

### Existing JSON Patterns
The character system (json_char.c) provides patterns for:
- `obj_to_json()` - Object serialization (shared code)
- `json_to_obj()` - Object deserialization
- Flag table lookups via `flag_string()` and `flag_value()`

---

## Migration Notes

### Phase 1 Target Structure
```
data/persist/
├── rooms/
│   └── <unique_id>.json
├── mobiles/
│   └── <uid0>_<uid1>.json
└── objects/
    └── <uid0>_<uid1>.json
```

### Unique ID Schemes
- **Rooms**: `vnum` or `wilds_<wuid>_<x>_<y>_<z>` or `clone_<src>_<id0>_<id1>`
- **Mobiles/Objects**: `<id[0]>_<id[1]>`

### Compatibility
During transition, the system should:
1. Check for JSON files first
2. Fall back to persist.dat for missing entities
3. Write only to JSON format (not updating persist.dat)
