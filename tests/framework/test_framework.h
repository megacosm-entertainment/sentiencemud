#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#ifdef BUILD_TESTS

#include <stdbool.h>
#include <jansson.h>
#include "../../merc.h"

// Test result codes
typedef enum {
    TEST_SUCCESS = 0,
    TEST_FAILURE = 1,
    TEST_ERROR = 2,
    TEST_SKIP = 3
} test_result_t;

// Test statistics
typedef struct {
    int total;
    int passed;
    int failed;
    int errors;
    int skipped;
} test_stats_t;

// Test case definition
typedef struct test_case {
    char *name;
    char *description;
    char *test_type;
    char **dependencies;  // Array of test names this depends on
    int dependency_count;
    json_t *config;
    test_result_t (*execute)(struct test_case *test);
    struct test_case *next;
} test_case_t;

// Test suite definition
typedef struct test_suite {
    char *name;
    char *description;
    char *version;
    bool requires_mud_environment;
    int timeout_seconds;
    char **dependencies;  // Array of suite names this depends on
    int dependency_count;
    test_case_t *tests;
    struct test_suite *next;
} test_suite_t;

// Test framework functions
void init_test_framework(void);
void cleanup_test_framework(void);
extern bool framework_initialized;
extern test_suite_t *test_suites;

// Test discovery and loading
bool load_test_suite_from_file(const char *filepath);
bool load_all_test_suites(const char *test_data_dir);
test_suite_t *find_test_suite(const char *name);

// Test execution
test_result_t run_test_case(test_case_t *test);
test_stats_t run_test_suite(test_suite_t *suite);
test_stats_t run_all_tests(void);
test_stats_t run_tests_by_pattern(const char *pattern);

// Dependency management
bool validate_dependencies(void);
bool check_suite_dependencies(test_suite_t *suite);
bool check_test_dependencies(test_case_t *test, test_suite_t *suite);

// Test registration for specific test types
void register_wnum_tests(void);
void register_area_loading_tests(void);
void register_database_tests(void);

// Utility functions
const char *test_result_to_string(test_result_t result);
void print_test_stats(test_stats_t stats);
bool test_environment_ready(void);

// JSON parsing helpers
json_t *load_json_file(const char *filepath);
const char *json_get_string(json_t *obj, const char *key);
int json_get_int(json_t *obj, const char *key);
bool json_get_bool(json_t *obj, const char *key);

#endif // BUILD_TESTS

#endif // TEST_FRAMEWORK_H