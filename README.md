# Sentience MUD

Sentience is a C-based MUD (Multi-User Dungeon) server descended from DikuMUD/ROM. It has been modernized over 20+ years with JSON data storage, Redis caching, TLS/WebSocket connectivity, a custom scripting engine, and a procedural dungeon system, while preserving the depth and flexibility of its original architecture.

## Features

- **Multiple connection protocols** -- Telnet, TLS, and WebSocket with MCCP compression
- **JSON-based persistence** -- Player accounts, characters, areas, and game settings stored as JSON
- **Redis caching layer** -- Async caching for player data with disk write-behind
- **Online Creation (OLC)** -- Full suite of in-game editors for areas, mobiles, objects, rooms, scripts, blueprints, and dungeons
- **Custom scripting engine** -- Data-driven game logic with 150+ trigger types across 15 dispatch slots
- **Procedural content** -- Blueprint/dungeon system for procedurally generated instances and mazes
- **Widevnum system** -- Area-scoped entity identification (`area_uid:vnum`) for collision-free multi-area development
- **Structured logging** -- zlog-based logging with configurable categories and output targets
- **Bootstrap system** -- Automated directory structure and minimal data generation for fresh deployments

## Quick Start

### Prerequisites

- C compiler (GCC or Clang)
- CMake 3.16+ and Ninja
- Libraries: Jansson (JSON), OpenSSL, libsodium, zlog, hiredis (Redis client)

### Build

```bash
cd /path/to/sentience/src
./build                     # Debug build (default)
./build release             # Optimized release build
./build tests               # Debug build with test framework
./install                   # Symlink binary to project root
```

The build system supports both CMake (preferred) and GNU Make. See [AGENTS.md](AGENTS.md) for full build options.

### Run

```bash
cd /path/to/sentience       # Must run from project root
./sent                      # Start the server
./sent -test                # Run test suite
./sent -g                   # Start with GDB attached
```

The server loads data from relative paths, so it must be run from the project root directory.

### First-Time Setup

On a fresh checkout, the bootstrap system will create the required directory structure and minimal data files on first boot. See [docs/PLAN_BOOTSTRAP.md](docs/PLAN_BOOTSTRAP.md) for details.

## Project Structure

```
sentience/
├── src/                        # Source code (version controlled)
│   ├── account/                # Authentication and account management
│   ├── bootstrap/              # First-run setup and directory creation
│   ├── editors/                # OLC editor implementations
│   │   ├── areas/              #   Area editor (aedit)
│   │   ├── blueprints/         #   Blueprint/section editors (bpedit, bsedit)
│   │   ├── dungeons/           #   Dungeon editor (dngedit)
│   │   ├── mobiles/            #   Mobile editor (medit)
│   │   ├── objects/            #   Object editor (oedit)
│   │   ├── rooms/              #   Room editor (redit)
│   │   └── tokens/             #   Token editor (tedit)
│   ├── io/json/                # JSON serialization for all data types
│   ├── nanny/                  # Connection state machine and login flow
│   ├── tests/                  # Test framework and test suites
│   │   ├── data/               #   JSON test definitions
│   │   ├── framework/          #   Core test framework
│   │   ├── integration/        #   Integration test handlers
│   │   └── unit/               #   Unit test handlers
│   ├── docs/                   # Project documentation
│   │   ├── done/               #   Completed work records
│   │   ├── testing/            #   Test framework documentation
│   │   └── widevnums/          #   Widevnum migration documentation
│   ├── CMakeLists.txt          # CMake build definition
│   ├── Makefile                # GNU Make build definition
│   ├── build                   # One-step build script
│   ├── AGENTS.md               # AI agent instructions
│   ├── CONTRIBUTING.md         # Contribution guidelines
│   └── README.md               # This file
│
├── area/                       # Zone files (JSON and legacy .are)
├── data/                       # Runtime data (races, settings, help, etc.)
├── accounts/                   # Player account files (JSON)
├── characters/                 # Player character files (JSON)
├── logs/                       # Server log files
└── sent                        # Server binary (symlinked from build)
```

## Architecture Overview

Sentience follows the traditional DikuMUD architecture with significant modernization:

- **Game loop** -- Single-threaded main loop in `comm.c` with pulse-based timing
- **Entity model** -- Index templates (`*_INDEX_DATA`) instantiated into runtime objects (`*_DATA`)
- **Persistence** -- JSON files via Jansson, with Redis as a caching layer for hot data
- **Areas** -- Self-contained zones with unique area UIDs; entities addressed via widevnums (`uid:vnum`)
- **Scripting** -- Trigger-based system with 15 dispatch slots for efficient runtime execution
- **OLC** -- Modular in-game editors for all content types, supporting both legacy and JSON formats
- **Networking** -- Multi-protocol listener with Telnet/TLS/WebSocket support

## Current Development Focus

The primary active project is the **widevnum migration** -- transitioning from global vnum identity to area-scoped widevnums (`area_uid:local_vnum`). Core infrastructure is complete; remaining work focuses on command updates, cross-area comparison fixes, and display consistency.

See [ROADMAP.md](docs/ROADMAP.md) for the full development roadmap.

## Testing

```bash
cd /path/to/sentience/src
./build tests               # Build with test support

cd /path/to/sentience
./sent -test                # Run all tests
./sent -test:unit           # Run unit tests only
./sent -test:integration    # Run integration tests only
./sent -test:wnum           # Run pattern-matched tests
```

Tests are JSON-driven: C test handlers in `src/tests/` implement logic, JSON files in `src/tests/data/` define test cases and expected outputs. Tests are conditionally compiled (`#ifdef BUILD_TESTS`) with zero production overhead.

See [docs/testing/](docs/testing/) for comprehensive testing documentation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on code style, branching, testing, and submitting changes.

## Documentation

Internal documentation lives in [docs/](docs/):

| Pattern | Purpose |
|---------|---------|
| `PLAN_*.md` | Design notes and implementation plans |
| `TODO_*.md` | Task tracking for specific features |
| `WORKLOG_*.md` | Records of completed work sessions |
| `TECH_DEBT.md` | Technical debt tracker with priorities |
| `ROADMAP.md` | Development roadmap and status |

## License

Sentience is derived from DikuMUD (1990), Merc (1992), and ROM 2.4 (1993-1998). All three license chains apply. The code may not be used for commercial purposes. Copyright notices must be preserved, and credits must be displayed to players. Code contributed to the project is given freely and irrevocably. See [LICENSE.md](LICENSE.md) for the full license texts.
