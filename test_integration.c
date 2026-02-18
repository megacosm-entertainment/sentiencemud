/***************************************************************************
 *  JSON-Driven Integration Test Framework for Sentience MUD
 *  
 *  Provides integration testing capabilities with JSON test definitions.
 *  Tests run with areas loaded, database initialized, and full game state.
 *  
 *  Only compiled when BUILD_TESTS is defined.
 ***************************************************************************/

#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "merc.h"
#include "log.h"
#include "tests/framework/test_framework.h"

static bool directory_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool file_exists_local(const char *path)
{
    return path && access(path, F_OK) == 0;
}

static const char *resolve_test_data_dir(char *buffer, size_t buffer_size)
{
    const char *env_dir = getenv("SENTIENCE_TEST_DATA_DIR");
    if (env_dir && directory_exists(env_dir)) {
        snprintf(buffer, buffer_size, "%s", env_dir);
        return buffer;
    }

    if (directory_exists("data/tests")) {
        snprintf(buffer, buffer_size, "%s", "data/tests");
        return buffer;
    }

    if (directory_exists("src/tests/data")) {
        snprintf(buffer, buffer_size, "%s", "src/tests/data");
        return buffer;
    }

    if (directory_exists("/sentience/src/tests/data")) {
        snprintf(buffer, buffer_size, "%s", "/sentience/src/tests/data");
        return buffer;
    }

    return NULL;
}

static const char *resolve_test_config_path(const char *test_data_dir, char *buffer, size_t buffer_size)
{
    const char *env_cfg = getenv("SENTIENCE_TEST_CONFIG");
    if (env_cfg && file_exists_local(env_cfg)) {
        snprintf(buffer, buffer_size, "%s", env_cfg);
        return buffer;
    }

    if (test_data_dir && test_data_dir[0]) {
        snprintf(buffer, buffer_size, "%s/test_config.json", test_data_dir);
        if (file_exists_local(buffer)) {
            return buffer;
        }
    }

    return NULL;
}

int run_integration_tests(const char *pattern) {
    init_test_framework();

    char test_data_dir[512];
    char test_config_path[512];
    const char *resolved_test_data_dir = resolve_test_data_dir(test_data_dir, sizeof(test_data_dir));
    const char *resolved_test_config_path = resolve_test_config_path(resolved_test_data_dir, test_config_path, sizeof(test_config_path));

    if (resolved_test_data_dir) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Using test data directory: %s", resolved_test_data_dir);
    } else {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No test data directory found (checked SENTIENCE_TEST_DATA_DIR, data/tests, src/tests/data)");
        cleanup_test_framework();
        return 1;
    }
    
    // Load test configuration
    if (!resolved_test_config_path || !load_test_config(resolved_test_config_path)) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "Failed to load test config, using built-in defaults");
    }
    
    // Load test suites from resolved JSON test data directory
    set_test_loader_logging(false);
    if (!load_all_test_suites(resolved_test_data_dir)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to load test suites from %s", resolved_test_data_dir);
        cleanup_test_framework();
        return 1; // Error
    }
    log_selected_test_suites(pattern);
    
    // Handle special registry commands
    if (pattern) {
        if (strcmp(pattern, "registry") == 0) {
            print_test_registry();
            cleanup_test_framework();
            return 0;
        } else if (strcmp(pattern, "summary") == 0) {
            print_test_suites_summary();
            cleanup_test_framework();
            return 0;
        } else if (strcmp(pattern, "config") == 0) {
            test_config_t *config = get_test_config();
            if (config) {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test Configuration: %s", config->description);
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Version: %s", config->version);
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Default test suites: %d", config->default_suite_count);
                for (int i = 0; i < config->default_suite_count; i++) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  - %s", config->default_test_suites[i]);
                }
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Available profiles: %d", config->profile_count);
                for (int i = 0; i < config->profile_count; i++) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  - %s: %s", 
                                 config->profiles[i].name, config->profiles[i].description);
                }
            } else {
                log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No test configuration loaded");
            }
            cleanup_test_framework();
            return 0;
        }
    }
    
    // Check for test profile patterns
    if (pattern) {
        if (strncmp(pattern, "profile:", 8) == 0) {
            const char *profile_name = pattern + 8;
            test_profile_t *profile = find_test_profile(profile_name);
            if (profile) {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Running test profile: %s", profile->description);
                // Run tests from this profile
                test_stats_t stats = {0};
                for (int i = 0; i < profile->suite_count; i++) {
                    test_stats_t suite_stats = run_tests_by_pattern(profile->test_suites[i]);
                    stats.total += suite_stats.total;
                    stats.passed += suite_stats.passed;
                    stats.failed += suite_stats.failed;
                    stats.errors += suite_stats.errors;
                    stats.skipped += suite_stats.skipped;
                    
                    if (profile->stop_on_first_failure && (suite_stats.failed > 0 || suite_stats.errors > 0)) {
                        log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Stopping on first failure as configured in profile");
                        break;
                    }
                }
                print_test_stats(stats);
                cleanup_test_framework();
                return stats.failed + stats.errors;
            } else {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test profile '%s' not found", profile_name);
                cleanup_test_framework();
                return 1;
            }
        } else if (strncmp(pattern, "test:", 5) == 0) {
            // Run specific tests by name (comma-separated)
            const char *test_names_str = pattern + 5;
            char *test_names_copy = strdup(test_names_str);
            char *saveptr;
            char *test_name;
            
            // Count tests
            int test_count = 1;
            for (char *c = test_names_copy; *c; c++) {
                if (*c == ',') test_count++;
            }
            
            // Parse test names
            char **test_names = malloc(sizeof(char*) * test_count);
            test_name = strtok_r(test_names_copy, ",", &saveptr);
            int idx = 0;
            while (test_name && idx < test_count) {
                // Trim whitespace
                while (*test_name == ' ') test_name++;
                test_names[idx] = strdup(test_name);
                test_name = strtok_r(NULL, ",", &saveptr);
                idx++;
            }
            
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Running specific tests (%d tests)", idx);
            test_stats_t stats = run_specific_tests(test_names, idx);
            
            // Cleanup
            for (int i = 0; i < idx; i++) {
                free(test_names[i]);
            }
            free(test_names);
            free(test_names_copy);
            
            print_test_stats(stats);
            cleanup_test_framework();
            return stats.failed + stats.errors;
        }
    }
    
    // Validate dependencies before running
    if (!validate_dependencies()) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Test dependency validation failed");
        cleanup_test_framework();
        return 1; // Error
    }
    
    // Run tests
    test_stats_t stats;
    if (pattern && strcmp(pattern, "all") != 0) {
        // Run by specific pattern
        stats = run_tests_by_pattern(pattern);
    } else if (pattern && strcmp(pattern, "all") == 0) {
        // Run all tests explicitly
        stats = run_all_tests();
    } else {
        // No pattern specified - use default test suites from config
        test_config_t *config = get_test_config();
        if (config && config->default_suite_count > 0) {
            log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, "No pattern specified, running default test suites (%d suites)", 
                         config->default_suite_count);
            
            stats = (test_stats_t){0};
            for (int i = 0; i < config->default_suite_count; i++) {
                log_message_f(LOG_LEVEL_INFO, LOG_DEBUG, "Running default suite: %s", config->default_test_suites[i]);
                test_stats_t suite_stats = run_tests_by_pattern(config->default_test_suites[i]);
                stats.total += suite_stats.total;
                stats.passed += suite_stats.passed;
                stats.failed += suite_stats.failed;
                stats.errors += suite_stats.errors;
                stats.skipped += suite_stats.skipped;
                
                if (config->stop_on_first_failure && (suite_stats.failed > 0 || suite_stats.errors > 0)) {
                    log_message(LOG_LEVEL_INFO, LOG_DEBUG, "Stopping on first failure as configured");
                    break;
                }
            }
        } else {
            // Fallback to all tests if no config
            log_message(LOG_LEVEL_INFO, LOG_DEBUG, "No default test suites configured, running all tests");
            stats = run_all_tests();
        }
    }
    
    print_test_stats(stats);
    cleanup_test_framework();
    
    return stats.failed + stats.errors; // Return 0 for success, non-zero for failures
}

#endif // BUILD_TESTS