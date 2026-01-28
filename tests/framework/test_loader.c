#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "test_framework.h"
#include "../../log.h"

static test_case_t *create_test_case_from_json(json_t *test_json);
static test_suite_t *create_test_suite_from_json(json_t *suite_json);

bool load_test_suite_from_file(const char *filepath) {
    if (!filepath) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "NULL filepath provided to load_test_suite_from_file");
        return false;
    }
    
    json_t *root = load_json_file(filepath);
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to load test suite file: %s", filepath);
        return false;
    }
    
    test_suite_t *suite = create_test_suite_from_json(root);
    json_decref(root);
    
    if (!suite) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to create test suite from file: %s", filepath);
        return false;
    }
    
    // Add to global test suite list
    suite->next = test_suites;
    test_suites = suite;
    
    log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, "Loaded test suite: %s from %s", suite->name, filepath);
    return true;
}

bool load_all_test_suites(const char *test_data_dir) {
    if (!test_data_dir) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "NULL test_data_dir provided");
        return false;
    }
    
    DIR *dir = opendir(test_data_dir);
    if (!dir) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Cannot open test data directory: %s", test_data_dir);
        return false;
    }
    
    struct dirent *entry;
    bool success = true;
    int loaded_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip non-JSON files and hidden files
        if (strstr(entry->d_name, ".json") == NULL || entry->d_name[0] == '.') {
            continue;
        }
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", test_data_dir, entry->d_name);
        
        if (load_test_suite_from_file(filepath)) {
            loaded_count++;
        } else {
            success = false;
            log_message_f(LOG_LEVEL_WARN, LOG_ERROR, "Failed to load test suite: %s", filepath);
        }
    }
    
    closedir(dir);
    
    log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, "Loaded %d test suite files from %s", loaded_count, test_data_dir);
    return success;
}

static test_suite_t *create_test_suite_from_json(json_t *suite_json) {
    if (!json_is_object(suite_json)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid JSON: expected object for test suite");
        return NULL;
    }
    
    const char *suite_name = json_get_string(suite_json, "test_suite");
    const char *description = json_get_string(suite_json, "description");
    const char *version = json_get_string(suite_json, "version");
    bool requires_mud = json_get_bool(suite_json, "requires_mud_environment");
    int timeout = json_get_int(suite_json, "timeout_seconds");
    json_t *dependencies_array = json_object_get(suite_json, "dependencies");
    json_t *tests_array = json_object_get(suite_json, "tests");
    
    if (!suite_name || !json_is_array(tests_array)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid test suite JSON: missing required fields");
        return NULL;
    }
    
    test_suite_t *suite = malloc(sizeof(test_suite_t));
    if (!suite) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Memory allocation failed for test suite");
        return NULL;
    }
    
    suite->name = strdup(suite_name);
    suite->description = description ? strdup(description) : strdup("");
    suite->version = version ? strdup(version) : strdup("1.0");
    suite->requires_mud_environment = requires_mud;
    suite->timeout_seconds = timeout > 0 ? timeout : 60;
    suite->tests = NULL;
    suite->next = NULL;
    
    // Handle dependencies
    if (json_is_array(dependencies_array)) {
        suite->dependency_count = json_array_size(dependencies_array);
        suite->dependencies = malloc(sizeof(char*) * suite->dependency_count);
        
        for (size_t i = 0; i < suite->dependency_count; i++) {
            json_t *dep = json_array_get(dependencies_array, i);
            if (json_is_string(dep)) {
                suite->dependencies[i] = strdup(json_string_value(dep));
            } else {
                suite->dependencies[i] = strdup("");
            }
        }
    } else {
        suite->dependency_count = 0;
        suite->dependencies = NULL;
    }
    
    // Load test cases
    size_t index;
    json_t *test_json;
    json_array_foreach(tests_array, index, test_json) {
        test_case_t *test_case = create_test_case_from_json(test_json);
        if (test_case) {
            test_case->next = suite->tests;
            suite->tests = test_case;
        }
    }
    
    return suite;
}

static test_case_t *create_test_case_from_json(json_t *test_json) {
    if (!json_is_object(test_json)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid JSON: expected object for test case");
        return NULL;
    }
    
    const char *test_name = json_get_string(test_json, "name");
    const char *description = json_get_string(test_json, "description");
    const char *test_type = json_get_string(test_json, "test_type");
    json_t *dependencies_array = json_object_get(test_json, "dependencies");
    
    if (!test_name) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid test case JSON: missing name field");
        return NULL;
    }
    
    test_case_t *test_case = malloc(sizeof(test_case_t));
    if (!test_case) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Memory allocation failed for test case");
        return NULL;
    }
    
    test_case->name = strdup(test_name);
    test_case->description = description ? strdup(description) : strdup("");
    test_case->test_type = test_type ? strdup(test_type) : strdup("generic");
    test_case->config = json_incref(test_json);  // Keep reference to the JSON config
    test_case->execute = NULL;  // Will be set by test type registration
    test_case->next = NULL;
    
    // Handle dependencies
    if (json_is_array(dependencies_array)) {
        test_case->dependency_count = json_array_size(dependencies_array);
        test_case->dependencies = malloc(sizeof(char*) * test_case->dependency_count);
        
        for (size_t i = 0; i < test_case->dependency_count; i++) {
            json_t *dep = json_array_get(dependencies_array, i);
            if (json_is_string(dep)) {
                test_case->dependencies[i] = strdup(json_string_value(dep));
            } else {
                test_case->dependencies[i] = strdup("");
            }
        }
    } else {
        test_case->dependency_count = 0;
        test_case->dependencies = NULL;
    }
    
    return test_case;
}

test_stats_t run_tests_by_pattern(const char *pattern) {
    test_stats_t total_stats = {0, 0, 0, 0, 0};
    
    if (!framework_initialized) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Test framework not initialized");
        total_stats.errors = 1;
        total_stats.total = 1;
        return total_stats;
    }
    
    if (!pattern) {
        return run_all_tests();
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        // Check if suite name matches pattern
        if (strstr(suite->name, pattern) != NULL) {
            test_stats_t suite_stats = run_test_suite(suite);
            total_stats.total += suite_stats.total;
            total_stats.passed += suite_stats.passed;
            total_stats.failed += suite_stats.failed;
            total_stats.errors += suite_stats.errors;
            total_stats.skipped += suite_stats.skipped;
        } else {
            // Check individual test cases within the suite
            test_case_t *test = suite->tests;
            while (test) {
                if (strstr(test->name, pattern) != NULL) {
                    test_result_t result = run_test_case(test);
                    total_stats.total++;
                    
                    switch (result) {
                        case TEST_SUCCESS: total_stats.passed++; break;
                        case TEST_FAILURE: total_stats.failed++; break;
                        case TEST_ERROR:   total_stats.errors++; break;
                        case TEST_SKIP:    total_stats.skipped++; break;
                    }
                }
                test = test->next;
            }
        }
        
        suite = suite->next;
    }
    
    return total_stats;
}

#endif // BUILD_TESTS