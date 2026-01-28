/***************************************************************************
 *  Integration Test Framework for Sentience MUD
 *  
 *  Provides integration testing capabilities within the full MUD environment.
 *  Tests run with areas loaded, database initialized, and full game state.
 *  
 *  Only compiled when BUILD_TESTS is defined.
 ***************************************************************************/

#ifdef BUILD_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "merc.h"
#include "log.h"

// Test case structure
typedef struct test_case {
    const char *name;
    const char *category;
    int (*test_func)(void);
    const char *description;
} TEST_CASE;

// Forward declarations for test functions
int test_wnum_integration(void);
int test_area_loading(void);
int test_database_integrity(void);

// Test registry
static const TEST_CASE test_registry[] = {
    { "wnum_integration", "unit", test_wnum_integration, "Test WNUM parsing with real areas" },
    { "area_loading", "integration", test_area_loading, "Test area loading and integrity" },
    { "database_integrity", "integration", test_database_integrity, "Test database consistency" },
    { NULL, NULL, NULL, NULL }  // Sentinel
};

/*
 * Run integration tests with pattern matching
 * Returns number of failed tests (0 = success)
 */
int run_integration_tests(const char *pattern) {
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Running integration tests (pattern: %s)", 
                  pattern ? pattern : "all");
    
    for (const TEST_CASE *test = test_registry; test->name; test++) {
        // Check if test matches pattern
        bool should_run = false;
        
        if (!pattern || !strcmp(pattern, "all")) {
            should_run = true;
        } else if (!strcmp(pattern, test->category)) {
            should_run = true;
        } else if (strstr(test->name, pattern)) {
            should_run = true;
        }
        
        if (!should_run) continue;
        
        total_tests++;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Running test: %s (%s)", 
                      test->name, test->description);
        
        int result = test->test_func();
        
        if (result == 0) {
            passed_tests++;
            log_message_f(LOG_LEVEL_INFO, LOG_INFO, "  PASS: %s", test->name);
        } else {
            failed_tests++;
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "  FAIL: %s (code: %d)", test->name, result);
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, 
                  "Test results: %d total, %d passed, %d failed", 
                  total_tests, passed_tests, failed_tests);
    
    return failed_tests; // Return 0 for success, non-zero for failures
}

// Test implementations

int test_wnum_integration(void) {
    // Test WNUM parsing with actual loaded areas
    AREA_DATA *realm = find_area("Realm of Alendith");
    if (!realm) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Could not find Realm of Alendith area");
        return 1;
    }
    
    // Test parsing with real area (using actual vnum range 3500-3799)
    WNUM result;
    if (!parse_widevnum("3501", realm, &result)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "WNUM parsing failed for simple vnum");
        return 2;
    }
    
    if (result.pArea != realm || result.vnum != 3501) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                      "WNUM parsing failed: expected area=%p vnum=3501, got area=%p vnum=%ld",
                      (void*)realm, (void*)result.pArea, result.vnum);
        return 3;
    }
    
    // Test area name format
    if (!parse_widevnum("'Realm of Alendith'#3650", NULL, &result)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "WNUM area name parsing failed");
        return 4;
    }
    
    if (result.pArea != realm || result.vnum != 3650) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Area name parsing failed: expected area=%p vnum=3650, got area=%p vnum=%ld", 
                      (void*)realm, (void*)result.pArea, result.vnum);
        return 5;
    }
    
    return 0; // Success
}

int test_area_loading(void) {
    // Test that essential areas are loaded
    const char *required_areas[] = { "Limbo", "Realm of Alendith", NULL };
    
    for (int i = 0; required_areas[i]; i++) {
        AREA_DATA *area = find_area(required_areas[i]);
        if (!area) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Required area not found: %s", required_areas[i]);
            return 1;
        }
        
        // Check area has valid UID
        if (area->uid <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Area %s has invalid UID: %ld", 
                          required_areas[i], area->uid);
            return 2;
        }
        
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "Area %s loaded (UID: %ld)", 
                      required_areas[i], area->uid);
    }
    
    return 0; // Success
}

int test_database_integrity(void) {
    // Test basic database integrity
    if (gconfig.next_area_uid <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid next_area_uid: %ld", gconfig.next_area_uid);
        return 1;
    }
    
    // Count areas and verify they have unique UIDs
    long uid_count[1000] = {0}; // Simple check for first 1000 UIDs
    int area_count = 0;
    
    for (AREA_DATA *area = area_first; area; area = area->next) {
        area_count++;
        
        if (area->uid > 0 && area->uid < 1000) {
            if (uid_count[area->uid]++ > 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Duplicate area UID detected: %ld", area->uid);
                return 2;
            }
        }
    }
    
    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "Database integrity check passed (%d areas)", area_count);
    return 0; // Success
}

#endif // BUILD_TESTS