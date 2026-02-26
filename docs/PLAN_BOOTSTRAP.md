# Bootstrap Mode Implementation Plan

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


## Context

A fresh clone of the Sentience MUD repository lacks the runtime data files needed to start the game. Currently, a new developer must manually create or obtain account files, area files, race definitions, and system configuration before the server will boot. This creates a significant barrier to entry for new contributors and testing scenarios.

This plan implements a `-bootstrap` mode that:
- Creates minimal viable data files from scratch
- Guides the user through creating the first staff (implementor) account interactively
- Optionally supports automation for CI/testing scenarios
- Ensures the game can boot successfully after bootstrap completes

## Implementation Approach

### High-Level Flow

```
./sent -bootstrap
  ↓
1. Detect bootstrap flag early in main()
2. Check if bootstrap is needed (missing critical files)
3. Create minimal data file structure
4. Interactive prompt for first staff account
5. Verify success
6. Continue with normal boot_db() flow
```

Bootstrap runs **before** `boot_db()` to ensure all required files exist before the standard loading process begins.

## Critical Files Analysis

**Must exist or game exits:**
- `data/system/gconfig.rc` - Global config with UID tracking (exits code 1 if missing)
- `data/world/area.lst` - List of areas to load (exits code 1 if missing)
- At least one area file in `area/` directory (exits code 2 if area fails to load)
- At least one race in `data/races/` directory

**Auto-recovers if missing:**
- `commands.json` - Required; bootstrap must generate it (see TODO below)
- `help.txt` - Auto-creates root category structure

**Optional (warns but continues):**
- `socials.json`, `ban.json`, `chat_rooms.json`, `reserved.json`

## Implementation Components

### 1. New Source Files

**`src/bootstrap.c`** - Bootstrap implementation (~500-700 lines)
- Core orchestration functions
- File creation helpers
- Interactive prompts
- Account/character creation

**`src/bootstrap.h`** - Bootstrap API
- Function declarations
- Constants and flags

### 2. Modified Files

**`src/comm.c`** - Integration point
- Add `detect_bootstrap_mode()` call after `log_init()` (after line 499)
- Add conditional `run_bootstrap()` call before `game_settings_read()`
- Modify `parse_options()` to handle `-bootstrap` flag family

### 3. Build System Updates

**`src/Makefile`**:
```makefile
SOURCES += bootstrap.c
OBJECTS += obj/bootstrap.o
```

**`src/CMakeLists.txt`**:
```cmake
set(sentience_SOURCES
    # ... existing files ...
    bootstrap.c
    bootstrap.h
)
```

## Key Functions to Implement

### Core Orchestration (bootstrap.c)

```c
// Main entry point, returns 0 on success
int run_bootstrap(void);

// Determine if bootstrap needed (check critical files)
bool check_bootstrap_needed(void);

// Create all minimal data files
bool create_minimal_data_files(void);

// Interactive staff account setup
bool interactive_account_setup(void);

// Verify bootstrap succeeded
bool verify_bootstrap_success(void);
```

### File Creation Helpers (bootstrap.c)

```c
bool ensure_directory_structure(void);  // Create accounts/, characters/, data/, area/
bool create_gconfig_rc(void);           // UID tracking config
bool create_area_lst(void);             // Single-area list
bool create_limbo_area(void);           // Minimal JSON area
bool create_human_race(void);           // Copy existing or create minimal
bool create_game_settings(void);        // Minimal game_settings.json
```

### Interactive Prompts (bootstrap.c)

```c
char *prompt_username(void);            // Validate alphanumeric, 3-12 chars
char *prompt_email(void);               // Basic @ validation
char *prompt_password(const char *prompt); // No echo via termios
```

### Account/Character Creation (bootstrap.c)

```c
bool bootstrap_create_account(const char *username, const char *email, const char *password);
bool bootstrap_create_character(ACCOUNT_DATA *account, const char *name);
```

**Reuses existing functions:**
- `new_account()` from mem.c
- `hash_password()` from account/auth.c
- `save_account()` from save.c (line 6236)
- `account_add_character()` from save.c (line 6273)
- `new_char()` / `new_pcdata()` from mem.c
- `save_char_obj()` from save.c

## Minimal Data Templates

### gconfig.rc
```
DBversion 16777217
NextMobUID 1 0
NextObjUID 1 0
NextTokenUID 1 0
NextVRoomUID 1 0
NextShipUID 1 0
NextAreaUID 1
NextWildsUID 0
NextVlinkUID 0
NextChurchUID 1
END
```

### area.lst
```
limbo.json
$
```

### limbo.json (via jansson)
```json
{
  "schema_version": "1.0.0",
  "area": {
    "uid": 1,
    "name": "Limbo",
    "filename": "limbo.json",
    "vnums": {"min": 1, "max": 10},
    "metadata": {
      "builders": "Bootstrap",
      "security": 9,
      "levels": {"min": 0, "max": 0}
    }
  },
  "rooms": [{
    "vnum": 1,
    "name": "The Void",
    "description": "Bootstrap limbo room. Use OLC to build your world.\n\r",
    "flags": ["indoors", "no_mob", "safe"],
    "sector": 0
  }]
}
```

### game_settings.json
```json
{
  "_version": "1.0",
  "_format": "game_settings",
  "core": {
    "env_var_prefix": "SENTIENCE_",
    "secrets_mount": "/sentience/data/system/.setting_override"
  },
  "email": {"email_enable": false},
  "dev_server": false,
  "testport": false
}
```

## Staff Account Creation Flow

Following the pattern in `nanny.c:finalize_staff_character_creation()` (line 4783):

1. Create account structure with `new_account()`
2. Set username, email, hashed password
3. Set `staff_account = true`
4. Generate account IDs (done automatically in `save_account()`)
5. Save account with `save_account()`
6. Create character with `new_char()` / `new_pcdata()`
7. Set `pcdata->staff_rank = STAFF_IMPLEMENTOR` (rank 6, highest level)
8. Link character to account via `account_add_character()`
9. Save character with `save_char_obj()`

**Staff rank constants** (from merc.h line 1033):
- `STAFF_PLAYER` = 0
- `STAFF_IMMORTAL` = 2
- `STAFF_IMPLEMENTOR` = 6 (max rank)

## Interactive Prompt Flow

```
Bootstrap Mode - Sentience MUD
==============================

Creating minimal game data files...
  Creating gconfig.rc... OK
  Creating area.lst... OK
  Creating limbo.json... OK
  Creating human.json... OK (if needed)
  Creating game_settings.json... OK

=== First Staff Account Setup ===

Enter username for implementor account: admin
Enter email address: admin@example.com
Enter password (8+ characters): ********
Confirm password: ********

Creating staff account...
Staff account 'admin' created successfully.
Rank: Implementor

Bootstrap complete!
You may now run the game normally.
```

## Command-Line Options

### Basic Usage
```bash
./sent -bootstrap
```

### Automation (for CI/testing)
```bash
./sent -bootstrap \
  --bootstrap-auto \
  --bootstrap-username=admin \
  --bootstrap-email=admin@example.com \
  --bootstrap-password=changeme123
```

**Flags:**
- `-bootstrap` - Enable bootstrap mode
- `--bootstrap-auto` - Non-interactive (uses provided or default values)
- `--bootstrap-username=<name>` - Pre-specify username
- `--bootstrap-email=<email>` - Pre-specify email
- `--bootstrap-password=<pwd>` - Pre-specify password (automation only)

## Password Security

**Terminal Security:**
- Use `termios` to disable echo during password input
- Zero password memory after use
- Never log passwords

**Password Storage:**
- Reuse existing `hash_password()` from `account/auth.c`
- Currently uses `crypt()` with salt
- Minimum 8 characters enforced

**File Permissions:**
- Account files: 600 (owner read/write)
- Config files: 644 (world readable)

## Error Handling

**Principles:**
- Fail fast if any critical file creation fails
- Idempotent: safe to re-run (skip existing files)
- Clear error messages with specific failures
- No partial state on failure

**Example:**
```c
bool create_minimal_data_files(void) {
    printf("Creating minimal data files...\n");

    if (!ensure_directory_structure()) {
        fprintf(stderr, "Failed to create directory structure\n");
        return false;
    }

    // Skip if file already exists
    if (access("data/system/gconfig.rc", F_OK) == 0) {
        printf("  gconfig.rc already exists... SKIP\n");
    } else {
        printf("  Creating gconfig.rc... ");
        if (!create_gconfig_rc()) {
            fprintf(stderr, "FAILED\n");
            return false;
        }
        printf("OK\n");
    }

    // ... repeat for other files

    return true;
}
```

## Testing Strategy

### Manual Testing Scenarios

1. **Fresh Install**: Delete all data files, run `-bootstrap`, verify game boots
2. **Partial Install**: Delete only some files, verify bootstrap completes
3. **Existing Install**: Run on populated system, verify no-op (doesn't overwrite)
4. **Invalid Input**: Test password mismatch, invalid username, etc.
5. **Automated Mode**: Test non-interactive flags for CI

### Automated Testing

**Unit Tests** (if test framework supports):
- File creation functions (verify content)
- Input validation (username, email, password)
- Directory structure creation

**Integration Test** (add to `data/tests/`):
```json
{
  "test_suite": "bootstrap_tests",
  "tests": [{
    "name": "automated_bootstrap",
    "test_type": "bootstrap_test",
    "input": {
      "username": "testadmin",
      "email": "test@example.com",
      "password": "testpass123"
    },
    "expected_output": {
      "success": true,
      "files_created": [
        "data/system/gconfig.rc",
        "data/world/area.lst",
        "area/limbo.json"
      ]
    }
  }]
}
```

### Verification Commands

After bootstrap:
```bash
# Verify files exist
ls -la data/system/gconfig.rc
ls -la data/world/area.lst
ls -la area/limbo.json

# Verify account created
ls -la accounts/a/admin.json

# Verify character created
ls -la characters/a/admin.json

# Boot the game
./sent
```

## Implementation Steps

### Phase 1: Core Infrastructure (Day 1)
1. Create `bootstrap.c` and `bootstrap.h`
2. Implement file creation functions
3. Add bootstrap detection to `comm.c`
4. Update `Makefile` and `CMakeLists.txt`
5. Basic smoke testing

### Phase 2: Interactive Setup (Day 2)
1. Implement interactive prompts (with termios for password)
2. Implement account creation (reusing existing functions)
3. Implement character creation with STAFF_IMPLEMENTOR rank
4. Integration testing with actual game boot

### Phase 3: Polish & Documentation (Day 3)
1. Add automation flags (`--bootstrap-auto`, etc.)
2. Error handling refinement
3. Edge case testing
4. Create `docs/BOOTSTRAP.md` documentation
5. Update main README with bootstrap instructions

## Verification Checklist

After implementation, verify:

- [ ] Fresh system: Delete all data, run `-bootstrap`, game boots successfully
- [ ] Account created: Staff account exists in `accounts/` with correct rank
- [ ] Character created: Staff character exists in `characters/` with STAFF_IMPLEMENTOR rank
- [ ] Login works: Can log in with created credentials
- [ ] Commands work: Staff commands available to implementor
- [ ] OLC access: Can edit areas, rooms, mobs, objects
- [ ] No overwrites: Running bootstrap twice doesn't destroy existing data
- [ ] Automation works: `--bootstrap-auto` mode works for CI
- [ ] Both builds work: Make and CMake both include new files

## TODO: Bootstrap Must Generate commands.json

The legacy `commands.dat` loader and the `cmd_table[]` bootstrap fallback have been removed
from `cmdedit.c`. The game now requires `data/system/commands.json` to exist at startup.

Bootstrap mode must generate a valid `commands.json` using the same format as
`json_save_commands()` in `io/json/json_commands.c`. The command definitions should be
built programmatically from `cmd_table[]` (which still exists in `interp.c` for this
purpose) or from a bundled template.

Once bootstrap can generate `commands.json`, the `cmd_table[]` array in `interp.c`
(~1200 lines) and its `struct cmd_type` / `interp.h` extern can also be removed.

## Future Enhancements (Out of Scope)

- Data pack download via HTTP (`--datapack=<url>`)
- Multi-area bootstrap with more content
- Web-based bootstrap UI for Docker deployments
- Enhanced password hashing (bcrypt/scrypt)

## Files Summary

**New Files:**
- `/sentience/src/bootstrap.c` - Bootstrap implementation
- `/sentience/src/bootstrap.h` - Bootstrap API
- `/sentience/docs/BOOTSTRAP.md` - User documentation

**Modified Files:**
- `/sentience/src/comm.c` - Bootstrap integration
- `/sentience/src/Makefile` - Build system (add bootstrap.c)
- `/sentience/src/CMakeLists.txt` - Build system (add bootstrap.c)

**Reused Functions:**
- `new_account()` (mem.c)
- `hash_password()` (account/auth.c)
- `save_account()` (save.c:6236)
- `account_add_character()` (save.c:6273)
- `new_char()`, `new_pcdata()` (mem.c)
- `save_char_obj()` (save.c)
- `player_exists()` (handler.c)
