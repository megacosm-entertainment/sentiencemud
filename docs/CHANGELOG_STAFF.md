# Staff Changelog

Changes since the account system update (ad9c53d).

## New Features

### Social Editor
- New `socialedit` command for creating and modifying socials in-game
- Full OLC-style interface for managing social actions

### Reserved VNUM System
- New system to reference important VNUMs by symbolic name instead of hardcoded numbers
- Use `load obj $OBJ_VNUM_SHARD` instead of remembering vnum 1234
- Reserved VNUM editor (`reserved`) for managing these mappings
- Supports rooms, objects, and mobiles

### Chat Room Administration
- Room creators can now view chat room passwords using the `show` command
- Improved visibility controls for chat room management

### Scripting Enhancements
- Access game settings in scripts: `$(game.setting.<setting_name>)`
- Access reserved VNUMs in scripts: `$(game.reserved_room.ROOM_VNUM_TEMPLE)`
- More flexible scripting without hardcoded values

### Improved Logging
- New categorized logging system for better troubleshooting
- Categories include: combat, scripts, OLC, security, SQL, HTTP
- Stack traces available in debug builds for crash investigation

## Editor Reorganization

The OLC system has been reorganized into a modular structure:

- **Areas**: `aedit` - Area editing
- **Blueprints**: `bpedit`, `bsedit` - Blueprint editing
- **Commands**: `cmdedit` - Command editing
- **Dungeons**: `dngedit` - Dungeon editing
- **Game Settings**: `gameedit` - Game configuration
- **Help**: `hedit` - Help file editing
- **Mobiles**: `medit` - Mobile editing
- **Objects**: `oedit` - Object editing
- **Projects**: `pedit` - Project management
- **Random Strings**: `rsgedit` - Random string groups
- **Rooms**: `redit` - Room editing
- **Scripts**: Script editing (mpedit, etc.)
- **Ships**: `shedit` - Ship editing
- **Socials**: `socialedit` - Social editing
- **Tokens**: `tedit` - Token editing
- **Wilderness**: `wedit` - Wilderness editing (vlink editor folded into wedit as a tab)

A common editor framework now provides consistent behavior across all editors.

## Bug Fixes

- Fixed issues with mob pronoun/body type changes not persisting
- Fixed duty check typo
- Improved account and character save/load reliability
- Various stability improvements

## Removed Features

### Locker System
- The old locker system has been removed (replaced by account shared storage)

### IMC2
- Inter-MUD Communication system removed
