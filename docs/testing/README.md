# Testing Framework Documentation

This directory contains comprehensive documentation for the Sentience MUD JSON-driven integration test framework.

## Documentation Files

### [TESTING_FRAMEWORK.md](TESTING_FRAMEWORK.md)
**Complete framework documentation** including:
- Detailed architecture overview
- Writing test code in C with full examples
- Creating JSON test scenarios with complete schema reference
- Running tests with all command-line options
- Extensive examples and best practices
- Memory testing and advanced debugging
- Troubleshooting guide

### [TESTING_QUICK_REFERENCE.md](TESTING_QUICK_REFERENCE.md)
**Quick developer reference** for rapid development:
- 5-minute guide to adding new test types
- Copy-paste code templates
- JSON schema examples
- Helper functions reference
- Common gotchas and solutions

### [TESTING_ROADMAP.md](TESTING_ROADMAP.md)
**Strategic testing roadmap** for long-term planning:
- Evolution from integration to comprehensive testing
- Implementation phases and priorities
- Memory testing strategy
- CI/CD integration planning
- Resource requirements and success metrics

## Quick Start

```bash
# Build with test support
cd /sentience/src
./build tests

# Build with coverage analysis
./build coverage

# Run all tests
cd /sentience
./sent -test

# View coverage report
$BROWSER /sentience/src/coverage_html/index.html

# Run specific test suite
./sent -test:area_loading
```

## Framework Overview

The test framework provides:
- **JSON-Driven Configuration**: Test scenarios in structured JSON
- **Conditional Compilation**: Zero production overhead
- **Dependency Management**: Test ordering and prerequisites  
- **Real MUD Environment**: Integration testing against actual systems
- **Console & Log Output**: Real-time results and detailed logging
- **Code Coverage Analysis**: Automated coverage reporting with gcov/lcov
- **Memory Testing**: AddressSanitizer and Valgrind integration

## Test Structure

```
/sentience/
├── src/tests/                     # Framework source code
│   ├── framework/                 # Core framework implementation
│   ├── integration/               # Integration test handlers
│   └── unit/                      # Unit test handlers (future)
└── data/tests/                    # JSON test configurations
    ├── area_loading_tests.json    # Area validation tests
    ├── wnum_parsing_tests.json    # WNUM parsing tests
    └── database_integrity_tests.json # Config validation tests
```

## Current Test Suites

1. **area_loading** - Validates essential areas load correctly with proper UIDs and vnum ranges
2. **wnum_parsing** - Tests WNUM parsing functionality with different area contexts  
3. **database_integrity** - Validates configuration files and system settings

Recent test status: 6 total tests, 5 passed, 1 failed (full MUD environment integration working).

## Adding New Tests

See [TESTING_QUICK_REFERENCE.md](TESTING_QUICK_REFERENCE.md) for step-by-step examples, or [TESTING_FRAMEWORK.md](TESTING_FRAMEWORK.md) for comprehensive documentation.

The basic process:
1. **Implement test handler** in C code
2. **Create JSON configuration** with test scenarios  
3. **Update build systems** (CMakeLists.txt and Makefile)
4. **Build and test** with framework

All test code is conditionally compiled with `#ifdef BUILD_TESTS` to ensure zero overhead in production builds.