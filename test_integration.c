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
#include "merc.h"
#include "log.h"
#include "tests/framework/test_framework.h"

int run_integration_tests(const char *pattern) {
    init_test_framework();
    
    // Load test suites from JSON files in data/tests
    if (!load_all_test_suites("data/tests")) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to load test suites from data/tests");
        cleanup_test_framework();
        return 1; // Error
    }
    
    // Validate dependencies before running
    if (!validate_dependencies()) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Test dependency validation failed");
        cleanup_test_framework();
        return 1; // Error
    }
    
    // Run tests (pattern or all)
    test_stats_t stats;
    if (pattern && strcmp(pattern, "all") != 0) {
        stats = run_tests_by_pattern(pattern);
    } else {
        stats = run_all_tests();
    }
    
    print_test_stats(stats);
    cleanup_test_framework();
    
    return stats.failed + stats.errors; // Return 0 for success, non-zero for failures
}

#endif // BUILD_TESTS