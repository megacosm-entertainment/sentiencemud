#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <time.h>
#include "test_framework.h"
#include "../../log.h"

test_suite_t *test_suites = NULL;
bool framework_initialized = false;
test_config_t *global_test_config = NULL;

typedef enum {
    TEST_FILE_LOG_TEXT = 0,
    TEST_FILE_LOG_JSON = 1
} test_file_log_format_t;

static FILE *test_file_log = NULL;
static test_file_log_format_t test_file_log_format = TEST_FILE_LOG_TEXT;
static char test_file_log_path[512] = "";

static void build_iso8601_timestamp(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (!buffer || buffer_size == 0) {
        return;
    }

    if (!localtime_r(&now, &tm_now)) {
        buffer[0] = '\0';
        return;
    }

    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S%z", &tm_now);
}

static const char *resolve_test_log_output_file(void)
{
    const char *env_file = getenv("SENTIENCE_TEST_LOG_FILE");
    if (env_file && env_file[0]) {
        return env_file;
    }

    if (global_test_config && global_test_config->log_output_file && global_test_config->log_output_file[0]) {
        return global_test_config->log_output_file;
    }

    return NULL;
}

static const char *resolve_test_log_output_format(void)
{
    const char *env_format = getenv("SENTIENCE_TEST_LOG_FORMAT");
    if (env_format && env_format[0]) {
        return env_format;
    }

    if (global_test_config && global_test_config->log_output_format && global_test_config->log_output_format[0]) {
        return global_test_config->log_output_format;
    }

    return "text";
}

static test_file_log_format_t parse_test_log_format(const char *format)
{
    if (format && (strcasecmp(format, "json") == 0 || strcasecmp(format, "jsonl") == 0)) {
        return TEST_FILE_LOG_JSON;
    }
    return TEST_FILE_LOG_TEXT;
}

static void close_test_file_log(void)
{
    if (test_file_log) {
        fclose(test_file_log);
        test_file_log = NULL;
    }
    test_file_log_path[0] = '\0';
}

static void write_test_file_log_text(const char *event, const char *payload)
{
    char ts[40];
    if (!test_file_log || !event) {
        return;
    }

    build_iso8601_timestamp(ts, sizeof(ts));
    fprintf(test_file_log, "[%s] event=%s", ts, event);
    if (payload && payload[0]) {
        fprintf(test_file_log, " %s", payload);
    }
    fputc('\n', test_file_log);
    fflush(test_file_log);
}

static void write_test_file_log_json(json_t *event)
{
    if (!test_file_log || !event) {
        return;
    }

    json_dumpf(event, test_file_log, JSON_COMPACT);
    fputc('\n', test_file_log);
    fflush(test_file_log);
}

static void configure_test_file_log(void)
{
    const char *output_file = resolve_test_log_output_file();
    const char *format = resolve_test_log_output_format();

    if (!output_file || !output_file[0]) {
        close_test_file_log();
        return;
    }

    test_file_log_format_t desired_format = parse_test_log_format(format);
    if (test_file_log
        && strcmp(test_file_log_path, output_file) == 0
        && test_file_log_format == desired_format) {
        return;
    }

    close_test_file_log();

    test_file_log = fopen(output_file, "a");
    if (!test_file_log) {
        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                      "Unable to open test log output file '%s'", output_file);
        return;
    }

    test_file_log_format = desired_format;
    snprintf(test_file_log_path, sizeof(test_file_log_path), "%s", output_file);

    if (test_file_log_format == TEST_FILE_LOG_JSON) {
        json_t *event = json_object();
        char ts[40];
        build_iso8601_timestamp(ts, sizeof(ts));
        json_object_set_new(event, "ts", json_string(ts));
        json_object_set_new(event, "event", json_string("session_start"));
        json_object_set_new(event, "format", json_string("json"));
        write_test_file_log_json(event);
        json_decref(event);
    } else {
        write_test_file_log_text("session_start", "format=text");
    }
}

void init_test_framework(void) {
    if (framework_initialized) {
        return;
    }
    
    test_suites = NULL;
    framework_initialized = true;

    configure_test_file_log();
    
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

    if (test_file_log_format == TEST_FILE_LOG_JSON && test_file_log) {
        json_t *event = json_object();
        char ts[40];
        build_iso8601_timestamp(ts, sizeof(ts));
        json_object_set_new(event, "ts", json_string(ts));
        json_object_set_new(event, "event", json_string("session_end"));
        write_test_file_log_json(event);
        json_decref(event);
    } else {
        write_test_file_log_text("session_end", NULL);
    }

    close_test_file_log();
    
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

/* json_get_string/int/bool now provided by json_common.h */

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

    if (test_file_log) {
        if (test_file_log_format == TEST_FILE_LOG_JSON) {
            char ts[40];
            json_t *event = json_object();
            build_iso8601_timestamp(ts, sizeof(ts));
            json_object_set_new(event, "ts", json_string(ts));
            json_object_set_new(event, "event", json_string("summary"));
            json_object_set_new(event, "total", json_integer(stats.total));
            json_object_set_new(event, "passed", json_integer(stats.passed));
            json_object_set_new(event, "failed", json_integer(stats.failed));
            json_object_set_new(event, "errors", json_integer(stats.errors));
            json_object_set_new(event, "skipped", json_integer(stats.skipped));
            write_test_file_log_json(event);
            json_decref(event);
        } else {
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "total=%d passed=%d failed=%d errors=%d skipped=%d",
                     stats.total, stats.passed, stats.failed, stats.errors, stats.skipped);
            write_test_file_log_text("summary", payload);
        }
    }
}

void test_log_suite_start(const char *suite_name)
{
    if (!test_file_log || !suite_name || !suite_name[0]) {
        return;
    }

    if (test_file_log_format == TEST_FILE_LOG_JSON) {
        char ts[40];
        json_t *event = json_object();
        build_iso8601_timestamp(ts, sizeof(ts));
        json_object_set_new(event, "ts", json_string(ts));
        json_object_set_new(event, "event", json_string("suite_start"));
        json_object_set_new(event, "suite", json_string(suite_name));
        write_test_file_log_json(event);
        json_decref(event);
    } else {
        char payload[320];
        snprintf(payload, sizeof(payload), "suite=%s", suite_name);
        write_test_file_log_text("suite_start", payload);
    }
}

void test_log_test_result(const test_case_t *test,
                          test_result_t actual_result,
                          double elapsed_seconds,
                          bool has_input,
                          bool has_expected_output,
                          bool expected_result_known,
                          test_result_t expected_result)
{
    if (!test_file_log || !test) {
        return;
    }

    if (test_file_log_format == TEST_FILE_LOG_JSON) {
        char ts[40];
        json_t *event = json_object();
        build_iso8601_timestamp(ts, sizeof(ts));

        json_object_set_new(event, "ts", json_string(ts));
        json_object_set_new(event, "event", json_string("test_result"));
        json_object_set_new(event, "test", json_string(test->name ? test->name : ""));
        json_object_set_new(event, "test_type", json_string(test->test_type ? test->test_type : ""));
        json_object_set_new(event, "result", json_string(test_result_to_string(actual_result)));
        json_object_set_new(event, "elapsed_seconds", json_real(elapsed_seconds));
        json_object_set_new(event, "has_input", json_boolean(has_input));
        json_object_set_new(event, "has_expected_output", json_boolean(has_expected_output));
        json_object_set_new(event, "expected_known", json_boolean(expected_result_known));
        if (expected_result_known) {
            json_object_set_new(event, "expected_result", json_string(test_result_to_string(expected_result)));
        }

        write_test_file_log_json(event);
        json_decref(event);
    } else {
        char payload[640];
        snprintf(payload, sizeof(payload),
                 "test=%s type=%s result=%s elapsed=%.6f input=%s expected_output=%s expected=%s",
                 test->name ? test->name : "",
                 test->test_type ? test->test_type : "",
                 test_result_to_string(actual_result),
                 elapsed_seconds,
                 has_input ? "present" : "missing",
                 has_expected_output ? "present" : "missing",
                 expected_result_known ? test_result_to_string(expected_result) : "unspecified");
        write_test_file_log_text("test_result", payload);
    }
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
    } else if (strcmp(pattern, "integration") == 0 ||
               strcmp(pattern, "unit") == 0 ||
               strcmp(pattern, "quick") == 0 ||
               strcmp(pattern, "full") == 0 ||
               strcmp(pattern, "default") == 0) {
        test_config_t *config = get_test_config();
        char **suite_names = NULL;
        int suite_name_count = 0;

        if (config) {
            if (strcmp(pattern, "integration") == 0) {
                suite_names = config->integration_only_suites;
                suite_name_count = config->integration_only_count;
            } else if (strcmp(pattern, "unit") == 0) {
                suite_names = config->unit_only_suites;
                suite_name_count = config->unit_only_count;
            } else if (strcmp(pattern, "quick") == 0) {
                suite_names = config->quick_test_suites;
                suite_name_count = config->quick_suite_count;
            } else if (strcmp(pattern, "full") == 0) {
                suite_names = config->full_test_suites;
                suite_name_count = config->full_suite_count;
            } else if (strcmp(pattern, "default") == 0) {
                suite_names = config->default_test_suites;
                suite_name_count = config->default_suite_count;
            }
        }

        if (suite_names && suite_name_count > 0) {
            for (int i = 0; i < suite_name_count; i++) {
                test_suite_t *suite = find_test_suite(suite_names[i]);
                if (suite) {
                    char reason[256];
                    snprintf(reason, sizeof(reason), "%s config", pattern);
                    mark_suite_selected(suite, suite_list, selected, suite_count, reasons, reason);
                }
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
    test_log_suite_start(suite->name);
    
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
        const char *file_type = test_json_get_string(root, "type");
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
    const char *version = test_json_get_string(test_config_obj, "version");
    if (version) global_test_config->version = strdup(version);
    
    const char *description = test_json_get_string(test_config_obj, "description");
    if (description) global_test_config->description = strdup(description);
    
    // Load settings
    json_t *settings = json_object_get(test_config_obj, "settings");
    if (settings) {
        global_test_config->default_timeout_seconds = test_json_get_int(settings, "default_timeout_seconds");
        global_test_config->stop_on_first_failure = test_json_get_bool(settings, "stop_on_first_failure");
        global_test_config->run_unit_tests_first = test_json_get_bool(settings, "run_unit_tests_first");
        global_test_config->require_mud_environment_for_integration = test_json_get_bool(settings, "require_mud_environment_for_integration");
        global_test_config->verbose_output = test_json_get_bool(settings, "verbose_output");
        global_test_config->verbose_test_names = test_json_get_bool(settings, "verbose_test_names");
        global_test_config->verbose_test_details = test_json_get_bool(settings, "verbose_test_details");
        global_test_config->show_test_config = test_json_get_bool(settings, "show_test_config");
        global_test_config->show_execution_time = test_json_get_bool(settings, "show_execution_time");

        const char *log_output_file = test_json_get_string(settings, "log_output_file");
        if (log_output_file && log_output_file[0]) {
            global_test_config->log_output_file = strdup(log_output_file);
        }

        const char *log_output_format = test_json_get_string(settings, "log_output_format");
        if (log_output_format && log_output_format[0]) {
            global_test_config->log_output_format = strdup(log_output_format);
        }
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
            
            const char *desc = test_json_get_string(profile_data, "description");
            if (desc) profile->description = strdup(desc);
            
            profile->stop_on_first_failure = test_json_get_bool(profile_data, "stop_on_first_failure");
            profile->require_coverage = test_json_get_bool(profile_data, "require_coverage");
            
            load_string_array(profile_data, "suites", &profile->test_suites, &profile->suite_count);
            profile_idx++;
        }
    }
    
    json_decref(root);
    configure_test_file_log();
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
    free(global_test_config->log_output_file);
    free(global_test_config->log_output_format);
    
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