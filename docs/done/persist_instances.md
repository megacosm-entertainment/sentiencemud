# Instance Persistence System

## Overview

The instance persistence system has been redesigned to store ships, dungeons, and standalone instances as individual self-contained JSON files in the `persist/` directory.

## Directory Structure

```
persist/
├── ships/          # Individual ship files
│   └── ship_<uid0>_<uid1>.json
├── dungeons/       # Individual dungeon files
│   └── dungeon_<uid0>_<uid1>.json
└── instances/      # Standalone instance files
    ├── instance_obj_<uid0>_<uid1>.json      # Object-owned instances
    ├── instance_plr_<uid0>_<uid1>.json      # Player-owned instances
    └── instance_bp_<vnum>.json              # Blueprint-based instances
```

## Self-Contained Storage

Each file contains complete embedded data:

- **Ships**: Include their instance with all rooms, NPCs, and objects
- **Dungeons**: Include all floors as instances with complete rooms
- **Instances**: Include all sections with complete rooms

This means NPCs, objects, and room state are fully preserved within the parent entity file, not as separate references.

## Conditional Loading

Each entity file includes a `load_on_boot` metadata field:

```json
{
  "load_on_boot": true,
  "id": [123, 456],
  ...
}
```

Setting this to `false` allows entities to exist in persist storage without being loaded at boot time. This is useful for:

- Archived/inactive instances
- On-demand loading for performance
- Temporary disable without deletion

## API Functions

### Individual Save
- `bool json_persist_save_ship(SHIP_DATA *ship)`
- `bool json_persist_save_dungeon(DUNGEON *dungeon)`
- `bool json_persist_save_instance(INSTANCE *instance)`

### Individual Load
- `SHIP_DATA *json_persist_load_ship(const char *filename)`
- `DUNGEON *json_persist_load_dungeon(const char *filename)`
- `INSTANCE *json_persist_load_instance(const char *filename)`

### Directory Scanning
- `int json_persist_load_all_ships(void)`
- `int json_persist_load_all_dungeons(void)`
- `int json_persist_load_all_instances(void)`

### Main Functions
- `bool json_save_instances(void)` - Iterates and saves all entities
- `bool json_load_instances(void)` - Scans directories and loads all entities

## Migration from Old Format

The old `data/world/instances.json` format (single monolithic file) is still supported as a fallback. On first boot with the new system:

1. Directories are checked first (`persist/ships/`, etc.)
2. If no persist files found, falls back to `data/world/instances.json`
3. If that fails, falls back to legacy `data/world/instances.dat`

To manually migrate from old format:
1. Boot with old instances.json
2. Save (triggers automatic save to persist directories)
3. Old file can be archived/removed

## Room Persistence

The `persist/rooms/`, `persist/mobiles/`, and `persist/objects/` directories are now reserved for **loose** persistent entities only - those not owned by any instance, ship, or dungeon.

Instance-owned rooms are embedded directly in their parent entity file.

## Implementation Details

- Uses `opendir()/readdir()` for directory scanning
- Creates directories automatically with `mkdir(path, 0755)`
- Filename format encodes entity type and UID for uniqueness
- Full room serialization via `json_persist_room_to_json()`
- Handles WNUM format for area-scoped references

## Benefits

1. **Scalability**: Individual files scale better than monolithic file
2. **Caching**: Can cache individual entities without loading all
3. **Performance**: Only load entities marked with `load_on_boot: true`
4. **Self-Contained**: No broken references - all data embedded
5. **Debugging**: Easier to inspect/edit individual entity files
6. **Modularity**: Aligns with character persistence model
