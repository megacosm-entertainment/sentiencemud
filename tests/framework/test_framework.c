#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "test_framework.h"
#include "../../log.h"

test_suite_t *test_suites = NULL;
bool framework_initialized = false;
test_config_t *global_test_config = NULL;

void init_test_framework(void) {
    if (framework_initialized) {
        return;
    }
    
    test_suites = NULL;
    framework_initialized = true;
    
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test framework initialized");
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
    
    // Clean up test config
    cleanup_test_config();
    
    framework_initialized = false;
    
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test framework cleaned up");
}

json_t *load_json_file(const char *filepath) {
    json_error_t error;
    json_t *root = json_load_file(filepath, 0, &error);
    
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
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
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "\n=== Test Results ===");
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Total:   %d", stats.total);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Passed:  %d", stats.passed);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Failed:  %d", stats.failed);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Errors:  %d", stats.errors);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Skipped: %d", stats.skipped);
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "==================");
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, 
                  "Test run complete: %d total, %d passed, %d failed, %d errors, %d skipped",
                  stats.total, stats.passed, stats.failed, stats.errors, stats.skipped);
}

bool test_environment_ready(void) {
    // Check if MUD environment is properly initialized
    if (!area_first) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "Test environment not ready: no areas loaded");
        return false;
    }
    
    // Check for essential areas
    if (!find_area("Limbo")) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "Test environment not ready: Limbo area not found");
        return false;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Test environment ready");
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

static void append_selection_reason(char **reasons, int index, const char *reason) {
    if (!reasons || index < 0 || !reason || !reason[0]) {
        return;
    }

    if (!reasons[index]) {
        reasons[index] = strdup(reason);
        return;
    }

    size_t current_len = strlen(reasons[index]);
    size_t reason_len = strlen(reason);
    size_t new_len = current_len + 2 + reason_len + 1;
    char *combined = malloc(new_len);
    if (!combined) {
        return;
    }

    snprintf(combined, new_len, "%s; %s", reasons[index], reason);
    free(reasons[index]);
    reasons[index] = combined;
}

static void mark_suite_selected(test_suite_t *suite, test_suite_t **suite_list, bool *selected,
                                int suite_count, char **reasons, const char *reason) {
    if (!suite || !suite_list || !selected) {
        return;
    }

    int index = -1;
    for (int i = 0; i < suite_count; i++) {
        if (suite_list[i] == suite) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        return;
    }

    append_selection_reason(reasons, index, reason);

    if (selected[index]) {
        return;
    }

    selected[index] = true;

    for (int i = 0; i < suite->dependency_count; i++) {
        if (!suite->dependencies[i] || !suite->dependencies[i][0]) {
            continue;
        }
        test_suite_t *dep = find_test_suite(suite->dependencies[i]);
        if (dep) {
            char dep_reason[256];
            snprintf(dep_reason, sizeof(dep_reason), "dependency of %s", suite->name);
            mark_suite_selected(dep, suite_list, selected, suite_count, reasons, dep_reason);
        } else {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                          "Test suite dependency not found: %s (required by %s)",
                          suite->dependencies[i], suite->name);
        }
    }
}

void log_selected_test_suites(const char *pattern) {
    int suite_count = count_test_suites();
    if (suite_count <= 0) {
        return;
    }

    test_suite_t **suite_list = calloc(suite_count, sizeof(test_suite_t *));
    bool *selected = calloc(suite_count, sizeof(bool));
    char **reasons = calloc(suite_count, sizeof(char *));

    if (!suite_list || !selected || !reasons) {
        free(suite_list);
        free(selected);
        free(reasons);
        return;
    }

    int idx = 0;
    for (test_suite_t *suite = test_suites; suite && idx < suite_count; suite = suite->next) {
        suite_list[idx++] = suite;
    }

    if (!pattern || strcmp(pattern, "all") == 0) {
        for (int i = 0; i < suite_count; i++) {
            selected[i] = true;
            append_selection_reason(reasons, i, "all");
        }
    } else if (strncmp(pattern, "profile:", 8) == 0) {
        const char *profile_name = pattern + 8;
        test_profile_t *profile = find_test_profile(profile_name);
        if (profile) {
            for (int i = 0; i < profile->suite_count; i++) {
                test_suite_t *suite = find_test_suite(profile->test_suites[i]);
                if (suite) {
                    char reason[256];
                    snprintf(reason, sizeof(reason), "profile %s", profile_name);
                    mark_suite_selected(suite, suite_list, selected, suite_count, reasons, reason);
                }
            }
        }
    } else if (strncmp(pattern, "test:", 5) == 0) {
        const char *test_names = pattern + 5;
        char *names_copy = strdup(test_names);
        if (names_copy) {
            char *saveptr = NULL;
            char *token = strtok_r(names_copy, ",", &saveptr);
            while (token) {
                while (*token == ' ') token++;
                for (int i = 0; i < suite_count; i++) {
                    test_case_t *test = suite_list[i]->tests;
                    while (test) {
                        if (strcmp(test->name, token) == 0) {
                            char reason[256];
                            snprintf(reason, sizeof(reason), "test %s", token);
                            mark_suite_selected(suite_list[i], suite_list, selected, suite_count, reasons, reason);
                            break;
                        }
                        test = test->next;
                    }
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
            free(names_copy);
        }
    } else {
        for (int i = 0; i < suite_count; i++) {
            test_suite_t *suite = suite_list[i];
            bool matched = false;
            if (strstr(suite->name, pattern)) {
                matched = true;
            } else {
                test_case_t *test = suite->tests;
                while (test) {
                    if (strstr(test->name, pattern)) {
                        matched = true;
                        break;
                    }
                    test = test->next;
                }
            }

            if (matched) {
                char reason[256];
                snprintf(reason, sizeof(reason), "pattern %s", pattern);
                mark_suite_selected(suite, suite_list, selected, suite_count, reasons, reason);
            }
        }
    }

    int selected_count = 0;
    for (int i = 0; i < suite_count; i++) {
        if (selected[i]) {
            selected_count++;
        }
    }

    if (selected_count > 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Selected test suites (%d):", selected_count);
        for (int i = 0; i < suite_count; i++) {
            if (selected[i]) {
                if (reasons[i]) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  - %s (%s)", suite_list[i]->name, reasons[i]);
                } else {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  - %s", suite_list[i]->name);
                }
            }
        }
    }

    for (int i = 0; i < suite_count; i++) {
        free(reasons[i]);
    }
    free(suite_list);
    free(selected);
    free(reasons);
}

test_stats_t run_all_tests(void) {
    test_stats_t total_stats = {0, 0, 0, 0, 0};
    
    if (!framework_initialized) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test framework not initialized");
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
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Running test suite: %s", suite->name);
    
    // Check if environment is ready for this suite
    if (suite->requires_mud_environment && !test_environment_ready()) {
        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS, 
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
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Cannot validate dependencies: framework not initialized");
        return false;
    }
    
    test_suite_t *suite = test_suites;
    while (suite) {
        if (!check_suite_dependencies(suite)) {
            return false;
        }
        suite = suite->next;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "All test dependencies validated successfully");
    return true;
}

bool check_suite_dependencies(test_suite_t *suite) {
    if (!suite || !suite->dependencies) {
        return true; // No dependencies to check
    }
    
    for (int i = 0; i < suite->dependency_count; i++) {
        if (!find_test_suite(suite->dependencies[i])) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
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
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Test %s depends on missing test: %s",
                          test->name, test->dependencies[i]);
            return false;
        }
    }
    
    return true;
}

// Test registry and discovery functions
void print_test_registry(void) {
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "=== Test Registry ===");
    
    int suite_count = 0;
    int total_tests = 0;
    
    test_suite_t *suite = test_suites;
    while (suite) {
        suite_count++;
        int test_count = 0;
        test_case_t *test = suite->tests;
        while (test) {
            test_count++;
            test = test->next;
        }
        total_tests += test_count;
        
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Suite: %s (%d tests)", suite->name, test_count);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Description: %s", suite->description);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Requires MUD: %s", suite->requires_mud_environment ? "Yes" : "No");
        
        char dep_buffer[512] = "";
        if (suite->dependency_count > 0) {
            for (int i = 0; i < suite->dependency_count; i++) {
                if (i > 0) strcat(dep_buffer, ", ");
                strcat(dep_buffer, suite->dependencies[i]);
            }
        } else {
            strcpy(dep_buffer, "None");
        }
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Dependencies: %s", dep_buffer);
        
        // List individual tests
        test = suite->tests;
        while (test) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "    - %s (%s)", test->name, test->test_type);
            test = test->next;
        }
        
        suite = suite->next;
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Total: %d test suites, %d tests", suite_count, total_tests);
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "====================");
}

void print_test_suites_summary(void) {
    int integration_suites = 0;
    int unit_suites = 0;
    int total_tests = 0;
    
    test_suite_t *suite = test_suites;
    while (suite) {
        if (suite->requires_mud_environment) {
            integration_suites++;
        } else {
            unit_suites++;
        }
        
        test_case_t *test = suite->tests;
        while (test) {
            total_tests++;
            test = test->next;
        }
        
        suite = suite->next;
    }
    
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test Suites Summary:");
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Integration suites: %d", integration_suites);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Unit test suites: %d", unit_suites);
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Total tests: %d", total_tests);
}

int count_total_tests(void) {
    int total = 0;
    test_suite_t *suite = test_suites;
    
    while (suite) {
        test_case_t *test = suite->tests;
        while (test) {
            total++;
            test = test->next;
        }
        suite = suite->next;
    }
    
    return total;
}

int count_test_suites(void) {
    int count = 0;
    test_suite_t *suite = test_suites;
    
    while (suite) {
        count++;
        suite = suite->next;
    }
    
    return count;
}

// Test configuration functions
bool load_test_config(const char *config_file) {
    json_t *root = load_json_file(config_file);
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to load test config from %s", config_file);
        return false;
    }
    
    json_t *test_config_obj = json_object_get(root, "test_configuration");
    if (!test_config_obj) {
        // Check if this is the new format with type field
        const char *file_type = json_get_string(root, "type");
        if (file_type && strcmp(file_type, "test_configuration") == 0) {
            test_config_obj = json_object_get(root, "test_configuration");
        }
        
        if (!test_config_obj) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Missing test_configuration object in config file");
            json_decref(root);
            return false;
        }
    }
    
    // Clean up existing config
    if (global_test_config) {
        cleanup_test_config();
    }
    
    global_test_config = calloc(1, sizeof(test_config_t));
    if (!global_test_config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to allocate memory for test config");
        json_decref(root);
        return false;
    }
    
    // Load basic config values
    const char *version = json_get_string(test_config_obj, "version");
    if (version) global_test_config->version = strdup(version);
    
    const char *description = json_get_string(test_config_obj, "description");
    if (description) global_test_config->description = strdup(description);
    
    // Load settings
    json_t *settings = json_object_get(test_config_obj, "settings");
    if (settings) {
        global_test_config->default_timeout_seconds = json_get_int(settings, "default_timeout_seconds");
        global_test_config->stop_on_first_failure = json_get_bool(settings, "stop_on_first_failure");
        global_test_config->run_unit_tests_first = json_get_bool(settings, "run_unit_tests_first");
        global_test_config->require_mud_environment_for_integration = json_get_bool(settings, "require_mud_environment_for_integration");
        global_test_config->verbose_output = json_get_bool(settings, "verbose_output");
        global_test_config->verbose_test_names = json_get_bool(settings, "verbose_test_names");
        global_test_config->verbose_test_details = json_get_bool(settings, "verbose_test_details");
        global_test_config->show_test_config = json_get_bool(settings, "show_test_config");
        global_test_config->show_execution_time = json_get_bool(settings, "show_execution_time");
    }
    
    // Helper function to load string arrays
    auto int load_string_array(json_t *parent, const char *key, char ***array, int *count) {
        json_t *arr = json_object_get(parent, key);
        if (!arr || !json_is_array(arr)) {
            *count = 0;
            return 0;
        }
        
        *count = json_array_size(arr);
        *array = calloc(*count, sizeof(char*));
        
        for (int i = 0; i < *count; i++) {
            json_t *item = json_array_get(arr, i);
            if (json_is_string(item)) {
                (*array)[i] = strdup(json_string_value(item));
            }
        }
        return *count;
    }
    
    // Load test suite arrays
    load_string_array(test_config_obj, "default_test_suites", &global_test_config->default_test_suites, &global_test_config->default_suite_count);
    load_string_array(test_config_obj, "quick_test_suites", &global_test_config->quick_test_suites, &global_test_config->quick_suite_count);
    load_string_array(test_config_obj, "full_test_suites", &global_test_config->full_test_suites, &global_test_config->full_suite_count);
    load_string_array(test_config_obj, "unit_only_suites", &global_test_config->unit_only_suites, &global_test_config->unit_only_count);
    load_string_array(test_config_obj, "integration_only_suites", &global_test_config->integration_only_suites, &global_test_config->integration_only_count);
    load_string_array(test_config_obj, "exclude_patterns", &global_test_config->exclude_patterns, &global_test_config->exclude_pattern_count);
    
    // Load test profiles
    json_t *profiles_obj = json_object_get(test_config_obj, "test_profiles");
    if (profiles_obj && json_is_object(profiles_obj)) {
        global_test_config->profile_count = json_object_size(profiles_obj);
        global_test_config->profiles = calloc(global_test_config->profile_count, sizeof(test_profile_t));
        
        int profile_idx = 0;
        const char *profile_name;
        json_t *profile_data;
        json_object_foreach(profiles_obj, profile_name, profile_data) {
            test_profile_t *profile = &global_test_config->profiles[profile_idx];
            profile->name = strdup(profile_name);
            
            const char *desc = json_get_string(profile_data, "description");
            if (desc) profile->description = strdup(desc);
            
            profile->stop_on_first_failure = json_get_bool(profile_data, "stop_on_first_failure");
            profile->require_coverage = json_get_bool(profile_data, "require_coverage");
            
            load_string_array(profile_data, "suites", &profile->test_suites, &profile->suite_count);
            profile_idx++;
        }
    }
    
    json_decref(root);
    log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test configuration loaded successfully");
    return true;
}

test_config_t *get_test_config(void) {
    return global_test_config;
}

test_profile_t *find_test_profile(const char *name) {
    if (!global_test_config || !name) {
        return NULL;
    }
    
    for (int i = 0; i < global_test_config->profile_count; i++) {
        if (strcmp(global_test_config->profiles[i].name, name) == 0) {
            return &global_test_config->profiles[i];
        }
    }
    
    return NULL;
}

char **get_default_test_suites(int *count) {
    if (!global_test_config) {
        *count = 0;
        return NULL;
    }
    
    *count = global_test_config->default_suite_count;
    return global_test_config->default_test_suites;
}

void cleanup_test_config(void) {
    if (!global_test_config) {
        return;
    }
    
    // Helper function to free string arrays
    auto void free_string_array(char **array, int count) {
        if (array) {
            for (int i = 0; i < count; i++) {
                free(array[i]);
            }
            free(array);
        }
    }
    
    free(global_test_config->version);
    free(global_test_config->description);
    
    free_string_array(global_test_config->default_test_suites, global_test_config->default_suite_count);
    free_string_array(global_test_config->quick_test_suites, global_test_config->quick_suite_count);
    free_string_array(global_test_config->full_test_suites, global_test_config->full_suite_count);
    free_string_array(global_test_config->unit_only_suites, global_test_config->unit_only_count);
    free_string_array(global_test_config->integration_only_suites, global_test_config->integration_only_count);
    free_string_array(global_test_config->exclude_patterns, global_test_config->exclude_pattern_count);
    
    // Free profiles
    if (global_test_config->profiles) {
        for (int i = 0; i < global_test_config->profile_count; i++) {
            test_profile_t *profile = &global_test_config->profiles[i];
            free(profile->name);
            free(profile->description);
            free_string_array(profile->test_suites, profile->suite_count);
        }
        free(global_test_config->profiles);
    }
    
    free(global_test_config);
    global_test_config = NULL;
}

#endif // BUILD_TESTS