# Testing Framework Architecture

This document provides the comprehensive reference for the Sentience MUD testing framework. The framework is a JSON-driven, hierarchical test system supporting both unit and integration tests with configurable execution profiles.

## 1. Architecture Overview

The testing framework follows a modular pipeline architecture:

```
JSON Test Files → test_loader.c → test_suite_t → test_dispatcher.c → Handler Functions → test_result_t
      ↓                ↓               ↓              ↓                    ↓               ↓
test suite JSON → parse & create → linked list → dispatch table → C handlers → results
```

**Data Flow:**
1. **Test Discovery**: `test_loader.c` recursively scans test data directories for `.json` files
2. **JSON Parsing**: Creates `test_suite_t` and `test_case_t` structures from JSON definitions
3. **Test Registry**: Builds linked list of loaded test suites in global `test_suites`
4. **Test Execution**: `test_dispatcher.c` matches test types to handler functions via dispatch table
5. **Result Collection**: Handlers return `test_result_t` values, framework aggregates statistics

**Core Components:**
- **test_framework.h/c**: Core data structures, initialization, and utility functions
- **test_dispatcher.c**: Handler dispatch table and test execution engine
- **test_loader.c**: JSON file loading and test suite creation
- **test_modules.h**: Handler function declarations and dispatch types
- **test_utils.h/c**: Fake player utilities and test helpers

## 2. Test Hierarchy

### test_suite_t Structure
Represents a collection of related tests loaded from a single JSON file.

```c
typedef struct test_suite {
    char *name;                    // Suite identifier (from "test_suite" field)
    char *description;             // Human-readable description
    char *version;                 // Version string (default: "1.0")
    bool requires_mud_environment; // Whether MUD state must be initialized
    int timeout_seconds;           // Default timeout for all tests in suite
    char **dependencies;           // Array of suite names this depends on
    int dependency_count;          // Number of dependencies
    test_case_t *tests;           // Linked list of test cases
    struct test_suite *next;       // Next suite in global list
} test_suite_t;
```

### test_case_t Structure
Individual test within a suite.

```c
typedef struct test_case {
    char *name;                   // Test identifier
    char *description;            // Test description
    char *test_type;              // Determines which handler to use
    char **dependencies;          // Array of test names this depends on
    int dependency_count;         // Number of dependencies
    bool verbose_output;          // Per-test verbose setting
    int timeout_seconds;          // Per-test timeout override
    json_t *config;              // Original JSON config (input/expected_output)
    test_result_t (*execute)(struct test_case *); // Function pointer (usually NULL)
    struct test_case *next;       // Next test in suite
} test_case_t;
```

### test_config_t Structure
Global configuration loaded from `test_config.json`.

```c
typedef struct test_config {
    char *version;                      // Config version
    char *description;                  // Config description
    char **default_test_suites;         // Default suite list
    int default_suite_count;
    char **quick_test_suites;           // Quick test profile
    int quick_suite_count;
    char **full_test_suites;            // Full test profile
    int full_suite_count;
    char **unit_only_suites;            // Unit tests only
    int unit_only_count;
    char **integration_only_suites;     // Integration tests only
    int integration_only_count;
    char **exclude_patterns;            // Test exclusion patterns
    int exclude_pattern_count;
    int default_timeout_seconds;       // Global default timeout
    bool stop_on_first_failure;        // Halt on first error
    bool run_unit_tests_first;          // Unit before integration
    bool require_mud_environment_for_integration; // MUD state requirement
    bool verbose_output;                // Global verbosity
    bool verbose_test_names;            // Show test names
    bool verbose_test_details;          // Show test details
    bool show_test_config;              // Display configuration
    bool show_execution_time;           // Show timing information
    char *log_output_file;              // Log file path
    char *log_output_format;            // "text" or "json"
    test_profile_t *profiles;           // Named test profiles
    int profile_count;
} test_config_t;
```

### test_profile_t Structure
Named test execution profiles.

```c
typedef struct test_profile {
    char *name;                   // Profile name (e.g., "development", "ci")
    char *description;            // Profile description
    char **test_suites;           // Suite list for this profile
    int suite_count;
    bool stop_on_first_failure;   // Profile-specific failure behavior
    bool require_coverage;        // Coverage requirement
} test_profile_t;
```

## 3. JSON Test Schema

### Test Suite JSON Structure
Each test suite is defined in a `.json` file with this schema:

```json
{
  "type": "test_suite",
  "test_suite": "suite_name",
  "description": "Human-readable description",
  "version": "1.0",
  "requires_mud_environment": true,
  "dependencies": ["other_suite"],
  "timeout_seconds": 300,
  "tests": [
    {
      "name": "test_name",
      "description": "Test description", 
      "test_type": "handler_type",
      "dependencies": ["other_test"],
      "verbose_output": false,
      "timeout_seconds": 60,
      "input": {
        // Test-specific input data
      },
      "expected_output": {
        "success": true,
        // Expected results
      }
    }
  ]
}
```

**Required Fields:**
- `type`: Must be `"test_suite"`
- `test_suite`: Unique suite identifier
- `tests`: Array of test case objects
- `tests[].name`: Unique test identifier within suite
- `tests[].test_type`: Determines handler function

**Optional Fields:**
- `description`: Suite description (default: "")
- `version`: Version string (default: "1.0")
- `requires_mud_environment`: MUD state requirement (default: false)
- `dependencies`: Suite dependency array (default: [])
- `timeout_seconds`: Default timeout (default: from config or 300)
- `tests[].description`: Test description (default: "")
- `tests[].dependencies`: Test dependency array (default: [])
- `tests[].verbose_output`: Per-test verbosity (default: from config)
- `tests[].timeout_seconds`: Per-test timeout (default: suite or config default)
- `tests[].input`: Test input data (default: null)
- `tests[].expected_output`: Expected results (default: null)

## 4. Handler Dispatch Table

The dispatch table in `test_dispatcher.c` maps test types to handler functions:

| Pattern | Handler Function | Match Mode | Description |
|---------|------------------|------------|-------------|
| `pure_function_test` | `run_pure_function_test_case` | EXACT | Pure function unit tests |
| `buffer_function_test` | `run_buffer_function_test_case` | EXACT | Buffer operation unit tests |
| `reset_cross_area_creation` | `run_reset_test_case` | EXACT | Reset system area creation |
| `reset_serialization` | `run_reset_test_case` | EXACT | Reset data serialization |
| `reset_legacy_vnum` | `run_reset_test_case` | EXACT | Reset legacy vnum handling |
| `shop_stock_cross_area_creation` | `run_shop_stock_test_case` | EXACT | Shop stock area creation |
| `shop_stock_serialization` | `run_shop_stock_test_case` | EXACT | Shop stock serialization |
| `shop_stock_legacy_vnum` | `run_shop_stock_test_case` | EXACT | Shop stock legacy vnums |
| `shop_stock_reference_integrity` | `run_shop_stock_test_case` | EXACT | Shop stock references |
| `damage_class_lookup_test` | `run_lookup_table_test_case` | EXACT | Damage class lookups |
| `reserved_lookup_test` | `run_wnum_test_case` | EXACT | Reserved number lookups |
| `reserved_wnum_format_test` | `run_wnum_test_case` | EXACT | Reserved wnum formatting |
| `reserved_compat_test` | `run_wnum_test_case` | EXACT | Reserved number compatibility |
| `string_editor_` | `run_string_editor_test_case` | SUBSTR | String editor tests |
| `church_` | `run_church_test_case` | SUBSTR | Church system tests |
| `instance_` | `run_instance_test_case` | SUBSTR | Instance system tests |
| `blueprint_` | `run_instance_test_case` | SUBSTR | Blueprint system tests |
| `dungeon_` | `run_instance_test_case` | SUBSTR | Dungeon system tests |
| `ship_` | `run_instance_test_case` | SUBSTR | Ship system tests |
| `wnum_json_` | `run_instance_test_case` | SUBSTR | WNUM JSON tests |
| `persist_directory` | `run_instance_test_case` | SUBSTR | Persistence directory tests |
| `chat_room_` | `run_chat_room_test_case` | SUBSTR | Chat room tests |
| `skill_group_` | `run_skill_group_test_case` | SUBSTR | Skill group tests |
| `skill_` | `run_skill_data_test_case` | SUBSTR | Skill data tests |
| `spell_fun_` | `run_skill_data_test_case` | SUBSTR | Spell function tests |
| `class_` | `run_class_data_test_case` | SUBSTR | Class data tests |
| `item_type_` | `run_item_type_test_case` | SUBSTR | Item type tests |
| `song_` | `run_song_data_test_case` | SUBSTR | Song data tests |
| `trait_` | `run_trait_system_test_case` | SUBSTR | Trait system tests |
| `script_engine_` | `run_script_engine_test_case` | SUBSTR | Script engine tests |
| `channel_` | `run_channel_pubsub_test_case` | SUBSTR | Channel pub/sub tests |
| `combat_` | `run_combat_telemetry_test_case` | SUBSTR | Combat telemetry tests |
| `_lookup_test` | `run_lookup_table_test_case` | SUBSTR | Generic lookup tests |
| `flag_table_` | `run_lookup_table_test_case` | SUBSTR | Flag table tests |

**Match Modes:**
- `MATCH_EXACT`: `test_type` must exactly match pattern
- `MATCH_SUBSTR`: `test_type` must contain pattern as substring

**Fallback Behavior:** Unmatched test types are dispatched to `run_wnum_test_case` for legacy compatibility.

## 5. Configuration

### Configuration Files
Test configuration is loaded from these sources in priority order:

1. `$SENTIENCE_TEST_CONFIG` environment variable path
2. `{test_data_dir}/test_config.json`
3. Built-in defaults

### Configuration Structure
The `test_config.json` file contains:

```json
{
  "type": "test_configuration",
  "test_configuration": {
    "version": "1.0",
    "description": "Configuration description",
    "default_test_suites": ["suite1", "suite2"],
    "quick_test_suites": ["suite1"],
    "full_test_suites": ["suite1", "suite2", "suite3"],
    "unit_only_suites": ["unit_suite"],
    "integration_only_suites": ["integration_suite"],
    "exclude_patterns": ["pattern1", "pattern2"],
    "settings": {
      "default_timeout_seconds": 300,
      "stop_on_first_failure": false,
      "run_unit_tests_first": true,
      "require_mud_environment_for_integration": true,
      "verbose_output": false,
      "verbose_test_names": true,
      "verbose_test_details": false,
      "show_test_config": true,
      "show_execution_time": true,
      "log_output_file": "",
      "log_output_format": "text"
    },
    "test_profiles": {
      "profile_name": {
        "description": "Profile description",
        "suites": ["suite1", "suite2"],
        "stop_on_first_failure": true,
        "require_coverage": false
      }
    }
  }
}
```

### Environment Variables
The framework recognizes these environment variables:

- `SENTIENCE_TEST_DATA_DIR`: Override test data directory
- `SENTIENCE_TEST_CONFIG`: Override config file path
- `SENTIENCE_TEST_LOG_FILE`: Override log output file
- `SENTIENCE_TEST_LOG_FORMAT`: Override log format ("text" or "json")

## 6. Assertion Macros

The framework provides comprehensive assertion macros for test handlers:

| Macro | Usage | Description |
|-------|-------|-------------|
| `TEST_ASSERT_TRUE(expr)` | `TEST_ASSERT_TRUE(condition)` | Assert expression is true |
| `TEST_ASSERT_FALSE(expr)` | `TEST_ASSERT_FALSE(condition)` | Assert expression is false |
| `TEST_ASSERT_INT_EQ(expected, actual)` | `TEST_ASSERT_INT_EQ(5, result)` | Assert integers are equal |
| `TEST_ASSERT_INT_NEQ(expected, actual)` | `TEST_ASSERT_INT_NEQ(0, count)` | Assert integers are not equal |
| `TEST_ASSERT_INT_GT(val, threshold)` | `TEST_ASSERT_INT_GT(count, 0)` | Assert value > threshold |
| `TEST_ASSERT_INT_GTE(val, threshold)` | `TEST_ASSERT_INT_GTE(count, 1)` | Assert value >= threshold |
| `TEST_ASSERT_STR_EQ(expected, actual)` | `TEST_ASSERT_STR_EQ("hello", str)` | Assert strings are equal |
| `TEST_ASSERT_STR_NEQ(expected, actual)` | `TEST_ASSERT_STR_NEQ("", name)` | Assert strings are not equal |
| `TEST_ASSERT_NULL(ptr)` | `TEST_ASSERT_NULL(result)` | Assert pointer is NULL |
| `TEST_ASSERT_NOT_NULL(ptr)` | `TEST_ASSERT_NOT_NULL(obj)` | Assert pointer is not NULL |

**Behavior:** All assertion macros log descriptive failure messages and return `TEST_FAILURE` on assertion failure. Use within functions that return `test_result_t`.

## 7. Test Utilities

### JSON Helper Functions
Convenience macros for accessing test configuration JSON:

```c
#define test_json_get_string(obj, key) json_get_string((obj), (key), NULL)
#define test_json_get_int(obj, key)    ((int)json_get_int((obj), (key), 0))
#define test_json_get_bool(obj, key)   json_get_bool((obj), (key), false)
```

### Fake Player Utilities
Create isolated test players for integration testing:

```c
bool test_utils_create_fake_player(CHAR_DATA **out_ch, DESCRIPTOR_DATA **out_desc);
void test_utils_destroy_fake_player(CHAR_DATA *ch, DESCRIPTOR_DATA *desc);
```

**`test_utils_create_fake_player`:**
- Creates a new `CHAR_DATA` with `pcdata` allocated
- Creates a new `DESCRIPTOR_DATA` with protocol layer initialized
- Links character and descriptor bidirectionally
- Sets descriptor state to `CON_PLAYING`
- Returns `true` on success, `false` on failure

**`test_utils_destroy_fake_player`:**
- Safely unlinks character and descriptor
- Destroys protocol layer
- Frees all allocated memory
- Handles NULL pointers gracefully

### Environment Check Functions
```c
bool test_environment_ready(void);
```

Verifies that the MUD environment is properly initialized for integration tests.

## 8. Logging

### Console Logging
All test output uses the `LOG_UNIT_TESTS` logging category with standard log levels:

- `LOG_LEVEL_INFO`: Normal test progress and results
- `LOG_LEVEL_WARN`: Non-fatal issues (config not found, etc.)
- `LOG_LEVEL_ERROR`: Test failures and framework errors

### File Logging
Configurable file logging supports two formats:

**Text Format** (default):
```
[2024-01-15T10:30:45-0800] event=session_start
[2024-01-15T10:30:45-0800] event=suite_start payload=suite_name
[2024-01-15T10:30:46-0800] event=test_result payload=test_name result=PASS elapsed=0.123
```

**JSON Format**:
```json
{"ts":"2024-01-15T10:30:45-0800","event":"session_start","format":"json"}
{"ts":"2024-01-15T10:30:45-0800","event":"suite_start","suite":"suite_name"}
{"ts":"2024-01-15T10:30:46-0800","event":"test_result","test":"test_name","result":"PASS","elapsed":0.123}
```

### Verbosity Settings
Configuration controls output detail levels:

- `verbose_output`: Global verbosity toggle
- `verbose_test_names`: Show individual test names as they run
- `verbose_test_details`: Show test descriptions, timeouts, and dependencies
- `show_execution_time`: Include timing information in output
- `show_test_config`: Display configuration at startup

## 9. Build Integration

### CMake Integration
The framework is conditionally compiled with the `BUILD_TESTS` preprocessor definition:

```cmake
# Configure tests (CMake)
./config
./compile
./install

# Run tests
cd /sentience
./sent --test-pattern=area_loading
```

### Makefile Integration
Traditional make also supports test compilation:

```bash
# Build with tests (Make)
make

# Run tests
./sent --test-pattern=quick
```

**Conditional Compilation:** All test framework code is wrapped in `#ifdef BUILD_TESTS` guards, ensuring zero overhead in production builds.

## 10. Path Resolution

The test framework uses a fallback strategy to locate test data:

### Test Data Directory Resolution
1. `$SENTIENCE_TEST_DATA_DIR` environment variable
2. `data/tests` (relative to working directory)
3. `src/tests/data` (relative to working directory)
4. `/sentience/src/tests/data` (absolute bootstrap path)

### Test Configuration Resolution
1. `$SENTIENCE_TEST_CONFIG` environment variable
2. `{test_data_dir}/test_config.json`
3. Built-in defaults (if config loading fails)

### Bootstrap Root Compatibility
The framework detects bootstrap root environments and adjusts paths accordingly:

- Test data directory resolution includes bootstrap-aware paths
- Configuration resolution adapts to bootstrap directory structure
- Path resolution logged for debugging

## 11. Multi-Cycle Testing

The framework supports multi-cycle testing for persistence verification:

### Test Cycle Scripts
External scripts can run multiple test cycles with MUD process restarts:

1. Start MUD server process
2. Run serialization-focused test profile
3. Shutdown MUD server cleanly
4. Restart MUD server (loads persisted data)
5. Run verification test profile
6. Compare results across cycles

### Persistence-Focused Profiles
The `serialization_cycle` profile includes tests specifically designed for multi-cycle validation:

- JSON area serialization
- Church persistence
- Chat room persistence
- Instance persistence
- Shop stock persistence
- Reset persistence
- WNUM persistence

## 12. Result Codes

### test_result_t Enumeration
All test handlers return one of these result codes:

```c
typedef enum {
    TEST_SUCCESS = 0,  // Test passed successfully
    TEST_FAILURE = 1,  // Test failed (assertion failed, wrong result)
    TEST_ERROR = 2,    // Test encountered an error (exception, crash, etc.)
    TEST_SKIP = 3      // Test was skipped (dependency not met, etc.)
} test_result_t;
```

**Result Semantics:**
- `TEST_SUCCESS`: Test completed and all assertions passed
- `TEST_FAILURE`: Test completed but assertions failed or results were incorrect
- `TEST_ERROR`: Test could not complete due to framework or environment errors
- `TEST_SKIP`: Test was intentionally skipped due to unmet dependencies or conditions

### test_stats_t Aggregation
Test results are aggregated into statistics:

```c
typedef struct {
    int total;    // Total number of tests executed
    int passed;   // Number of successful tests
    int failed;   // Number of failed tests
    int errors;   // Number of error tests
    int skipped;  // Number of skipped tests
} test_stats_t;
```

---

## Framework Entry Points

**Initialization:**
```c
void init_test_framework(void);
void cleanup_test_framework(void);
```

**Test Execution:**
```c
test_stats_t run_all_tests(void);
test_stats_t run_tests_by_pattern(const char *pattern);
test_stats_t run_specific_tests(char **test_names, int count);
```

**Test Discovery:**
```c
bool load_all_test_suites(const char *test_data_dir);
test_suite_t *find_test_suite(const char *name);
test_case_t *find_test_case_global(const char *test_name);
```

This framework provides comprehensive testing capabilities for both unit and integration testing scenarios while maintaining clear separation between test code and production code through conditional compilation.