#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "test_framework.h"
#include "../../log.h"

static bool load_test_suites_recursive(const char *dir_path);
static bool is_json_file(const char *filename);
static test_case_t *create_test_case_from_json(json_t *test_json);
static test_suite_t *create_test_suite_from_json(json_t *suite_json);

static bool test_loader_verbose = true;

void set_test_loader_logging(bool enabled) {
    test_loader_verbose = enabled;
}

bool load_test_suite_from_file(const char *filepath) {
    if (!filepath) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "NULL filepath provided to load_test_suite_from_file");
        return false;
    }
    
    json_t *root = load_json_file(filepath);
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to load test suite file: %s", filepath);
        return false;
    }
    
    // Check if this is a configuration file, not a test suite
    if (json_object_get(root, "test_configuration")) {
        json_decref(root);
        if (test_loader_verbose) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Skipping configuration file: %s", filepath);
        }
        return true; // Not an error, just not a test suite
    }
    
    test_suite_t *suite = create_test_suite_from_json(root);
    json_decref(root);
    
    if (!suite) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test suite from file: %s", filepath);
        return false;
    }
    
    // Add to global test suite list
    suite->next = test_suites;
    test_suites = suite;
    
    if (test_loader_verbose) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Loaded test suite: %s from %s", suite->name, filepath);
    }
    return true;
}

bool load_all_test_suites(const char *test_data_dir) {
    if (!test_data_dir) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "NULL test_data_dir provided");
        return false;
    }
    
    // Load test suites recursively from directory and subdirectories
    return load_test_suites_recursive(test_data_dir);
}

static bool load_test_suites_recursive(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Cannot open test directory: %s", dir_path);
        return false;
    }
    
    struct dirent *entry;
    bool success = true;
    int loaded_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        // Check if it's a directory
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // Recursively load from subdirectory
                if (test_loader_verbose) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Scanning subdirectory: %s", full_path);
                }
                if (!load_test_suites_recursive(full_path)) {
                    success = false;
                }
            } else if (S_ISREG(statbuf.st_mode) && is_json_file(entry->d_name)) {
                // Load JSON test file
                if (load_test_suite_from_file(full_path)) {
                    loaded_count++;
                } else {
                    success = false;
                    log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "Failed to load test suite: %s", full_path);
                }
            }
        }
    }
    
    closedir(dir);
    
    if (loaded_count > 0 && test_loader_verbose) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Loaded %d test suite files from %s", loaded_count, dir_path);
    }
    
    return success;
}

static bool is_json_file(const char *filename) {
    if (!filename) return false;
    
    size_t len = strlen(filename);
    return (len > 5 && strcmp(filename + len - 5, ".json") == 0);
}

static test_suite_t *create_test_suite_from_json(json_t *suite_json) {
    if (!json_is_object(suite_json)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid JSON: expected object for test suite");
        return NULL;
    }
    
    const char *suite_name = test_json_get_string(suite_json, "test_suite");
    const char *description = test_json_get_string(suite_json, "description");
    const char *version = test_json_get_string(suite_json, "version");
    bool requires_mud = test_json_get_bool(suite_json, "requires_mud_environment");
    int timeout = test_json_get_int(suite_json, "timeout_seconds");
    json_t *dependencies_array = json_object_get(suite_json, "dependencies");
    json_t *tests_array = json_object_get(suite_json, "tests");
    
    if (!suite_name || !json_is_array(tests_array)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid test suite JSON: missing required fields");
        return NULL;
    }
    
    test_suite_t *suite = malloc(sizeof(test_suite_t));
    if (!suite) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory allocation failed for test suite");
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
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid JSON: expected object for test case");
        return NULL;
    }
    
    const char *test_name = test_json_get_string(test_json, "name");
    const char *description = test_json_get_string(test_json, "description");
    const char *test_type = test_json_get_string(test_json, "test_type");
    json_t *dependencies_array = json_object_get(test_json, "dependencies");
    
    if (!test_name) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid test case JSON: missing name field");
        return NULL;
    }
    
    test_case_t *test_case = malloc(sizeof(test_case_t));
    if (!test_case) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory allocation failed for test case");
        return NULL;
    }
    
    test_case->name = strdup(test_name);
    test_case->description = description ? strdup(description) : strdup("");
    test_case->test_type = test_type ? strdup(test_type) : strdup("generic");
    test_case->config = json_incref(test_json);  // Keep reference to the JSON config
    test_case->execute = NULL;  // Will be set by test type registration
    test_case->next = NULL;
    
    // Load per-test verbose and timeout settings
    test_case->verbose_output = test_json_get_bool(test_json, "verbose_output");
    test_case->timeout_seconds = test_json_get_int(test_json, "timeout_seconds");
    
    // Apply defaults if not specified
    test_config_t *config = get_test_config();
    if (config) {
        if (!json_object_get(test_json, "verbose_output")) {
            test_case->verbose_output = config->verbose_output;
        }
        if (test_case->timeout_seconds <= 0) {
            test_case->timeout_seconds = config->default_timeout_seconds;
        }
    } else {
        // Built-in fallback defaults
        if (!json_object_get(test_json, "verbose_output")) {
            test_case->verbose_output = false;
        }
        if (test_case->timeout_seconds <= 0) {
            test_case->timeout_seconds = 300; // 5 minute default
        }
    }
    
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
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test framework not initialized");
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

test_stats_t run_specific_tests(char **test_names, int count) {
    test_stats_t total_stats = {0, 0, 0, 0, 0};
    
    if (!framework_initialized) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test framework not initialized");
        total_stats.errors = 1;
        total_stats.total = 1;
        return total_stats;
    }
    
    test_config_t *config = get_test_config();
    
    for (int i = 0; i < count; i++) {
        bool found = false;
        test_case_t *test = find_test_case_global(test_names[i]);
        
        if (test) {
            found = true;
            
            if (config && config->verbose_test_names) {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Running specific test: %s", test->name);
            }
            
            test_result_t result = run_test_case(test);
            total_stats.total++;
            
            switch (result) {
                case TEST_SUCCESS: total_stats.passed++; break;
                case TEST_FAILURE: total_stats.failed++; break;
                case TEST_ERROR:   total_stats.errors++; break;
                case TEST_SKIP:    total_stats.skipped++; break;
            }
            
            if (config && config->stop_on_first_failure && 
                (result == TEST_FAILURE || result == TEST_ERROR)) {
                log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Stopping on first failure as configured");
                break;
            }
        }
        
        if (!found) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test not found: %s", test_names[i]);
            total_stats.errors++;
            total_stats.total++;
        }
    }
    
    return total_stats;
}

test_case_t *find_test_case(const char *suite_name, const char *test_name) {
    if (!suite_name || !test_name) {
        return NULL;
    }
    
    test_suite_t *suite = find_test_suite(suite_name);
    if (!suite) {
        return NULL;
    }
    
    test_case_t *test = suite->tests;
    while (test) {
        if (strcmp(test->name, test_name) == 0) {
            return test;
        }
        test = test->next;
    }
    
    return NULL;
}

test_case_t *find_test_case_global(const char *test_name) {
    if (!test_name) {
        return NULL;
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        test_case_t *test = suite->tests;
        while (test) {
            if (strcmp(test->name, test_name) == 0) {
                return test;
            }
            test = test->next;
        }
        suite = suite->next;
    }
    
    return NULL;
}

#endif // BUILD_TESTS