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
    bool verbose_output;   // Per-test verbose setting
    int timeout_seconds;   // Per-test timeout override
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

// Test configuration structures
typedef struct test_profile {
    char *name;
    char *description;
    char **test_suites;
    int suite_count;
    bool stop_on_first_failure;
    bool require_coverage;
} test_profile_t;

typedef struct test_config {
    char *version;
    char *description;
    char **default_test_suites;
    int default_suite_count;
    char **quick_test_suites;
    int quick_suite_count;
    char **full_test_suites;
    int full_suite_count;
    char **unit_only_suites;
    int unit_only_count;
    char **integration_only_suites;
    int integration_only_count;
    char **exclude_patterns;
    int exclude_pattern_count;
    int default_timeout_seconds;
    bool stop_on_first_failure;
    bool run_unit_tests_first;
    bool require_mud_environment_for_integration;
    bool verbose_output;
    bool verbose_test_names;
    bool verbose_test_details;
    bool show_test_config;
    bool show_execution_time;
    test_profile_t *profiles;
    int profile_count;
} test_config_t;

// Test framework functions
void init_test_framework(void);
void cleanup_test_framework(void);
extern bool framework_initialized;
extern test_suite_t *test_suites;

// Test discovery and loading
bool load_test_suite_from_file(const char *filepath);
bool load_all_test_suites(const char *test_data_dir);
void set_test_loader_logging(bool enabled);
test_suite_t *find_test_suite(const char *name);

// Test configuration
bool load_test_config(const char *config_file);
test_config_t *get_test_config(void);
test_profile_t *find_test_profile(const char *name);
char **get_default_test_suites(int *count);
void cleanup_test_config(void);

// Test execution
test_result_t run_test_case(test_case_t *test);
test_stats_t run_test_suite(test_suite_t *suite);
test_stats_t run_all_tests(void);
test_stats_t run_tests_by_pattern(const char *pattern);
test_stats_t run_specific_tests(char **test_names, int count);
test_case_t *find_test_case(const char *suite_name, const char *test_name);
test_case_t *find_test_case_global(const char *test_name);

// Dependency management
bool validate_dependencies(void);
bool check_suite_dependencies(test_suite_t *suite);
bool check_test_dependencies(test_case_t *test, test_suite_t *suite);

// Test registry and discovery functions
void print_test_registry(void);
void print_test_suites_summary(void);
test_suite_t *find_test_suite(const char *name);
int count_total_tests(void);
int count_test_suites(void);
void log_selected_test_suites(const char *pattern);

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