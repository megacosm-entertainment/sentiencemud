# Quick Test Development Reference

## Adding a New Test Type - 5 Minute Guide

### Step 1: Create Test Handler (C Code)

Edit `/sentience/src/tests/integration/wnum_tests.c` (or create new file):

```c
// 1. Add forward declaration
static test_result_t test_my_feature(test_case_t *test);

// 2. Add to dispatcher in run_test_case()
if (strcmp(test->test_type, "my_feature_validator") == 0) {
    result = test_my_feature(test);
}

// 3. Implement handler
static test_result_t test_my_feature(test_case_t *test) {
    // Extract input parameters
    json_t *input = json_object_get(test->config, "input");
    const char *param = json_get_string(input, "parameter");
    
    // Run test logic
    bool result = my_function_to_test(param);
    
    // Validate result
    json_t *expected = json_object_get(test->config, "expected_output");
    bool expected_result = json_get_bool(expected, "success");
    
    return (result == expected_result) ? TEST_SUCCESS : TEST_FAILURE;
}
```

### Step 2: Create JSON Test Configuration  

Create `/sentience/src/tests/data/my_feature_tests.json`:

```json
{
  "test_suite": "my_feature_tests",
  "description": "Tests for my feature",
  "version": "1.0",
  "requires_mud_environment": false,
  "dependencies": [],
  "tests": [
    {
      "name": "basic_test",
      "description": "Test basic functionality",
      "test_type": "my_feature_validator",
      "input": {
        "parameter": "test_value"
      },
      "expected_output": {
        "success": true
      }
    }
  ]
}
```

### Step 3: Build and Test

```bash
cd /sentience/src
./build tests
cd ..
./sent -test:my_feature
```

## Common Test Patterns

### Testing MUD Functions

```c
static test_result_t test_mud_function(test_case_t *test) {
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    const char *area_name = json_get_string(input, "area_name");
    
    AREA_DATA *area = find_area((char*)area_name);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                      "Area '%s' not found", area_name);
        return TEST_FAILURE;
    }
    
    return TEST_SUCCESS;
}
```

### Testing String Functions

```c
static test_result_t test_string_function(test_case_t *test) {
    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_cases, index, test_case) {
        const char *input_str = json_get_string(test_case, "input");
        bool expected = json_get_bool(test_case, "expected");
        
        bool actual = my_string_function(input_str);
        if (actual != expected) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Failed for '%s': expected %s, got %s",
                          input_str, expected ? "true" : "false",
                          actual ? "true" : "false");
            return TEST_FAILURE;
        }
    }
    return TEST_SUCCESS;
}
```

### Testing Parser Functions

```c
static test_result_t test_parser(test_case_t *test) {
    json_t *input = json_object_get(test->config, "input");
    json_t *test_strings = json_object_get(input, "test_strings");
    
    size_t index;
    json_t *string_test;
    json_array_foreach(test_strings, index, string_test) {
        const char *input_str = json_get_string(string_test, "string");
        bool should_parse = json_get_bool(string_test, "should_parse");
        
        // Test the parser
        PARSED_RESULT result;
        bool parsed = my_parser(input_str, &result);
        
        if (parsed != should_parse) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Parse test failed for '%s'", input_str);
            return TEST_FAILURE;
        }
        
        // If it should parse, validate the result
        if (should_parse && parsed) {
            int expected_value = json_get_int(string_test, "expected_value");
            if (result.value != expected_value) {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                              "Parse result wrong for '%s': expected %d, got %d",
                              input_str, expected_value, result.value);
                return TEST_FAILURE;
            }
        }
    }
    
    return TEST_SUCCESS;
}
```

## JSON Test Schemas

### Simple Function Test

```json
{
  "name": "function_validation",
  "test_type": "function_validator",
  "input": {
    "parameter": "test_input"
  },
  "expected_output": {
    "success": true,
    "return_value": 42
  }
}
```

### Multiple Test Cases

```json
{
  "name": "multiple_cases",
  "test_type": "multi_case_validator",
  "input": {
    "test_cases": [
      {"input": "case1", "expected": true},
      {"input": "case2", "expected": false},
      {"input": "case3", "expected": true}
    ]
  },
  "expected_output": {
    "all_passed": true
  }
}
```

### Coverage-Driven Unit Test

```json
{
  "name": "comprehensive_function_test",
  "test_type": "pure_function_test",
  "coverage_focus": ["target_function_name"],
  "input": {
    "test_cases": [
      {"scenario": "normal_case", "args": ["normal_input"], "expected": true},
      {"scenario": "edge_case", "args": ["edge_input"], "expected": false},
      {"scenario": "error_case", "args": [null], "expected": false}
    ]
  },
  "expected_output": {
    "all_cases_pass": true,
    "coverage_complete": true
  }
}
```

## Quick Reference Commands

```bash
# Build with tests
./build tests

# Build with coverage analysis
./build coverage

# Build with memory testing
./build tests-memory

# Run all tests
./sent -test

# Run specific suite  
./sent -test:my_feature

# Run pattern match
./sent -test:wnum

# Check logs
tail -f logs/system.log | grep -E "(TEST|debug)"

# Validate JSON
jq '.' src/tests/data/my_tests.json

# Coverage analysis
lcov --summary coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html
$BROWSER coverage_html/index.html

# Find uncovered functions
lcov --list coverage_filtered.info | grep "0.0%"

# Build clean
./build clean tests
```

## Helper Functions Available

```c
// JSON helpers
const char *json_get_string(json_t *obj, const char *key);
int json_get_int(json_t *obj, const char *key);  
bool json_get_bool(json_t *obj, const char *key);

// MUD environment
bool test_environment_ready(void);
AREA_DATA *find_area(char *name);
bool parse_widevnum(char *argument, AREA_DATA *current_area, WNUM *wnum);

// Logging
log_message(level, category, message);
log_message_f(level, category, format, ...);
```

## Test Result Values

```c
typedef enum {
    TEST_SUCCESS = 0,  // Test passed
    TEST_FAILURE = 1,  // Test failed but ran
    TEST_ERROR = 2,    // Test had an error  
    TEST_SKIP = 3      // Test was skipped
} test_result_t;
```

## Common Gotchas

1. **Build System**: Add new test files to both CMakeLists.txt AND Makefile
2. **JSON Validation**: Use `jq` to validate JSON syntax before testing
3. **MUD Environment**: Set `requires_mud_environment: true` for integration tests
4. **Dependencies**: Test dependencies run in order - plan accordingly
5. **Logging**: Use appropriate log levels (ERROR for failures, DEBUG for info)
6. **Memory**: Tests run in MUD environment - be careful with memory allocation
7. **Console Output**: Check `/sentience/data/system/zlog.conf` if output not showing
8. **Coverage**: Rebuild with coverage flags (`./build coverage`) to generate coverage data
9. **Coverage Files**: Check that .gcda and .gcno files are generated in build directory

## Error Messages

**"JSON parse error"** → Check JSON syntax with `jq`  
**"Test type not found"** → Add handler to `run_test_case()` dispatcher  
**"MUD environment not ready"** → Set `requires_mud_environment: false` or check startup  
**"Dependency not found"** → Verify referenced test suites/cases exist  
**"Build tests not enabled"** → Use `./build tests` or `make BUILD_TESTS=1`
**"No coverage data found"** → Build with coverage flags: `./build coverage`
**"lcov command not found"** → Install coverage tools: `sudo apt-get install lcov`
**"Coverage files not generated"** → Check that gcov flags are properly applied during build