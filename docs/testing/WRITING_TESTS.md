# Writing Tests for Sentience MUD

This guide covers how to write tests in the Sentience MUD codebase using Test-Driven Development (TDD).

## 1. The TDD Cycle for Sentience

The TDD workflow follows this pattern:

1. **Write test JSON first** — Create or add test cases in JSON format
2. **Build** — Compile with `./build` (tests enabled by default) 
3. **See it fail** — Run tests and verify your new test fails as expected
4. **Write/modify C handler** — Implement the code to make the test pass
5. **Build** — Compile again
6. **See it pass** — Verify your test now passes
7. **Commit** — Save your working changes

This ensures you write only the code needed and have test coverage from the start.

## 2. Adding a Test to an Existing Type

**Simplest case:** Add a new test case to an existing JSON test suite.

### Example: Adding a test to `core_string_unit_tests.json`

1. **Edit the JSON file:**
```bash
# Edit the existing test suite
vim tests/data/unit/core_string_unit_tests.json
```

2. **Add your test case to the `tests` array:**
```json
{
  "name": "str_prefix_edge_cases",
  "description": "Test edge cases for historical not-prefix semantics",
  "test_type": "pure_function_test",
  "input": {
    "function": "str_prefix",
    "test_cases": [
      { "astr": "", "bstr": "hello", "expected_not_prefix": true },
      { "astr": "hello", "bstr": "", "expected_not_prefix": true }
    ]
  },
  "expected_output": { "all_cases_pass": true }
}
```

3. **Build and test:**
```bash
./build
./sent -test unit_only
```

4. **No C code changes needed** — The existing handler processes the new test cases automatically.

## 3. Adding a New Test Type

**Full walkthrough:** Create a completely new test type with its own JSON suite and C handler.

### Step A: Create JSON test suite file

Create in `tests/data/unit/` (for unit tests) or `tests/data/integration/` (for integration tests):

```bash
# Example: tests/data/unit/my_feature_unit_tests.json
```

### Step B: Create C handler file

Create in `tests/unit/` or `tests/integration/`:

```bash
# Example: tests/unit/my_feature_tests.c
```

### Step C: Add handler declaration

Add to `tests/framework/test_modules.h`:

```c
test_result_t run_my_feature_test_case(test_case_t *test);
```

### Step D: Add entry to handler table

Add to `handler_table[]` in `tests/framework/test_dispatcher.c`:

```c
/* Add before the sentinel { NULL, NULL, MATCH_EXACT } */
{ "my_feature_test", run_my_feature_test_case, MATCH_EXACT },
```

### Step E: Add source file to build systems

**CMakeLists.txt** (in the `if(BUILD_TESTS)` block):
```cmake
tests/unit/my_feature_tests.c
```

**Makefile** (in the `ifdef BUILD_TESTS` block):
```make
tests/unit/my_feature_tests.c \
```

### Step F: Add suite name to test profiles

Add to appropriate profiles in `tests/data/test_config.json`:

```json
"development": {
  "suites": ["my_feature_unit_tests", ...]
}
```

### Step G: Build and run

```bash
./build
./sent -test development
```

## 4. Copy-Paste Templates

### Unit Test JSON Template

Based on `core_string_unit_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "my_feature_unit_tests",
  "description": "Unit tests for my feature functionality",
  "version": "1.0",
  "requires_mud_environment": false,
  "test_level": "unit",
  "tests": [
    {
      "name": "basic_functionality",
      "description": "Test basic feature operation",
      "test_type": "pure_function_test",
      "input": {
        "function": "my_function",
        "test_cases": [
          { "arg": "input1", "expected": "output1" },
          { "arg": "input2", "expected": "output2" }
        ]
      },
      "expected_output": { "all_cases_pass": true }
    }
  ]
}
```

### Unit Test C Handler Template

Based on `pure_function_tests.c`:

```c
/**
 * My Feature Tests
 * 
 * Tests for my feature including:
 * - Basic functionality
 * - Edge cases
 * - Error handling
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_my_function(test_case_t *test);

/**
 * Main test dispatcher for my feature tests
 */
test_result_t run_my_feature_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "my_function_test") == 0) {
        result = test_my_function(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown my feature test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test my_function with various inputs
 */
static test_result_t test_my_function(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *arg = test_json_get_string(tc, "arg");
        const char *expected = test_json_get_string(tc, "expected");

        if (!arg || !expected) continue;

        const char *result = my_function(arg);

        TEST_ASSERT_STR_EQ(expected, result);
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
```

### Integration Test JSON Template

Based on `combat_telemetry_tests.json`:

```json
{
  "type": "test_suite",
  "test_suite": "my_integration_tests",
  "description": "Integration tests for my feature",
  "version": "1.0",
  "requires_mud_environment": true,
  "dependencies": [],
  "timeout_seconds": 30,
  "tests": [
    {
      "name": "feature_integration_test",
      "description": "Test feature in full MUD environment",
      "dependencies": [],
      "test_type": "my_integration_test",
      "input": {
        "test_data": "some_value",
        "fixture_area_name": "Bootstrap Tests",
        "fixture_room_vnum": 2
      },
      "expected_output": {
        "success": true
      }
    }
  ]
}
```

### Integration Test C Handler Template

Based on `trait_system_tests.c`:

```c
/**
 * My Integration Tests
 * 
 * Tests my feature integration including:
 * - Full system interaction
 * - Data persistence
 * - Error conditions
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_my_integration(test_case_t *test);

/**
 * Main test dispatcher for my integration tests
 */
test_result_t run_my_integration_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "my_integration_test") == 0) {
        result = test_my_integration(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown my integration test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test feature integration in MUD environment
 */
static test_result_t test_my_integration(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    const char *test_data = test_json_get_string(input, "test_data");
    const char *area_name = test_json_get_string(input, "fixture_area_name");
    long room_vnum = test_json_get_int(input, "fixture_room_vnum");

    if (!test_data || !area_name || room_vnum <= 0) {
        return TEST_ERROR;
    }

    /* Find the test fixture area and room */
    AREA_DATA *area = find_area((char *)area_name);
    TEST_ASSERT_NOT_NULL(area);

    ROOM_INDEX_DATA *room = get_room_index(room_vnum);
    TEST_ASSERT_NOT_NULL(room);

    /* Run your integration test logic here */
    bool result = my_feature_integration_function(test_data, room);
    TEST_ASSERT_TRUE(result);

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
```

## 5. Using Assertion Macros

The test framework provides 10 assertion macros in `tests/framework/test_framework.h`:

```c
/* Boolean assertions */
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);

/* Integer comparisons */
TEST_ASSERT_INT_EQ(expected, actual);
TEST_ASSERT_INT_NEQ(unexpected, actual);
TEST_ASSERT_INT_GT(value, threshold);
TEST_ASSERT_INT_GTE(value, threshold);

/* String comparisons */
TEST_ASSERT_STR_EQ(expected, actual);
TEST_ASSERT_STR_NEQ(unexpected, actual);

/* Pointer checks */
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);
```

### Example Usage

```c
/* Test a lookup function */
int result = position_lookup("standing");
TEST_ASSERT_INT_GT(result, -1);  /* Valid position */

/* Test string transformation */
char buffer[256];
strcpy(buffer, "test~string");
smash_tilde(buffer);
TEST_ASSERT_STR_EQ("test-string", buffer);

/* Test data integrity */
TRAIT_DEF *def = trait_def_lookup("strength");
TEST_ASSERT_NOT_NULL(def);
TEST_ASSERT_STR_EQ("strength", def->id);
TEST_ASSERT_INT_EQ(TRAIT_INTEGER, def->type);
```

## 6. Common Patterns

### Testing a Lookup Function

```c
static test_result_t test_my_lookup(test_case_t *test)
{
    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        int result = my_lookup_function(name);

        if (should_exist) {
            TEST_ASSERT_INT_GT(result, -1);  /* Valid result */
        } else {
            TEST_ASSERT_INT_EQ(-1, result);  /* Not found */
        }
    }

    return TEST_SUCCESS;
}
```

### Testing Data Integrity

```c
static test_result_t test_data_integrity(test_case_t *test)
{
    /* Skip if no data loaded */
    if (my_data_count <= 0) {
        return TEST_SKIP;
    }

    MY_DATA_T *item;
    for (item = my_data_list; item; item = item->next) {
        /* Must have valid ID */
        TEST_ASSERT_NOT_NULL(item->id);
        TEST_ASSERT_TRUE(strlen(item->id) > 0);

        /* Must have valid name */
        TEST_ASSERT_NOT_NULL(item->name);
        TEST_ASSERT_TRUE(strlen(item->name) > 0);

        /* Check for duplicates */
        for (MY_DATA_T *other = item->next; other; other = other->next) {
            if (other->id) {
                TEST_ASSERT_STR_NEQ(item->id, other->id);
            }
        }
    }

    return TEST_SUCCESS;
}
```

### Testing with Fake Players

```c
static test_result_t test_with_fake_player(test_case_t *test)
{
    /* Find test fixture room */
    ROOM_INDEX_DATA *room = get_room_index(2);  /* Bootstrap test room */
    TEST_ASSERT_NOT_NULL(room);

    /* Create temporary character for testing */
    CHAR_DATA *ch = create_mobile(get_mob_index(100));  /* Test mob */
    TEST_ASSERT_NOT_NULL(ch);
    
    char_to_room(ch, room);
    
    /* Run your test logic */
    bool result = my_feature_function(ch);
    TEST_ASSERT_TRUE(result);
    
    /* Clean up */
    extract_char(ch, TRUE);
    
    return TEST_SUCCESS;
}
```

## 7. Gotchas

### BUILD_TESTS Guards

**Always** wrap test code in `#ifdef BUILD_TESTS`:

```c
#ifdef BUILD_TESTS
/* All test code here */
#endif /* BUILD_TESTS */
```

### Both Build Files

When adding new test source files, update **both** build systems:
- `CMakeLists.txt` (in the `if(BUILD_TESTS)` block)
- `Makefile` (in the `ifdef BUILD_TESTS` block)

### Unique Test Names

Test names must be unique across all JSON files:
```json
/* BAD: This name might conflict */
"name": "basic_test"

/* GOOD: Include the feature name */
"name": "my_feature_basic_test"
```

### Valid JSON

Always validate your JSON syntax:
```bash
# Check syntax before building
cat tests/data/unit/my_tests.json | jq . > /dev/null
```

### Handler Registration Order

In `test_dispatcher.c`, place exact matches before substring matches:

```c
/* Good: exact matches first */
{ "my_specific_test", run_specific_handler, MATCH_EXACT },
{ "my_", run_general_handler, MATCH_SUBSTR },

/* Bad: substring match would catch everything */
{ "my_", run_general_handler, MATCH_SUBSTR },
{ "my_specific_test", run_specific_handler, MATCH_EXACT },  /* Never reached! */
```

### Test Dependencies

For integration tests requiring specific data, use `dependencies` and `requires_mud_environment`:

```json
{
  "requires_mud_environment": true,
  "dependencies": ["area_loading"],
  "timeout_seconds": 30
}
```

Remember: Start simple, write tests first, and build incrementally. The TDD cycle keeps your code focused and well-tested from the beginning.