#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "test_framework.h"
#include "../../log.h"

test_suite_t *test_suites = NULL;
bool framework_initialized = false;

void init_test_framework(void) {
    if (framework_initialized) {
        return;
    }
    
    test_suites = NULL;
    framework_initialized = true;
    
    log_message(LOG_LEVEL_INFO, LOG_DEBUG, "Test framework initialized");
}

void cleanup_test_framework(void) {
    if (!framework_initialized) {
        return;
    }
    
    // Clean up test suites
    test_suite_t *suite = test_suites;
    while (suite) {
        test_suite_t *next_suite = suite->next;
        
        // Clean up test cases
        test_case_t *test = suite->tests;
        while (test) {
            test_case_t *next_test = test->next;
            free(test->name);
            free(test->description);
            if (test->config) {
                json_decref(test->config);
            }
            free(test);
            test = next_test;
        }
        
        free(suite->name);
        free(suite->description);
        free(suite);
        suite = next_suite;
    }
    
    test_suites = NULL;
    framework_initialized = false;
    
    log_message(LOG_LEVEL_INFO, LOG_DEBUG, "Test framework cleaned up");
}

json_t *load_json_file(const char *filepath) {
    json_error_t error;
    json_t *root = json_load_file(filepath, 0, &error);
    
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                      "Failed to parse JSON file %s: %s (line %d)", 
                      filepath, error.text, error.line);
        return NULL;
    }
    
    return root;
}

const char *json_get_string(json_t *obj, const char *key) {
    json_t *value = json_object_get(obj, key);
    if (!json_is_string(value)) {
        return NULL;
    }
    return json_string_value(value);
}

int json_get_int(json_t *obj, const char *key) {
    json_t *value = json_object_get(obj, key);
    if (!json_is_integer(value)) {
        return 0;
    }
    return (int)json_integer_value(value);
}

bool json_get_bool(json_t *obj, const char *key) {
    json_t *value = json_object_get(obj, key);
    if (!json_is_boolean(value)) {
        return false;
    }
    return json_boolean_value(value);
}

const char *test_result_to_string(test_result_t result) {
    switch (result) {
        case TEST_SUCCESS: return "PASS";
        case TEST_FAILURE: return "FAIL";
        case TEST_ERROR:   return "ERROR";
        case TEST_SKIP:    return "SKIP";
        default:          return "UNKNOWN";
    }
}

void print_test_stats(test_stats_t stats) {
    printf("\n=== Test Results ===\n");
    printf("Total:   %d\n", stats.total);
    printf("Passed:  %d\n", stats.passed);
    printf("Failed:  %d\n", stats.failed);
    printf("Errors:  %d\n", stats.errors);
    printf("Skipped: %d\n", stats.skipped);
    printf("==================\n");
    
    log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, 
                  "Test run complete: %d total, %d passed, %d failed, %d errors, %d skipped",
                  stats.total, stats.passed, stats.failed, stats.errors, stats.skipped);
}

bool test_environment_ready(void) {
    // Check if MUD environment is properly initialized
    if (!area_first) {
        log_message(LOG_LEVEL_WARN, LOG_DEBUG, "Test environment not ready: no areas loaded");
        return false;
    }
    
    // Check for essential areas
    if (!find_area("Limbo")) {
        log_message(LOG_LEVEL_WARN, LOG_DEBUG, "Test environment not ready: Limbo area not found");
        return false;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "Test environment ready");
    return true;
}

test_suite_t *find_test_suite(const char *name) {
    if (!name) {
        return NULL;
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        if (strcmp(suite->name, name) == 0) {
            return suite;
        }
        suite = suite->next;
    }
    
    return NULL;
}

test_stats_t run_all_tests(void) {
    test_stats_t total_stats = {0, 0, 0, 0, 0};
    
    if (!framework_initialized) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Test framework not initialized");
        total_stats.errors = 1;
        total_stats.total = 1;
        return total_stats;
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        test_stats_t suite_stats = run_test_suite(suite);
        total_stats.total += suite_stats.total;
        total_stats.passed += suite_stats.passed;
        total_stats.failed += suite_stats.failed;
        total_stats.errors += suite_stats.errors;
        total_stats.skipped += suite_stats.skipped;
        
        suite = suite->next;
    }
    
    return total_stats;
}

test_stats_t run_test_suite(test_suite_t *suite) {
    test_stats_t stats = {0, 0, 0, 0, 0};
    
    if (!suite) {
        return stats;
    }
    
    printf("\n[TESTS] Running test suite: %s\n", suite->name);
    fflush(stdout);
    log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, "Running test suite: %s", suite->name);
    
    // Check if environment is ready for this suite
    if (suite->requires_mud_environment && !test_environment_ready()) {
        log_message_f(LOG_LEVEL_WARN, LOG_DEBUG, 
                      "Skipping test suite %s: MUD environment not ready", suite->name);
        test_case_t *test = suite->tests;
        while (test) {
            stats.total++;
            stats.skipped++;
            test = test->next;
        }
        return stats;
    }
    
    test_case_t *test = suite->tests;
    while (test) {
        test_result_t result = run_test_case(test);
        stats.total++;
        
        switch (result) {
            case TEST_SUCCESS: stats.passed++; break;
            case TEST_FAILURE: stats.failed++; break;
            case TEST_ERROR:   stats.errors++; break;
            case TEST_SKIP:    stats.skipped++; break;
        }
        
        test = test->next;
    }
    
    return stats;
}

bool validate_dependencies(void) {
    if (!framework_initialized) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Cannot validate dependencies: framework not initialized");
        return false;
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        if (!check_suite_dependencies(suite)) {
            return false;
        }
        suite = suite->next;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "All test dependencies validated successfully");
    return true;
}

bool check_suite_dependencies(test_suite_t *suite) {
    if (!suite || !suite->dependencies) {
        return true; // No dependencies to check
    }
    
    for (int i = 0; i < suite->dependency_count; i++) {
        if (!find_test_suite(suite->dependencies[i])) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                          "Suite %s depends on missing suite: %s", 
                          suite->name, suite->dependencies[i]);
            return false;
        }
    }
    
    return true;
}

bool check_test_dependencies(test_case_t *test, test_suite_t *suite) {
    if (!test || !test->dependencies || !suite) {
        return true; // No dependencies to check
    }
    
    for (int i = 0; i < test->dependency_count; i++) {
        bool found = false;
        test_case_t *dep_test = suite->tests;
        
        while (dep_test) {
            if (strcmp(dep_test->name, test->dependencies[i]) == 0) {
                found = true;
                break;
            }
            dep_test = dep_test->next;
        }
        
        if (!found) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Test %s depends on missing test: %s",
                          test->name, test->dependencies[i]);
            return false;
        }
    }
    
    return true;
}

#endif // BUILD_TESTS