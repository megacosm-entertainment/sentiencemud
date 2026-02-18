# Sentience MUD Documentation

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

### [RSGEdit Usage and Integration Guide](RSGEDIT_USAGE.md)
Complete guide for random string generators:
- Step-by-step generator creation workflow
- Pattern/class commands and escaping rules
- Masculine/feminine naming strategies
- Cross-editor and scripting integration plan

## Event Runtime Documentation

### [Event Editor Runtime Reference](EVENT_EDITOR_RUNTIME.md)
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

See [TESTING_QUICK_REFERENCE.md](TESTING_QUICK_REFERENCE.md) for step-by-step examples.

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