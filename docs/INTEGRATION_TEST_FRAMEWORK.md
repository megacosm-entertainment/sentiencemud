# Integration Test Framework Usage

## Overview

The MUD now supports integration testing with full MUD environment loaded, including areas, database, and all game systems. The test framework is conditionally compiled and only included when explicitly enabled.

## Building with Test Support

### Using Build Script (Preferred)
```bash
# Build with test support
./build tests

# Build specific configuration with tests
./build tests release
./build tests gcc debug
./build clean tests
```

### Using Config/Compile Scripts
```bash
# Configure with test support
./config tests
./compile

# Or with specific compiler
./config gcc tests
./compile debug
```

### Using CMake Directly
```bash
# Configure and build with test support
cmake -S . -B .build -DBUILD_TESTS=ON
cmake --build .build --config Debug
```

### Using Make (Alternative)
```bash
# Build with test support
make clean
make BUILD_TESTS=1

# Normal build (no tests)
make
```

## Running Integration Tests

After building with test support, you can run integration tests:

```bash
# Run all tests
./sent -test

# Run specific test category
./sent -test:unit
./sent -test:integration

# Run tests matching a pattern
./sent -test:wnum
```

## Test Categories

- **unit**: Tests for individual functions (WNUM parsing, formatting)
- **integration**: Tests requiring full MUD environment (area loading, database integrity)

## Production Builds

Normal production builds **do not include** any test code:

```bash
./build           # No test code included
./config && ./compile  # No test code included
make              # No test code included
```

If you try to run test mode on a production build, you'll get:
```
Test mode requested but MUD was not compiled with BUILD_TESTS
```

## Adding New Tests

Tests are located in `/sentience/src/test_integration.c`. To add new tests:

1. Add function declaration at top of file
2. Add test case to `test_registry[]` array
3. Implement test function (return 0 for success, non-zero for failure)

Example:
```c
int my_new_test(void) {
    // Test implementation
    if (some_condition) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Test failed: ...");
        return 1;  // Failure
    }
    return 0;  // Success
}
```

## CI/CD Integration

For automated testing in CI pipelines:

```bash
# Build with tests
./build tests

# Run tests and capture exit code
./sent -test
echo $?  # 0 = success, non-zero = failures
```

## Development Workflow Examples

### Quick Testing During Development
```bash
# Build with tests for debugging
./build tests debug

# Run specific test category
./sent -test:unit

# Run pattern matching tests
./sent -test:wnum
```

### Release Testing
```bash
# Build optimized version with tests
./build tests release

# Run full integration test suite
./sent -test:integration
```

### CI Pipeline
```bash
# Automated build and test
./build clean tests release
exit_code=$(./sent -test 2>&1; echo $?)
if [ $exit_code -eq 0 ]; then
    echo "All tests passed"
else
    echo "Tests failed with code $exit_code"
    exit 1
fi
```

## Architecture Benefits

The test framework provides the ideal balance:
- **Clean separation**: No test code in production builds
- **Full integration**: Tests run with complete MUD environment
- **CI-friendly**: Command-line driven with proper exit codes
- **Maintainable**: Tests are isolated but can access all MUD systems
- **Build flexibility**: Works with all build systems (scripts, CMake, Make)