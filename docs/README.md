# Sentience MUD Documentation

## Current Documentation Status

### [Docs Portfolio Review (2026-02-26)](DOCS_REVIEW_2026-02-26.md)
Comprehensive doc-by-doc status ledger for the full `src/docs/` tree, including active plans, open TODOs, worklogs, references, and archived completed docs.

### [Code-Backed Project Reality Audit (2026-02-26)](DOCS_CODE_REALITY_2026-02-26.md)
Per-project active plan audit cross-checked against `src/` implementation artifacts (planned file references and code footprint evidence).

### [Docs Portfolio Review (2026-02-18)](DOCS_REVIEW_2026-02-18.md)
Previous portfolio snapshot retained for historical context.

## Changelog Status

Audience changelog docs were retired from this tree during cleanup.

For current implementation status and historical disposition, use:
- [Docs Portfolio Review (2026-02-26)](DOCS_REVIEW_2026-02-26.md)
- [Roadmap](ROADMAP.md)
- archived completion records under `docs/done/`

## Testing Documentation

### [Testing Framework](testing/TESTING_FRAMEWORK.md)
Comprehensive guide to the JSON-driven integration test framework including:
- Framework architecture and components
- Writing test code in C
- Creating JSON test scenarios
- Running and debugging tests
- Memory testing and analysis
- Code coverage analysis with gcov/lcov
- Complete examples and best practices

### [Testing Quick Reference](testing/TESTING_QUICK_REFERENCE.md)  
Quick developer guide for adding new tests:
- 5-minute guide to adding new test types
- Common test patterns and code templates
- JSON schema examples
- Helper functions and utilities
- Troubleshooting common issues

### [Testing Roadmap](testing/TESTING_ROADMAP.md)
Strategic testing evolution plan:
- Long-term testing strategy
- Implementation phases and priorities  
- Memory testing and CI/CD integration
- Resource planning and success metrics

## RSG Documentation

### [RSGEdit Usage and Integration Guide](guides/RSGEDIT_USAGE.md)
Complete guide for random string generators:
- Step-by-step generator creation workflow
- Pattern/class commands and escaping rules
- Masculine/feminine naming strategies
- Cross-editor and scripting integration plan

## Event Runtime Documentation

### [Event System Admin Guide](guides/EVENT_SYSTEM_ADMIN_GUIDE.md)
Admin/operator guide for live event creation and control:
- End-to-end `evtedit` and `event` workflows
- Schedule setup (manual/recurring/calendar)
- Time scheduling via `event schedule`
- Phase plan configuration and script-driven phase control
- Troubleshooting for common operator issues

### [Event Editor Runtime Reference](guides/EVENT_EDITOR_RUNTIME.md)
Current implementation reference for event editor/runtime behavior:
- Supported schedule/runtime modes
- Bracket validation and aggregation rules
- Progress/completion paths by event type
- Script-facing event command/ifcheck/expansion surfaces
- Deferred items tracked for later phases

### [Event Editor Runtime Worklog](WORKLOG_EVENT_EDITOR_RUNTIME.md)
Chronological summary of what was implemented in the event runtime closure pass:
- Runtime/editor code changes
- Script integration updates
- Validation behavior and known deferred items

### [Event Eprogs Plan](PLAN_EVENT_EPROGS.md)
Design stub for future definition-level event program hooks:
- Lifecycle trigger model (`on_start`, `on_tick`, `on_complete`, etc.)
- Scope-aware execution contract for area/zone-limited effects
- Proposed `evtedit` command surface and phased implementation plan

## Channel Documentation

### [Channel System Docs Index](guides/channels/README.md)
Landing page for player/admin channel guides.

### [Channels Player Guide](guides/channels/CHANNELS_PLAYER_GUIDE.md)
Player-facing usage reference for channels, history, and reporting behavior.

### [Channels Admin Guide](guides/channels/CHANNELS_ADMIN_GUIDE.md)
Admin/operator guide for cedit workflows, moderation controls, and runtime behavior.

## Class System — Extended Data and Features

### [Class Extended Data System](PLAN_CLASS_EXTENDED_DATA.md)
Design for per-class runtime and persistent state beyond level/XP:
- Two-layer model: `custom_data` (JSON persistence) + `void *ext` (runtime C struct)
- Three optional callbacks on `CLASS_DATA`: `ext_init`, `ext_sync`, `ext_free`
- Concrete examples: Ranger pet stable (`RANGER_EXT`), Druid grove (`DRUID_EXT`)
- Full lifecycle: character load, class enter/leave, save, free

### [Druid Grove System](PLAN_DRUID_GROVE.md)
Design for persistent, expandable personal grove instances for druids:
- Persistent dormant instances: `INSTANCE_PERSISTENT` + `INSTANCE_DORMANT` flags
- `sleep_instance()` / `wake_instance()` lifecycle (rooms freed/rebuilt on demand)
- Expandable blueprint sections: `BSECREF_DEFERRED` + `add_section_to_instance()`
- `bp_section_index` fix for section-by-position lookup decoupling
- Grove level progression, ambient bonuses, tending mechanics
- Persistence schema for dormant instances with `active_sections` + `room_states`

## Developer How-To Guides

### [Guides Directory Index](guides/README.md)
Home for non-plan, non-worklog, non-analysis reference docs.

### [BUFFER System Guide](guides/BUFFER_SYSTEM_GUIDE.md)
General reference for BUFFER architecture, API contracts, failure semantics, and migration trade-offs.

### [Adding Skills, Spells, and Commands](guides/SKILL_SPELL_COMMAND_GUIDE.md)
Practical implementation checklist for gameplay additions:
- New command function + registration + cmdedit wiring
- New skill/spell JSON data workflow
- Spell function registration in `skill_data` backend
- Bootstrap seed updates for clean-environment parity
- Build/run validation and common failure modes

### [Wilds Systems Scripting Guide](guides/WILDS_SYSTEMS_SCRIPTING_GUIDE.md)
Reference for wilderness-oriented scripting flows and operational usage.

## Test Framework Overview

The Sentience MUD test framework provides:

- **JSON-Driven Configuration**: Test scenarios defined in structured JSON files
- **Conditional Compilation**: Zero overhead when tests disabled (`#ifdef BUILD_TESTS`)
- **Dependency Management**: Tests can depend on other tests or test suites  
- **Real MUD Environment**: Integration tests run against actual MUD systems
- **Multiple Test Types**: Support for validation, parsing, integration, and custom tests
- **Console & Log Output**: Real-time results with detailed logging

### Quick Start

```bash
# Build with test support
cd /sentience/src  
./build tests

# Run all tests
cd /sentience
./sent -test

# Run specific tests
./sent -test:area_loading
./sent -test:wnum_parsing
```

### Directory Structure

```
/sentience/src/
├── tests/                         # Test framework and data
│   ├── data/                      # JSON test configurations
│   │   ├── integration/           # Integration test definitions
│   │   ├── unit/                  # Unit test definitions
│   │   └── test_config.json       # Test configuration
│   ├── framework/                 # Core framework (test_framework.c, test_loader.c)
│   ├── integration/               # Integration test handlers (wnum_tests.c)
│   └── unit/                      # Unit test handlers
├── docs/                          # This documentation
│   ├── guides/                    # Stable subsystem and API reference guides
│   │   ├── BUFFER_SYSTEM_GUIDE.md
│   │   ├── EVENT_EDITOR_RUNTIME.md
│   │   ├── EVENT_SYSTEM_ADMIN_GUIDE.md
│   │   ├── RSGEDIT_USAGE.md
│   │   ├── SKILL_SPELL_COMMAND_GUIDE.md
│   │   └── WILDS_SYSTEMS_SCRIPTING_GUIDE.md
│   │   └── channels/
│   │       ├── CHANNELS_ADMIN_GUIDE.md
│   │       ├── CHANNELS_PLAYER_GUIDE.md
│   │       └── README.md
│   ├── testing/                   # Testing framework documentation
│   └── README.md                  # This file
```

### Current Test Status

The framework includes these test suites:

1. **area_loading** - Validates essential areas are loaded correctly
2. **wnum_parsing** - Tests WNUM parsing with different area contexts
3. **database_integrity** - Validates configuration and system settings

Recent test run results:
- 6 total tests executed
- 5 tests passed  
- 1 test failed
- Full MUD environment integration working

### Framework Features

#### JSON Configuration
- Structured test definitions with input/output validation
- Support for complex nested data structures
- Version tracking and documentation within test files

#### Dependency System
- Suite-level dependencies (test suites that must run first)
- Test-level dependencies (specific tests within a suite)  
- Circular dependency detection and validation

#### Console Output
- Real-time test execution feedback
- Configurable via zlog.conf  
- Both success and failure details visible

#### Integration Testing
- Tests run against real MUD environment
- Area loading, WNUM parsing, database validation
- Character creation, persistence, and game mechanics

### Adding New Tests

1. **Implement test handler** in `/sentience/src/tests/integration/`
2. **Create JSON configuration** in `/sentience/src/tests/data/`
3. **Update build systems** (CMakeLists.txt and Makefile)  
4. **Build and test** with `./build tests && ./sent -test`

See [TESTING_QUICK_REFERENCE.md](testing/TESTING_QUICK_REFERENCE.md) for step-by-step examples.

## Build Integration

### CMake (Preferred)
```bash
cd /sentience/src
./build tests              # Build Debug with tests
./build release tests      # Build Release with tests  
./install debug            # Install Debug build
```

### Make (Alternative)
```bash
cd /sentience/src
make BUILD_TESTS=1         # Build with tests enabled
```

### Test Compilation
Tests are conditionally compiled based on `BUILD_TESTS` definition:
- Production builds have zero test overhead
- Test framework only included when explicitly enabled
- All test code guarded with `#ifdef BUILD_TESTS`

## Logging Configuration

Test output visibility controlled by `/sentience/data/system/zlog.conf`:

```ini
# Console output for tests
*.=debug >stdout
debug.* >stdout
info.=debug >stdout

# File logging
*.* "./logs/system.log"; DATED
```

This configuration enables both console output during test runs and detailed logging to files.

---

For detailed information, see the specific documentation files:
- [Complete Testing Framework Guide](testing/TESTING_FRAMEWORK.md)
- [Quick Developer Reference](testing/TESTING_QUICK_REFERENCE.md)
- [Strategic Testing Roadmap](testing/TESTING_ROADMAP.md)