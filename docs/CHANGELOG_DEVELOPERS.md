# Developer Changelog

Changes since the account system update (ad9c53d).

## Recent Event Runtime Updates

- `editors/events/evtedit.c` now includes stronger editor validation for bracket specifications (`spawnbrackets`, `collectionbrackets`), `bracketmode`, and `progressagg`.
- Event runtime scheduling now supports recurring **and calendar cadence** behavior from a scheduled anchor + interval.
- Boss event progression/completion is now handled in runtime kill tracking.
- Passive/worldstate event participation is explicitly blocked from `event join` paths.
- Script integration now includes event provenance + bracket metadata propagation and runtime control/read surfaces for progress/phase/finish flows.
- See `docs/EVENT_EDITOR_RUNTIME.md` for current behavior and deferred items.

## Major Architectural Changes

### JSON-Based Data Storage
- **Player files** (`json_char.c`, `json_char.h`): Characters now save/load as JSON
- **Account files** (`json_account.c`, `json_account.h`): Accounts stored as JSON
- **Game settings** (`json_game_settings.c`, `json_game_settings.h`): Configuration in JSON
- **Race definitions** (`json_race.c`): Races moved from hardcoded `race_table[]` in const.c to JSON files in `/sentience/data/races/`
- **Persistence layer** (`json_persist.c`, `json_persist.h`): Common JSON serialization utilities

### Connection Abstraction Layer
New modular connection system supporting multiple protocols:
- `connection.c`, `connection.h` - Abstract connection interface
- `connection_tcp.c` - Plain TCP connections
- `connection_tls.c` - TLS/SSL encrypted connections
- `connection_websocket.c` - WebSocket protocol support (initial implementation)
- `protocol_layer.c`, `protocol_layer.h` - Protocol abstraction
- `protocol_telnet.c` - Telnet protocol handling
- `protocol_websocket.c` - WebSocket frame handling

### Redis Caching
- `redis_cache.c`, `redis_cache.h` - Redis integration for player file caching
- `async_cache.c`, `async_cache.h` - Asynchronous caching layer
- Reduces disk I/O for frequently accessed player data

### Logging System Overhaul
- `log.c`, `log.h` - New logging system built on zlog
- Categorized logging: `LOG_INIT`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG`, `LOG_CRITICAL`, `LOG_SQL`, `LOG_HTTP`, `LOG_SCRIPT`, `LOG_SECURITY`, `LOG_COMBAT`, `LOG_SCRIPTS`, `LOG_OLC`
- Stack trace support in debug builds via `log_stacktrace()` and `log_stacktrace_f()`
- Crash handler installation via `log_install_crash_handler()` - captures SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL

### Secrets Management
- `secret.c`, `secret.h` - Unified secrets retrieval
- Priority order: config file -> Doppler mount (`/mnt/secrets`) -> environment variables
- Named pipe support for secrets instead of environment variables
- Supports `secret_get()` for transient access, `secret_get_alloc()` for persistent copies

### Authentication Centralization
- `account/auth.c`, `account/auth.h` - Centralized authentication logic
- `account/auth_migrate.c`, `account/auth_migrate.h` - Legacy auth migration utilities
- Unified handling for passwords, MFA, recovery codes, email verification

### OLC/Editor Reorganization
Editors moved from monolithic `olc_act.c` (was 13,859+ lines) to modular structure:
```
editors/
├── common.c, common.h      # Shared editor framework
├── areas/aedit.c
├── blueprints/bpedit.c, bsedit.c
├── commands/cmdedit.c
├── dungeons/dngedit.c
├── game_settings/gameedit.c
├── help/hedit.c
├── mobiles/medit.c
├── objects/oedit.c
├── projects/pedit.c
├── random_strings/rsgedit.c
├── reserved_vnums/reserved.c, reserved.h
├── rooms/redit.c
├── scripting/olc_mpcode.c
├── ships/shedit.c
├── socials/socialedit.c
├── tokens/tedit.c
└── wilderness/wedit.c  (vledit folded in as VLinks tab)
```

### Nanny System Refactoring
Login/character creation system being modularized:
- `nanny/nanny.h` - Shared declarations
- `nanny/nanny_auth.c`, `nanny/nanny_auth.h` - Authentication handlers
- `nanny/nanny_menus.c`, `nanny/nanny_menus.h` - Menu handlers
- `nanny/nanny_states.c`, `nanny/nanny_states.h` - State machine
- `nanny/nanny_utils.c`, `nanny/nanny_utils.h` - Utility functions

### Reserved VNUM System
- `editors/reserved_vnums/reserved.c`, `reserved.h`
- Dynamic VNUM lookup by symbolic name
- Eliminates hardcoded `#define OBJ_VNUM_*` scattered through code
- Scripting integration: `$(game.reserved_room.ROOM_VNUM_TEMPLE)`

## New Data Structures

### Body Type and Pronouns
```c
typedef enum {
    BODY_TYPE_NEUTRAL,
    BODY_TYPE_MALE,
    BODY_TYPE_FEMALE,
    BODY_TYPE_OTHER,
    BODY_TYPE_RANDOM,
    BODY_TYPE_MAX
} body_type_t;

typedef enum {
    VERB_FORM_DEFAULT,
    VERB_FORM_SINGULAR,
    VERB_FORM_PLURAL
} verb_form_preference_t;

// In CHAR_DATA:
body_type_t body_type;
char *pronoun_he_she;
char *pronoun_him_her;
char *pronoun_his_her;      // Possessive adjective
char *pronoun_his_hers;     // Possessive pronoun
char *pronoun_himself_herself;
```

### New Connection States
```c
#define CON_GET_NEW_BODY_TYPE           76
#define CON_SET_CUSTOM_PRONOUN_SUBJ     78
#define CON_SET_CUSTOM_PRONOUN_OBJ      79
#define CON_SET_CUSTOM_PRONOUN_POSS_ADJ 80
#define CON_SET_CUSTOM_PRONOUN_POSS_PRON 81
#define CON_SET_CUSTOM_PRONOUN_REFL     82
```

## Build System Changes

### Compiler Updates
- Updated for GCC 14.2
- C23 standard (`-std=c23`)
- Debian Trixie compatibility

### CMake Improvements
- Ninja Multi-Config generator
- New build scripts: `./build`, `./install`, `./config`, `./compile`
- Build configurations: debug, release, relwithdebinfo
- Output to `.build/Debug/`, `.build/Release/`, etc.

### File Changes
- `buildnumber.mak` removed
- `compile_commands.json` added for IDE integration
- Makefiles updated for new file structure

## Removed Code

### Deleted Files
- `healer.c` - Automated healer NPCs
- `locker.c` - Old locker system (replaced by account storage)
- `imc.c`, `imc.h`, `imccfg.h` - IMC2 inter-mud communication (~9,300 lines removed)
- `olc_act2.c` - Merged into editor modules
- `buildnumber.mak` - Build numbering

### Removed from const.c
- `race_table[]` - Moved to JSON files
- `crew_table[]` - Commented out, pending reserved VNUM conversion

## Scripting Changes

### New Script Variables
- `$(game.setting.<setting_name>)` - Access game settings
- `$(game.reserved_room.<vnum_name>)` - Access reserved room VNUMs
- `$(game.reserved_obj.<vnum_name>)` - Access reserved object VNUMs
- `$(game.reserved_mob.<vnum_name>)` - Access reserved mobile VNUMs

### Script Command Updates
- `load obj $OBJ_VNUM_SHARD` - Load objects by reserved name
- Similar support for mobs and other loadable entities

## Bug Fixes (Technical)

- Fixed garbage collection check in `obj_update()` preventing segfault
- Fixed account loading race condition causing data loss
- Fixed SSL SIGPIPE handling
- Fixed TLS connection stability issues
- Fixed inventory processing performance for large inventories
- Fixed caching issues causing crashes
- Cache optimization improvements

## Migration Notes

### For New Builds
1. Run `./build` from `/sentience/src`
2. Run `./install` to create symlink

### Race Data
- Race definitions must exist in `/sentience/data/races/` as JSON
- Old `race_table[]` entries need migration to JSON format

### Player Files
- Existing player files need conversion to JSON format
- Migration utilities available in save.c

### Configuration
- Secrets can now use Doppler mounts or named pipes
- Check `secret.h` for configuration options
