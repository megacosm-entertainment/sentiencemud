# Bootstrap Mode - Sentience MUD

## Overview

Bootstrap mode allows you to set up a fresh Sentience MUD installation from just the source code. It creates all necessary minimal data files and guides you through creating the first staff (implementor) account.

This is particularly useful for:
- New developers setting up their development environment
- Fresh installations from source repository
- Testing and CI/CD environments
- Creating clean sandbox environments

## Quick Start

### Basic Usage

```bash
cd /sentience
./src/sent -bootstrap
```

This will:
1. Create minimal required data files
2. Generate commands.json from hardcoded command table
3. Prompt you interactively for staff account details
4. Create your first implementor account
5. Exit (ready for normal startup)

After bootstrap completes, start the game normally:

```bash
./sent
```

### Automated Mode (CI/Testing)

For non-interactive use:

```bash
./src/sent -bootstrap \
  --bootstrap-auto \
  --bootstrap-username=admin \
  --bootstrap-email=admin@example.com \
  --bootstrap-password=changeme123
```

For isolated CI roots with fixture areas:

```bash
./src/sent -bootstrap \
   --bootstrap-auto \
   --bootstrap-username=ciadmin \
   --bootstrap-email=ci@example.com \
   --bootstrap-password=changeme123 \
   --bootstrap-root=/tmp/sentience-ci \
   --bootstrap-fixtures=ci
```

## What Bootstrap Creates

### Critical System Files

**data/system/gconfig.rc** - Global configuration
```
DBversion 16777217
NextMobUID 1 0
NextObjUID 1 0
...
```

**data/world/area.lst** - Area registry
```
limbo.json
$
```

**area/limbo.json** - Minimal starting area
- Single room: "The Void"
- Safe, indoors, no mobs
- UID 1, vnum 1

**data/system/commands.json** - All game commands
- Generated from cmd_table[] array
- ~300+ commands with metadata
- Ready for OLC editing

**data/system/game_settings.json** - Game configuration
- Email disabled by default
- Default network ports
- Minimal viable settings

**data/system/reserved.json** - Reserved vnums
- Empty initial structure
- Ready for special entity tracking

**data/races/human.json** - Minimal human race
- Basic attributes and anatomy
- Created only if no race files exist
- Sufficient for basic testing

### Directory Structure

Bootstrap creates all required directories:
```
/sentience/
├── data/
│   ├── system/      # Config files
│   ├── world/       # World data
│   ├── races/       # Race definitions
│   ├── help/        # Help files
│   ├── notes/       # Notes system
│   ├── orgs/        # Organizations
│   ├── persist/     # Persistent entities
│   └── dump/        # Core dumps
├── area/            # Zone files
├── accounts/        # Account files (a-z subdirs)
├── characters/      # Character files (a-z subdirs)
└── logs/            # Log output
```

### Staff Account

Bootstrap creates the first staff account with:
- **Account**: Username, email, hashed password
- **Character**: Same name as account
- **Rank**: STAFF_IMPLEMENTOR (level 6, highest)
- **Level**: MAX_LEVEL
- **Permissions**: Full access to all commands and OLC

The account and character are saved to:
- `/sentience/accounts/<first_letter>/<username>`
- `/sentience/characters/<first_letter>/<Username>`

## Command-Line Options

| Option | Description |
|--------|-------------|
| `-bootstrap` | Enable bootstrap mode (interactive) |
| `--bootstrap-auto` | Non-interactive mode |
| `--bootstrap-username=<name>` | Pre-specify username (3-12 alphanumeric) |
| `--bootstrap-email=<email>` | Pre-specify email address |
| `--bootstrap-password=<pwd>` | Pre-specify password (8+ characters) |
| `--bootstrap-fixtures=ci` / `--bootstrap-ci-fixtures` | Generate CI fixture areas and append them to `data/world/area.lst` |
| `--bootstrap-root=<path>` | Run bootstrap inside an alternate root directory |
| `--data-root=<path>` / `--game-root=<path>` | Runtime root override for relative-path access (experimental) |
| `SENTIENCE_DATA_ROOT` | Environment fallback for runtime root override |

### Current Root Override Limitations

- `--bootstrap-root` now applies to bootstrap-created assets and account/character persistence paths.
- Runtime `--data-root` / `--game-root` remaps compile-time `/sentience/...` path constants through the runtime root override.
- The test framework currently expects `src/tests/data` relative to process working directory, so `-test:*` runs may fail when launched directly from an isolated root without test assets.

### Path Compatibility Policy (Current)

- Keep `GAME_DIR` and dependent macros (`DATA_DIR`, `SYSTEM_DIR`, `AREA_DIR`, `LOG_DIR`, etc.) in place during the migration.
- Use `resolve_game_path()` at runtime to remap compile-time `/sentience/...` paths when `--data-root`, `--game-root`, or `--bootstrap-root` is set.
- Do **not** remove `GAME_DIR` until high-risk filesystem callsites are migrated and startup validation for critical paths is in place.
- This preserves backward compatibility and avoids regressions where startup behavior depends on launching from a specific working directory.

## CI Fixture Areas

When `--bootstrap-fixtures=ci` is used, bootstrap also creates:
- `area/bootstrap_reserved_fixture.json`
- `area/bootstrap_dummy_fixture.json`

And appends both files to `data/world/area.lst` after `limbo.json`.

Fixture mode also bumps `NextAreaUID` in `data/system/gconfig.rc` from `2` to `4`, reserving UIDs for the generated fixture areas.

## Interactive Flow

When run interactively, bootstrap will:

1. **Check for existing files**
   - Skips files that already exist
   - Reports what needs to be created

2. **Create data files**
   ```
   Creating minimal data files...
     Directory structure verified.
     Creating gconfig.rc... OK
     Creating area.lst... OK
     Creating limbo.json... OK
     Checking race files... OK
     Creating game_settings.json... OK
     Creating commands.json from cmd_table[]... OK
     Creating reserved.json... OK
   ```

3. **Prompt for staff account**
   ```
   === First Staff Account Setup ===

   Enter username for implementor account (3-12 alphanumeric): admin
   Enter email address (optional, press Enter to skip): admin@example.com
   Enter password (8+ characters): ********
   Confirm password: ********
   ```

4. **Create and save**
   ```
   Creating staff account 'admin'...
   Account created and saved.
   Creating implementor character 'admin'...
   Character created with STAFF_IMPLEMENTOR rank.
   ```

5. **Verify and exit**
   ```
   Verifying bootstrap...
   Bootstrap verification successful!

   ======================================
     Bootstrap Complete!
   ======================================

   You can now start the game normally.
   Your implementor account has been created.
   ```

## Validation Rules

### Username
- 3-12 characters
- Alphanumeric only
- Must start with a letter
- Converted to lowercase
- Checked against existing accounts

### Email
- Optional (can be empty)
- Must contain @ and . if provided
- Basic format validation

### Password
- Minimum 8 characters
- Hashed via libsodium (Argon2id path)
- Password input is hidden (no echo)
- Confirmation required

## Safety Features

### Idempotent
- Safe to run multiple times
- Skips files that already exist
- Won't overwrite existing data

### No Overwrites
- Bootstrap will not overwrite existing accounts
- Check performed before account creation
- Fails gracefully if account exists

### Error Handling
- Clear error messages for each failure
- Exits immediately on critical errors
- No partial state left on failure

## Troubleshooting

### "Account already exists"
Bootstrap won't overwrite existing accounts. Either:
- Use a different username
- Manually delete the existing account file
- Skip account creation if files are the goal

### "No race files found"
If you see warnings about race files:
- Copy race files from existing installation
- Place .json files in `/sentience/data/races/`
- Bootstrap creates minimal human.json as fallback

### "Failed to create directory"
Check permissions:
```bash
chmod 755 /sentience
chown -R youruser:yourgroup /sentience
```

### "Commands.json generation failed"
Ensure the binary has access to cmd_table[]:
- Should not happen in normal builds
- Verify compile completed successfully
- Check for linker errors

## Integration with Build System

Bootstrap is integrated into both build systems:

**Makefile:**
```makefile
C_FILES = \
    ...
    bootstrap.c \
    ...
```

**CMakeLists.txt:**
```cmake
set(SOURCE_FILES
    ...
    bootstrap.c
    ...
)
```

## Testing Bootstrap

### Manual Test
```bash
# Remove critical files
cd /sentience
rm -f data/system/gconfig.rc data/system/commands.json

# Run bootstrap
./src/sent -bootstrap

# Verify files created
ls -l data/system/gconfig.rc
ls -l data/system/commands.json
ls -l accounts/*/youraccountname
```

### Automated Test
```bash
./src/sent -bootstrap \
  --bootstrap-auto \
  --bootstrap-username=testuser \
  --bootstrap-email=test@test.com \
  --bootstrap-password=testpass123

# Check exit code
echo $?  # Should be 0 for success
```

### Verification Checklist
After bootstrap, verify:
- [ ] gconfig.rc exists and has valid UID tracking
- [ ] area.lst exists and lists limbo.json
- [ ] limbo.json exists with valid JSON structure
- [ ] commands.json exists with 300+ commands
- [ ] reserved.json exists (may be empty)
- [ ] Account file created in accounts/<letter>/<username>
- [ ] Character file created in characters/<letter>/<Username>
- [ ] Game starts normally: `./sent`
- [ ] Can log in with created credentials
- [ ] Staff commands available

## Future Enhancements

### Data Pack Download (Planned)
```bash
./src/sent -bootstrap --datapack=https://sentiencemud.net/starter-pack.tar.gz
```

This would:
- Download pre-built area, race, and help files
- Extract to appropriate directories
- Provide a richer starting environment

### Web-Based Bootstrap (Planned)
For Docker/container deployments:
- Browser-based setup wizard
- Pre-configured environment templates
- One-click deployment options

## Technical Details

### Implementation
- **Files**: `/sentience/src/bootstrap/bootstrap.c` and `/sentience/src/bootstrap/bootstrap_*.c`
- **Headers**: `/sentience/src/bootstrap/bootstrap.h`, `/sentience/src/bootstrap/bootstrap_internal.h`
- **Integration**: `/sentience/src/comm.c` (argument parsing + startup)

### Key Functions
- `detect_bootstrap_mode()` - Parse command-line flags
- `run_bootstrap()` - Main orchestration
- `create_minimal_data_files()` - File creation
- `interactive_account_setup()` - Account creation
- `bootstrap_create_account()` - Account structure + save
- `bootstrap_create_character()` - Character structure + save

### Reused Functions
Bootstrap leverages existing game functions:
- `new_account()` - Account allocation
- `hash_password()` - Password hashing
- `save_account()` - Account persistence
- `account_add_character()` - Link character to account
- `new_char()` / `new_pcdata()` - Character allocation
- `save_char_obj()` - Character persistence
- `player_exists()` - Duplicate check

### Password Security
- Input hidden via termios (no echo)
- Hashed via libsodium (Argon2id flow)
- Memory zeroed before free
- Never logged or displayed

## See Also

- [CLAUDE.md](../CLAUDE.md) - General development guide
- [Build System](../CLAUDE.md#build-system) - Building the project
- [Testing](../CLAUDE.md#testing-framework) - Test framework
- [OLC](../CLAUDE.md#olc-editors) - Online creation editors
