# Unit Testing Framework Documentation

## Overview

Sentience MUD features a comprehensive JSON-driven integration test framework that validates core MUD functionality including area loading, WNUM parsing, database integrity, and more. The framework supports dependency management, structured test scenarios, and real MUD environment validation.

## Table of Contents

- [Quick Start](#quick-start)
- [Test Framework Architecture](#test-framework-architecture)
- [Writing Test Code](#writing-test-code)
- [Creating Test Scenarios](#creating-test-scenarios)
- [Running Tests](#running-tests)
- [Examples](#examples)
- [Best Practices](#best-practices)

## Quick Start

### Running Tests

```bash
# Run all tests
cd /sentience
./sent -test

# Run specific test pattern
./sent -test:area_loading
./sent -test:wnum

# Build with test support
cd /sentience/src
./build tests
```

### Console Output

Tests output both to console and logs. Configuration in `/sentience/data/system/zlog.conf` controls visibility:

```
[TESTS] Running test suite: area_loading
  [TEST] essential_areas_loaded (area_existence_check)... PASS
  [TEST] area_integrity_validation (area_integrity_check)... PASS

=== Test Results ===
Total:   6
Passed:  5
Failed:  1
Errors:  0
Skipped: 0
==================
```

## Test Framework Architecture

### Directory Structure

```
/sentience/
├── src/tests/                     # Test framework source code
│   ├── framework/                 # Core framework implementation
│   │   ├── test_framework.h       # Framework headers and types
│   │   ├── test_framework.c       # Core framework functions
│   │   └── test_loader.c          # JSON test loading and discovery
│   ├── integration/               # Integration test implementations
│   │   └── wnum_tests.c           # WNUM and area testing handlers
│   └── unit/                      # Unit test implementations (future)
├── src/tests/data/                # JSON test scenario definitions
│   ├── integration/               # Integration test definitions
│   ├── unit/                      # Unit test definitions
│   └── test_config.json           # Test configuration
```

### Component Overview

1. **Test Framework Core** (`src/tests/framework/`):
   - Manages test discovery and execution
   - Handles JSON loading and parsing
   - Validates test dependencies
   - Provides reporting infrastructure

2. **Test Implementations** (`src/tests/integration/`):
  - Contains actual test logic grouped by system/module
  - Each module exposes `run_<module>_test_case()` (e.g., `run_skill_data_test_case()`)
  - Validates real MUD environment functionality

3. **Central Dispatcher** (`src/tests/framework/test_dispatcher.c`):
  - Owns `run_test_case()` routing
  - Routes by `test_type` prefix/identifier to module dispatchers
  - Keeps framework flow independent from any one test module

4. **Test Scenarios** (`src/tests/data/`):
   - JSON configuration files defining test cases
   - Structured input/output specifications
   - Dependency declarations
   - Test metadata and documentation

### Bootstrap Fixture Policy

Cross-area tests are valid and may reference existing gameplay zones such as
`bootstrap.json` and `limbo.json`.

For test fixture data (new rooms, mobs, objects, resets, shops), add entities only
in `src/bootstrap/bootstrap_data/area/tests.json`.

- Add new fixture entities there first.
- Reference those fixtures from test JSON in `src/tests/data/integration/`.
- Prefer fixture-bound suites (for example `bootstrap_fixture_tests`) in CI profiles.

## Writing Test Code

### Adding New Test Handlers

Test handlers are implemented in C and dispatch based on the `test_type` field in JSON configurations.

#### 1. Add Test Type Handler

Create or extend a module file under `src/tests/unit/` or `src/tests/integration/`
and add routing in `src/tests/framework/test_dispatcher.c`:

```c
// Add forward declaration
static test_result_t test_my_new_feature(test_case_t *test);

// Add to dispatcher in run_test_case()
if (strcmp(test->test_type, "my_feature_test") == 0) {
    result = test_my_new_feature(test);
}

// Implement the handler
static test_result_t test_my_new_feature(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected_output = json_object_get(test->config, "expected_output");
    
    // Extract test parameters from JSON
    const char *param = json_get_string(input, "parameter");
    bool expected_result = json_get_bool(expected_output, "success");
    
    // Perform test logic
    bool actual_result = my_function_to_test(param);
    
    if (actual_result != expected_result) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                      "Test failed: expected %s, got %s",
                      expected_result ? "true" : "false",
                      actual_result ? "true" : "false");
        return TEST_FAILURE;
    }
    
    return TEST_SUCCESS;
}
```

#### 2. Update Build Configuration

Add new test files to both CMakeLists.txt and Makefile:

**CMakeLists.txt**:
```cmake
if(BUILD_TESTS)
    list(APPEND SOURCE_FILES 
        test_integration.c
        tests/framework/test_framework.c
        tests/framework/test_loader.c
        tests/integration/wnum_tests.c
        tests/integration/my_new_tests.c  # Add new file
    )
endif()
```

**Makefile**:
```makefile
ifdef BUILD_TESTS
    C_FILES += test_integration.c \
               tests/framework/test_framework.c \
               tests/framework/test_loader.c \
               tests/integration/wnum_tests.c \
               tests/integration/my_new_tests.c  # Add new file
endif
```

### Test Result Types

```c
typedef enum {
    TEST_SUCCESS = 0,  // Test passed
    TEST_FAILURE = 1,  // Test failed but ran properly
    TEST_ERROR = 2,    // Test encountered an error
    TEST_SKIP = 3      // Test was skipped
} test_result_t;
```

### Helper Functions

The framework provides utility functions for JSON parsing:

```c
// JSON access helpers
const char *json_get_string(json_t *obj, const char *key);
int json_get_int(json_t *obj, const char *key);
bool json_get_bool(json_t *obj, const char *key);

// MUD environment helpers
bool test_environment_ready(void);          // Check if MUD is properly loaded
AREA_DATA *find_area(char *name);           // Find area by name
bool parse_widevnum(char *argument, AREA_DATA *current_area, WNUM *wnum);

// Logging helpers
log_message(level, category, message);
log_message_f(level, category, format, ...);
```

## Creating Test Scenarios

### JSON Test Configuration

Test scenarios are defined in JSON files in `/sentience/src/tests/data/`. Each file defines a test suite with multiple test cases.

#### Basic Structure

```json
{
  "test_suite": "my_feature_tests",
  "description": "Tests for my new feature",
  "version": "1.0",
  "requires_mud_environment": true,
  "dependencies": ["area_loading"],
  "timeout_seconds": 60,
  "tests": [
    {
      "name": "basic_functionality",
      "description": "Test basic feature operation",
      "dependencies": [],
      "test_type": "my_feature_test",
      "input": {
        "parameter": "test_value",
        "options": ["option1", "option2"]
      },
      "expected_output": {
        "success": true,
        "result_value": "expected_result"
      }
    }
  ]
}
```

#### Field Reference

**Test Suite Fields**:
- `test_suite`: Unique identifier for the test suite
- `description`: Human-readable description
- `version`: Version number for tracking changes
- `requires_mud_environment`: Whether tests need full MUD initialization
- `dependencies`: Array of test suite names this depends on
- `timeout_seconds`: Maximum execution time
- `tests`: Array of test case objects

**Test Case Fields**:
- `name`: Unique test case identifier within suite
- `description`: Human-readable description of what is tested
- `dependencies`: Array of test case names within this suite that must run first
- `test_type`: Handler type for dispatching (e.g., `wnum_parser`, `area_existence_check`)
- `input`: Structured input data for the test
- `expected_output`: Expected results for validation

### Dependency Management

Tests can depend on other tests or test suites:

```json
{
  "test_suite": "advanced_features",
  "dependencies": ["area_loading", "database_integrity"],
  "tests": [
    {
      "name": "complex_test",
      "dependencies": ["basic_setup_test"],
      "test_type": "complex_validator",
      ...
    }
  ]
}
```

Dependencies are validated before test execution. Circular dependencies are detected and reported as errors.

### Environment Requirements

Tests that need the full MUD environment (areas loaded, database initialized) should set:

```json
{
  "requires_mud_environment": true
}
```

Tests marked this way are skipped if the MUD environment is not ready.

## Running Tests

### Command Line Options

```bash
# Run all tests
./sent -test

# Run all tests (explicit)
./sent -test:all

# Run specific test suite
./sent -test:area_loading

# Run tests matching pattern
./sent -test:wnum
./sent -test:database
```

### Exit Codes

- `0`: All tests passed
- `>0`: Number of failed/error tests

### Output Locations

- **Console**: Real-time test execution with results
- **Logs**: Detailed execution in `/sentience/logs/system.log`
- **Return Code**: For scripting and CI integration

### Build Requirements

Tests are only compiled when BUILD_TESTS is defined:

```bash
# CMake build with tests
./build tests

# Make build with tests  
make BUILD_TESTS=1

# Install test-enabled build
./install debug
```

## Examples

### Example 1: Area Validation Test

**JSON Configuration** (`src/tests/data/integration/area_validation_tests.json`):
```json
{
  "test_suite": "area_validation", 
  "description": "Validate area loading and integrity",
  "version": "1.0",
  "requires_mud_environment": true,
  "dependencies": [],
  "tests": [
    {
      "name": "required_areas_exist",
      "description": "Check that essential areas are loaded",
      "test_type": "area_existence_check",
      "input": {
        "required_areas": [
          {
            "name": "Limbo",
            "expected_uid": 923,
            "vnum_range": [1, 30]
          },
          {
            "name": "Realm of Alendith",
            "expected_uid": 937, 
            "vnum_range": [3500, 3799]
          }
        ]
      },
      "expected_output": {
        "all_areas_found": true,
        "valid_uids": true,
        "success": true
      }
    }
  ]
}
```

### Example 2: Function Testing

**JSON Configuration**:
```json
{
  "name": "string_validation",
  "test_type": "string_validator",
  "input": {
    "test_strings": [
      {"value": "ValidName", "should_pass": true},
      {"value": "Invalid Name", "should_pass": false},
      {"value": "toolongofanamethatshouldnotpass", "should_pass": false}
    ]
  },
  "expected_output": {
    "all_validations_correct": true
  }
}
```

**C Implementation**:
```c
static test_result_t test_string_validator(test_case_t *test) {
    json_t *input = json_object_get(test->config, "input");
    json_t *test_strings = json_object_get(input, "test_strings");
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_strings, index, test_case) {
        const char *value = json_get_string(test_case, "value");
        bool should_pass = json_get_bool(test_case, "should_pass");
        
        bool actual_result = validate_name(value);
        
        if (actual_result != should_pass) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "String validation failed for '%s': expected %s, got %s",
                          value, should_pass ? "pass" : "fail",
                          actual_result ? "pass" : "fail");
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}
```

### Example 3: Integration Test with Dependencies

```json
{
  "test_suite": "player_creation",
  "dependencies": ["area_loading", "database_integrity"],
  "tests": [
    {
      "name": "create_test_character", 
      "dependencies": [],
      "test_type": "character_creation",
      "input": {
        "character_name": "TestPlayer",
        "race": "human",
        "class": "warrior",
        "starting_area": "Limbo"
      },
      "expected_output": {
        "character_created": true,
        "placed_in_area": "Limbo",
        "has_starting_equipment": true
      }
    },
    {
      "name": "test_character_save",
      "dependencies": ["create_test_character"],
      "test_type": "character_persistence", 
      "input": {
        "character_name": "TestPlayer"
      },
      "expected_output": {
        "save_successful": true,
        "data_integrity": true
      }
    }
  ]
}
```

## Best Practices

### Test Organization

1. **Group Related Tests**: Keep related functionality in the same JSON file
2. **Use Dependencies**: Structure tests so complex scenarios build on simpler ones
3. **Clear Naming**: Use descriptive test names that explain what is being validated
4. **Document Purpose**: Include detailed descriptions for complex tests

### JSON Configuration

1. **Version Tracking**: Always include version numbers in test suites
2. **Realistic Data**: Use actual MUD data (area names, vnums) in test scenarios
3. **Complete Coverage**: Include both positive and negative test cases
4. **Error Conditions**: Test edge cases and error handling

### Code Implementation

1. **Input Validation**: Always validate JSON input structure
2. **Detailed Logging**: Log both success and failure conditions with context
3. **Resource Cleanup**: Ensure tests clean up any created resources
4. **Environment Checks**: Verify MUD environment state before testing

### Debugging

1. **Check Logs**: Full execution details are in `/sentience/logs/system.log`
2. **Console Output**: Real-time results appear on console during test runs
3. **Pattern Testing**: Use `-test:pattern` to run subset of tests for debugging
4. **Dependency Validation**: Framework reports dependency errors before execution

### Adding New Test Types

1. **Plan Structure**: Design JSON input/output schema before implementation
2. **Update Documentation**: Add examples to this documentation
3. **Consider Dependencies**: Determine what other tests should run first
4. **Test Your Tests**: Verify both success and failure paths work correctly

## Code Coverage Analysis

### Overview

The framework integrates with gcov/lcov for automated code coverage analysis, helping identify untested code paths and guide test development priorities.

### Installation and Setup

```bash
# Install coverage tools (most systems have gcov with GCC)
sudo apt-get install lcov

# Verify installation
gcov --version
lcov --version
```

### Build System Integration

#### CMake Coverage Support

Add coverage options to your build:

```bash
# Build with coverage analysis
cd /sentience/src
./build coverage

# Or manually configure
./build tests -DENABLE_COVERAGE=ON
```

#### Coverage Build Configuration

The build system automatically adds coverage flags:

```cmake
# Automatically configured when ENABLE_COVERAGE=ON
CFLAGS: --coverage -fprofile-arcs -ftest-coverage
LDFLAGS: --coverage
```

### Generating Coverage Reports

#### Quick Coverage Analysis

```bash
# Single command for complete coverage analysis
cd /sentience/src
./build coverage

# View HTML report
$BROWSER coverage_html/index.html
```

#### Manual Coverage Generation

```bash
# Build with coverage and run tests
cd /sentience/src
./build tests -DENABLE_COVERAGE=ON
cd /sentience
./sent -test

# Generate coverage data
cd /sentience/src
lcov --capture --directory .build --output-file coverage.info
lcov --remove coverage.info '/usr/*' '/sentience/src/.deps/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html

# View results
echo "Coverage report: file://$(pwd)/coverage_html/index.html"
```

### Coverage Integration with Tests

#### JSON Test Configuration

Tests can specify coverage requirements:

```json
{
  "test_suite": "string_utilities_coverage",
  "description": "String utility tests with coverage validation",
  "requires_mud_environment": false,
  "coverage_requirements": {
    "target_functions": ["is_name", "str_dup", "one_argument"],
    "minimum_line_coverage": 90,
    "minimum_function_coverage": 95
  },
  "tests": [
    {
      "name": "is_name_comprehensive",
      "test_type": "pure_function_test",
      "coverage_focus": ["is_name"],
      "input": {
        "test_cases": [
          {"name": "exact_match", "args": ["TestPlayer", "testplayer"], "expected": true},
          {"name": "partial_match", "args": ["Test", "testplayer test"], "expected": true},
          {"name": "no_match", "args": ["Other", "testplayer test"], "expected": false},
          {"name": "empty_string", "args": ["", "testplayer"], "expected": false},
          {"name": "null_handling", "args": [null, "testplayer"], "expected": false}
        ]
      },
      "expected_output": {
        "all_cases_pass": true,
        "coverage_achieved": true
      }
    }
  ]
}
```

#### Coverage-Driven Test Development

Use coverage analysis to identify gaps:

```bash
# Generate coverage for specific source files
lcov --extract coverage_filtered.info "*/src/act_*.c" --output-file commands_coverage.info
lcov --extract coverage_filtered.info "*/src/handler.c" --output-file handlers_coverage.info

# List uncovered functions
lcov --list coverage_filtered.info | grep "0.0%"

# Check coverage for specific file
lcov --list coverage_filtered.info | grep "handler.c"
```

### Coverage Analysis Examples

#### Function-Level Coverage Analysis

```bash
# Analyze coverage by function for critical files
genhtml coverage_filtered.info --output-directory coverage_html --show-details --function-coverage

# Generate coverage summary for specific areas
lcov --summary coverage_filtered.info
#  Summary coverage rate:
#    lines......: 78.5% (1547 of 1970 lines)
#    functions..: 82.1% (187 of 228 functions)
#    branches...: 68.9% (891 of 1294 branches)
```

#### Coverage for Critical Systems

```bash
# String utilities coverage
lcov --extract coverage_filtered.info "*/src/*string*.c" --output-file string_coverage.info
lcov --summary string_coverage.info

# Iterator system coverage
lcov --extract coverage_filtered.info "*/src/*iterator*.c" --output-file iterator_coverage.info
lcov --summary iterator_coverage.info

# Object management coverage
lcov --extract coverage_filtered.info "*/src/handler.c" "*/src/act_obj*.c" --output-file objects_coverage.info
lcov --summary objects_coverage.info
```

### Coverage Targets by System

#### Core Infrastructure (Target: 90%+ line coverage)
- String utilities (`is_name`, `str_dup`, `one_argument`, etc.)
- Iterator system (`iterator_start`, `iterator_nextdata`, `iterator_stop`)
- Object getters (`get_obj_carry_number`, `get_obj_here`, etc.)
- Lookup functions (`skill_lookup`, `flag_value`, `item_lookup`)

#### Command System (Target: 70%+ line coverage)
- Information commands (`do_look`, `do_inventory`, `do_score`)
- Object manipulation (`do_get`, `do_drop`, `do_give`)
- Character interaction commands

#### Integration Systems (Target: 60%+ line coverage)
- Area loading and validation
- Database integrity and persistence
- Network protocol handling

### CI/CD Coverage Integration

#### Automated Coverage in CI

```yaml
name: Test Coverage Analysis
on: [push, pull_request]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Install coverage tools
      run: sudo apt-get install -y lcov
      
    - name: Build with coverage
      run: |
        cd src
        ./build coverage
        
    - name: Upload coverage reports
      uses: codecov/codecov-action@v3
      with:
        file: ./src/coverage_filtered.info
        flags: unittests,integration
        name: sentience-mud-coverage
        
    - name: Coverage comment
      uses: 5monkeys/cobertura-action@master
      with:
        path: ./src/coverage_filtered.info
        repo_token: ${{ secrets.GITHUB_TOKEN }}
        minimum_coverage: 75
```

#### Coverage Regression Detection

```bash
# Compare coverage between branches
git checkout main
cd /sentience/src && ./build coverage
mv coverage_filtered.info ../main_coverage.info

git checkout feature/new-tests  
cd /sentience/src && ./build coverage

# Generate coverage diff
lcov --diff ../main_coverage.info coverage_filtered.info --output-file coverage_diff.info
genhtml coverage_diff.info --output-directory coverage_diff_html
```

### Advanced Coverage Analysis

#### Branch Coverage Analysis

```bash
# Enable branch coverage (requires GCC 4.6+)
./build tests -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage -fprofile-generate"

# Generate report with branch coverage
genhtml coverage_filtered.info --output-directory coverage_html --branch-coverage
```

#### Memory Testing Combined with Coverage

```bash
# Run tests with both memory testing and coverage
cd /sentience/src
./build tests -DENABLE_ASAN=ON -DENABLE_COVERAGE=ON
cd /sentience
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./sent -test

# Generate combined report
cd /sentience/src
lcov --capture --directory .build --output-file coverage.info
genhtml coverage.info --output-directory coverage_html_with_asan
```

### Coverage-Guided Testing Workflow

#### 1. Generate Baseline Coverage
```bash
cd /sentience/src
./build coverage
echo "Baseline: $(lcov --summary coverage_filtered.info | grep lines | awk '{print $2}')"
```

#### 2. Identify Coverage Gaps
```bash
# Find uncovered functions
lcov --list coverage_filtered.info | grep "0.0%" > uncovered_functions.txt

# Focus on critical uncovered functions
grep -E "(is_name|str_dup|get_obj|iterator)" uncovered_functions.txt
```

#### 3. Write Targeted Tests
```bash
# Create unit tests for uncovered functions
# Add to appropriate JSON test files in /sentience/src/tests/data/unit/
```

#### 4. Verify Coverage Improvement
```bash
cd /sentience/src
./build coverage
echo "New coverage: $(lcov --summary coverage_filtered.info | grep lines | awk '{print $2}')"
```

## Memory Testing and Analysis

### Memory Leak Detection

For a C-based MUD, memory management is critical. The framework supports multiple memory testing approaches:

#### AddressSanitizer (Recommended for Development)

Build with AddressSanitizer for fast memory error detection:

```bash
# Build with AddressSanitizer
cd /sentience/src
./build tests -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"

# Run tests with leak detection
cd /sentience
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./sent -test
```

**Advantages:**
- Fast execution (2-3x slower than normal)
- Catches use-after-free, buffer overflows, double-free
- Excellent error reporting with stack traces
- Integrates well with existing test framework

#### Valgrind (Recommended for CI/Comprehensive Testing)

Run tests under Valgrind for thorough memory analysis:

```bash
# Run tests under Valgrind
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --error-exitcode=1 \
         ./sent -test
```

**Advantages:**
- No recompilation needed
- Extremely thorough leak detection
- Tracks uninitialized memory usage

**Disadvantages:**
- Very slow (10-50x slower)
- Best suited for CI rather than development

### Memory Testing Strategy

Use a layered approach for different scenarios:

1. **Development Testing**: AddressSanitizer for fast feedback
2. **CI Testing**: Both AddressSanitizer and Valgrind
3. **Release Testing**: Comprehensive Valgrind analysis

### Build System Integration

Add memory testing options to CMake:

```cmake
# Memory testing options in CMakeLists.txt
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_VALGRIND "Enable Valgrind testing" OFF)

if(ENABLE_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# Custom target for memory testing
add_custom_target(memory-test
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} --build . -- -DENABLE_ASAN=ON
    COMMAND cd /sentience && ASAN_OPTIONS=detect_leaks=1 ./sent -test
    COMMENT "Running tests with memory leak detection"
)
```

### Critical Memory Testing Areas

Focus memory testing on these MUD-specific areas:

1. **String Handling**: `str_dup()`, `free_string()`, `alloc_string()`
2. **Object Management**: `new_obj()`, `free_obj()`, `extract_obj()`
3. **Character Data**: `new_char()`, `free_char()`, player loading/saving
4. **Iterators**: LLIST operations, `iterator_start()/stop()`
5. **Buffer Management**: `new_buf()`, `free_buf()`, `buf_string()`
6. **Area Loading**: JSON parsing, area data allocation/cleanup

## Testing Strategy and Priorities

### Testing Pyramid for MUD Development

The framework supports different levels of testing complexity:

#### Level 1: Pure Functions (High ROI, Easy Implementation)

Test functions with no side effects or global state dependencies:
- String utilities: `is_name()`, `one_argument()`, `smash_tilde()`
- Math/calculations: Damage formulas, experience calculations
- Lookup functions: `skill_lookup()`, `flag_value()`, `item_lookup()`
- Validators: `is_valid_name()`, `parse_gen_name()`

```json
{
  "name": "string_utilities_test",
  "test_type": "string_validator",
  "input": {
    "test_cases": [
      {"input": "validName", "expected": true},
      {"input": "invalid name", "expected": false}
    ]
  },
  "expected_output": {
    "all_validations_correct": true
  }
}
```

#### Level 2: Handler Functions (Medium Complexity)

Test functions that manipulate game objects with minimal state:
- Object getters: `get_obj_carry_number()`, `get_obj_here()`
- Character getters: `get_char_room()`, `get_char_world()`
- List operations: Iterator functions, LLIST operations

#### Level 3: Integration Tests (Current Framework Focus)

Test complete workflows with real game state:
- Area loading and validation
- WNUM parsing with area context
- Database integrity checks
- Character creation and persistence

#### Level 4: End-to-End Testing (Future Expansion)

Test complete user scenarios:
- Player login/logout flows
- Command execution workflows
- Combat systems
- Complex game mechanics

### MUD-Specific Testing Challenges

The framework addresses these unique challenges:

#### State Management
- **Global State**: Areas, objects, characters loaded in memory
- **Persistent State**: Player files, area files, configuration
- **Runtime State**: Timers, events, combat rounds
- **Network State**: Descriptors, connections, buffers

**Framework Solutions:**
- `requires_mud_environment` flag for tests needing full state
- Dependency management ensures proper initialization order
- JSON configuration allows flexible state setup

#### Dependencies
- **Database/File I/O**: Player accounts, character data, areas
- **External Services**: Redis cache, logging (zlog)
- **Time-Dependent**: Combat ticks, regeneration, events

**Framework Solutions:**
- Dependency validation ensures prerequisites are met
- Test environment readiness checks
- Structured input/output for predictable test scenarios

## Advanced Testing Patterns

### Testing Memory-Intensive Operations

For tests that allocate significant memory:

```c
static test_result_t test_area_loading_memory(test_case_t *test) {
    // Track memory before test
    size_t mem_before = get_memory_usage();
    
    // Perform memory-intensive operation
    json_t *input = json_object_get(test->config, "input");
    const char *area_file = json_get_string(input, "area_file");
    
    AREA_DATA *area = load_area(area_file);
    if (!area) {
        return TEST_FAILURE;
    }
    
    // Verify area loaded correctly
    bool area_valid = validate_area_integrity(area);
    
    // Clean up
    unload_area(area);
    
    // Verify memory cleanup
    size_t mem_after = get_memory_usage();
    if (mem_after > mem_before + acceptable_overhead) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Memory leak detected: %zu bytes", 
                      mem_after - mem_before);
        return TEST_FAILURE;
    }
    
    return area_valid ? TEST_SUCCESS : TEST_FAILURE;
}
```

### Testing Error Conditions

Test both success and failure scenarios:

```json
{
  "name": "error_handling_test",
  "test_type": "error_validator",
  "input": {
    "test_cases": [
      {
        "scenario": "valid_input",
        "data": "valid_data",
        "should_succeed": true
      },
      {
        "scenario": "invalid_input",
        "data": "corrupted_data",
        "should_succeed": false,
        "expected_error": "PARSE_ERROR"
      },
      {
        "scenario": "null_input",
        "data": null,
        "should_succeed": false,
        "expected_error": "NULL_POINTER"
      }
    ]
  },
  "expected_output": {
    "error_handling_correct": true
  }
}
```

## Troubleshooting

### Common Issues

**Tests not running**:
- Ensure BUILD_TESTS was used during compilation
- Check that JSON files are valid and in `/sentience/src/tests/data/`
- Verify MUD environment initializes properly

**Console output missing**:
- Check `/sentience/data/system/zlog.conf` configuration
- Verify debug output rules are enabled
- Look for output in `/sentience/logs/system.log`

**JSON parsing errors**:
- Validate JSON syntax with tools like `jq`
- Check that required fields are present
- Ensure data types match expectations (string, number, boolean, array)

**Dependency failures**:
- Verify referenced test suites and cases exist
- Check for circular dependencies
- Ensure dependency order makes logical sense

**Memory testing issues**:
- Rebuild with memory testing flags when using AddressSanitizer
- Check that Valgrind is available for comprehensive testing
- Verify memory testing tools are properly installed

**Coverage analysis issues**:
- Ensure gcov and lcov are installed (`sudo apt-get install lcov`)
- Rebuild with coverage flags (`./build coverage` or `-DENABLE_COVERAGE=ON`)
- Check that coverage data files (.gcda, .gcno) are generated in build directory
- Verify lcov can access build directory and source files

### Debugging Commands

```bash
# Validate JSON syntax
jq '.' /sentience/src/tests/data/my_tests.json

# Run specific test suite
./sent -test:my_test_suite

# Check test logs
tail -f /sentience/logs/system.log | grep -E "(TEST|debug)"

# Build with verbose output
./build tests | tee build.log

# Memory testing with AddressSanitizer
cd /sentience/src
./build tests -DCMAKE_C_FLAGS="-fsanitize=address"
cd /sentience
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./sent -test

# Memory testing with Valgrind
valgrind --tool=memcheck --leak-check=full ./sent -test

# Check memory usage during tests
ps aux | grep sent
top -p $(pgrep sent)

# Coverage analysis commands
./build coverage                        # Build and generate coverage report
lcov --list coverage_filtered.info     # List coverage by file
lcov --summary coverage_filtered.info  # Coverage summary statistics
genhtml coverage_filtered.info --output-directory coverage_html  # Generate HTML report

# Check coverage for specific files
lcov --extract coverage_filtered.info "*/src/handler.c" --output-file handler_coverage.info
lcov --summary handler_coverage.info

# Find uncovered functions
lcov --list coverage_filtered.info | grep "0.0%"
```