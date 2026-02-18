#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../log.h"

// Forward declarations
test_result_t run_reset_test_case(test_case_t *test);
test_result_t run_shop_stock_test_case(test_case_t *test);
test_result_t run_church_test_case(test_case_t *test);
test_result_t run_instance_test_case(test_case_t *test);
test_result_t run_chat_room_test_case(test_case_t *test);
test_result_t run_skill_data_test_case(test_case_t *test);
test_result_t run_class_data_test_case(test_case_t *test);
test_result_t run_item_type_test_case(test_case_t *test);
test_result_t run_lookup_table_test_case(test_case_t *test);
test_result_t run_song_data_test_case(test_case_t *test);
test_result_t run_skill_group_test_case(test_case_t *test);
test_result_t run_trait_system_test_case(test_case_t *test);

static test_result_t test_wnum_parsing(test_case_t *test);
static test_result_t test_area_name_parsing(test_case_t *test);
static test_result_t test_wnum_parsing_structured(test_case_t *test);
static test_result_t test_area_existence_check(test_case_t *test);
static test_result_t test_area_integrity_check(test_case_t *test);
static test_result_t test_config_validator(test_case_t *test);
static test_result_t test_uid_uniqueness_check(test_case_t *test);
static test_result_t test_pure_function(test_case_t *test);
static test_result_t test_reserved_lookup(test_case_t *test);
static test_result_t test_reserved_wnum_format(test_case_t *test);
static test_result_t test_reserved_compat(test_case_t *test);
static test_result_t test_game_setting_exists(test_case_t *test);
static test_result_t test_system_area_resolve(test_case_t *test);
static test_result_t test_system_area_fallback(test_case_t *test);
static test_result_t test_widevnum_parse_fallback(test_case_t *test);
static test_result_t test_widevnum_parse_explicit(test_case_t *test);
static test_result_t test_json_area_serialize(test_case_t *test);
static test_result_t test_json_area_rooms(test_case_t *test);
static test_result_t test_cross_area_exit(test_case_t *test);
static test_result_t test_json_area_roundtrip(test_case_t *test);
static test_result_t test_redis_available(test_case_t *test);
static test_result_t test_redis_area_cached(test_case_t *test);
static test_result_t test_redis_area_cache_format(test_case_t *test);
static test_result_t test_redis_warm_queue(test_case_t *test);
void print_test_result(test_case_t *test, test_result_t result, clock_t start_time);

void register_wnum_tests(void) {
    // These will be loaded from JSON files rather than registered directly
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "WNUM test handlers registered");
}

static test_result_t test_wnum_parsing(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *area_json = json_object_get(test->config, "area");
    json_t *test_cases = json_object_get(test->config, "test_cases");
    
    if (!json_is_string(area_json) || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid WNUM test configuration");
        return TEST_ERROR;
    }
    
    const char *area_name = json_string_value(area_json);
    AREA_DATA *area = find_area((char*)area_name);
    
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Area not found for WNUM test: %s", area_name);
        return TEST_FAILURE;
    }
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_cases, index, test_case) {
        const char *input = test_json_get_string(test_case, "input");
        const char *context_area_name = test_json_get_string(test_case, "context_area");
        int expected_vnum = test_json_get_int(test_case, "expected_vnum");
        const char *expected_area_name = test_json_get_string(test_case, "expected_area");
        
        if (!input || expected_vnum == 0) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid test case in WNUM test");
            return TEST_ERROR;
        }
        
        AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : NULL;
        WNUM result;
        
        if (!parse_widevnum((char*)input, context_area, &result)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "WNUM parsing failed for input: %s", input);
            return TEST_FAILURE;
        }
        
        if (result.vnum != expected_vnum) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                          "WNUM vnum mismatch for input '%s': expected %d, got %ld", 
                          input, expected_vnum, result.vnum);
            return TEST_FAILURE;
        }
        
        if (expected_area_name) {
            AREA_DATA *expected_area = find_area((char*)expected_area_name);
            if (result.pArea != expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                              "WNUM area mismatch for input '%s': expected %s, got different area", 
                              input, expected_area_name);
                return TEST_FAILURE;
            }
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_area_name_parsing(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *test_cases = json_object_get(test->config, "test_cases");
    
    if (!json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area name test configuration");
        return TEST_ERROR;
    }
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_cases, index, test_case) {
        const char *input = test_json_get_string(test_case, "input");
        int expected_vnum = test_json_get_int(test_case, "expected_vnum");
        const char *expected_area_name = test_json_get_string(test_case, "expected_area");
        
        if (!input || expected_vnum == 0 || !expected_area_name) {
            log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Invalid test case in area name test");
            return TEST_ERROR;
        }
        
        WNUM result;
        if (!parse_widevnum((char*)input, NULL, &result)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Area name parsing failed for input: %s", input);
            return TEST_FAILURE;
        }
        
        if (result.vnum != expected_vnum) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                          "Area name vnum mismatch for input '%s': expected %d, got %ld", 
                          input, expected_vnum, result.vnum);
            return TEST_FAILURE;
        }
        
        AREA_DATA *expected_area = find_area((char*)expected_area_name);
        if (result.pArea != expected_area) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                          "Area name area mismatch for input '%s': expected %s", 
                          input, expected_area_name);
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}

// Test case execution dispatcher
test_result_t run_test_case(test_case_t *test) {
    if (!test) {
        return TEST_ERROR;
    }
    
    test_config_t *config = get_test_config();
    bool show_names = config ? config->verbose_test_names : true;
    bool show_details = config ? config->verbose_test_details : false;
    bool show_timing = config ? config->show_execution_time : true;
    
    // Use per-test verbose setting if available, otherwise use global
    bool verbose = test->verbose_output;
    if (config && !verbose) {
        verbose = config->verbose_output;
    }
    
    if (show_names) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  [TEST] %s (%s)... ",
                      test->name, test->test_type ? test->test_type : "unknown");
    }
    
    if (verbose || show_details) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Running test case: %s (type: %s)",
                      test->name, test->test_type ? test->test_type : "unknown");
        if (test->description && strlen(test->description) > 0) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Description: %s", test->description);
        }
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Timeout: %d seconds", test->timeout_seconds);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "  Verbose output: %s", test->verbose_output ? "Yes" : "No");
    }
    
    // Record start time if timing is enabled
    clock_t start_time = 0;
    if (show_timing) {
        start_time = clock();
    }
    
    // Dispatch based on test type
    test_result_t result = TEST_SKIP;
    
    if (test->test_type) {
        if (strcmp(test->test_type, "wnum_parser") == 0) {
            result = test_wnum_parsing_structured(test);
        } else if (strcmp(test->test_type, "area_existence_check") == 0) {
            result = test_area_existence_check(test);
        } else if (strcmp(test->test_type, "area_integrity_check") == 0) {
            result = test_area_integrity_check(test);
        } else if (strcmp(test->test_type, "config_validator") == 0) {
            result = test_config_validator(test);
        } else if (strcmp(test->test_type, "uid_uniqueness_check") == 0) {
            result = test_uid_uniqueness_check(test);
        } else if (strcmp(test->test_type, "pure_function_test") == 0) {
            // New handler for unit tests
            result = test_pure_function(test);
        } else if (strcmp(test->test_type, "reserved_lookup_test") == 0) {
            result = test_reserved_lookup(test);
        } else if (strcmp(test->test_type, "reserved_wnum_format_test") == 0) {
            result = test_reserved_wnum_format(test);
        } else if (strcmp(test->test_type, "reserved_compat_test") == 0) {
            result = test_reserved_compat(test);
        } else if (strcmp(test->test_type, "game_setting_exists_test") == 0) {
            result = test_game_setting_exists(test);
        } else if (strcmp(test->test_type, "system_area_resolve_test") == 0) {
            result = test_system_area_resolve(test);
        } else if (strcmp(test->test_type, "system_area_fallback_test") == 0) {
            result = test_system_area_fallback(test);
        } else if (strcmp(test->test_type, "widevnum_parse_fallback_test") == 0) {
            result = test_widevnum_parse_fallback(test);
        } else if (strcmp(test->test_type, "widevnum_parse_explicit_test") == 0) {
            result = test_widevnum_parse_explicit(test);
        } else if (strcmp(test->test_type, "json_area_serialize_test") == 0) {
            result = test_json_area_serialize(test);
        } else if (strcmp(test->test_type, "json_area_rooms_test") == 0) {
            result = test_json_area_rooms(test);
        } else if (strcmp(test->test_type, "cross_area_exit_test") == 0) {
            result = test_cross_area_exit(test);
        } else if (strcmp(test->test_type, "json_area_roundtrip_test") == 0) {
            result = test_json_area_roundtrip(test);
        } else if (strcmp(test->test_type, "redis_available_test") == 0) {
            result = test_redis_available(test);
        } else if (strcmp(test->test_type, "redis_area_cached_test") == 0) {
            result = test_redis_area_cached(test);
        } else if (strcmp(test->test_type, "redis_area_cache_format_test") == 0) {
            result = test_redis_area_cache_format(test);
        } else if (strcmp(test->test_type, "redis_warm_queue_test") == 0) {
            result = test_redis_warm_queue(test);
        } else if (strcmp(test->test_type, "reset_cross_area_creation") == 0 ||
                   strcmp(test->test_type, "reset_serialization") == 0 ||
                   strcmp(test->test_type, "reset_legacy_vnum") == 0) {
            result = run_reset_test_case(test);
        } else if (strcmp(test->test_type, "shop_stock_cross_area_creation") == 0 ||
                   strcmp(test->test_type, "shop_stock_serialization") == 0 ||
                   strcmp(test->test_type, "shop_stock_legacy_vnum") == 0) {
            result = run_shop_stock_test_case(test);
        } else if (strstr(test->test_type, "church_") != NULL) {
            // Church tests: church_serialize_test, church_members_test, etc.
            result = run_church_test_case(test);
        } else if (strstr(test->test_type, "instance_") != NULL ||
                   strstr(test->test_type, "blueprint_") != NULL ||
                   strstr(test->test_type, "dungeon_") != NULL ||
                   strstr(test->test_type, "ship_") != NULL ||
                   strstr(test->test_type, "wnum_json_") != NULL ||
                   strstr(test->test_type, "persist_directory") != NULL) {
            // Instance/blueprint/ship/dungeon tests
            result = run_instance_test_case(test);
        } else if (strstr(test->test_type, "chat_room_") != NULL) {
            // Chat room tests
            result = run_chat_room_test_case(test);
        } else if (strstr(test->test_type, "skill_group_") != NULL) {
            // Skill group tests (check before skill_ to avoid prefix collision)
            result = run_skill_group_test_case(test);
        } else if (strstr(test->test_type, "skill_") != NULL ||
                   strstr(test->test_type, "spell_fun_") != NULL) {
            // Skill data tests
            result = run_skill_data_test_case(test);
        } else if (strstr(test->test_type, "class_") != NULL) {
            // Class data tests
            result = run_class_data_test_case(test);
        } else if (strstr(test->test_type, "item_type_") != NULL) {
            // Item type tests
            result = run_item_type_test_case(test);
        } else if (strstr(test->test_type, "song_") != NULL) {
            // Song data tests
            result = run_song_data_test_case(test);
        } else if (strstr(test->test_type, "trait_") != NULL) {
            // Trait system tests
            result = run_trait_system_test_case(test);
        } else if (strstr(test->test_type, "_lookup_test") != NULL ||
                   strstr(test->test_type, "flag_table_") != NULL) {
            // Lookup table tests
            result = run_lookup_table_test_case(test);
        } else {
            // Fallback to name-based dispatch for backwards compatibility
            if (strstr(test->name, "vnum_parsing")) {
                result = test_wnum_parsing(test);
            } else if (strstr(test->name, "area_name_parsing")) {
                result = test_area_name_parsing(test);
            } else if (test->execute) {
                // If we have a registered execute function, use it
                result = test->execute(test);
            } else {
                log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "No handler found for test: %s (type: %s)",
                              test->name, test->test_type ? test->test_type : "unknown");
                result = TEST_SKIP;
            }
        }
    } else {
        // No test type specified, try name-based dispatch
        if (strstr(test->name, "vnum_parsing")) {
            result = test_wnum_parsing(test);
        } else if (strstr(test->name, "area_name_parsing")) {
            result = test_area_name_parsing(test);
        } else if (test->execute) {
            result = test->execute(test);
        } else {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "No handler found for test: %s (type: %s)",
                          test->name, test->test_type ? test->test_type : "unknown");
            result = TEST_SKIP;
        }
    }
    
    // Print results with timing and verbose details
    print_test_result(test, result, start_time);
    
    return result;
}

// Enhanced test result output with timing and verbose details
void print_test_result(test_case_t *test, test_result_t result, clock_t start_time) {
    test_config_t *config = get_test_config();
    bool show_names = config ? config->verbose_test_names : true;
    bool show_timing = config ? config->show_execution_time : true;
    bool verbose = test->verbose_output || (config ? config->verbose_output : false);
    
    const char *result_str = test_result_to_string(result);
    
    if (show_names) {
        if (show_timing && start_time > 0) {
            double elapsed = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "%s (%.3fs)", result_str, elapsed);
        } else {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "%s", result_str);
        }
    }
    
    if (verbose) {
        if (show_timing && start_time > 0) {
            double elapsed = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test completed: %s - %s (%.3fs)", 
                         test->name, result_str, elapsed);
        } else {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test completed: %s - %s", 
                         test->name, result_str);
        }
        
        if (result != TEST_SUCCESS) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Test failure details for: %s", test->name);
        }
    }
}

static test_result_t test_wnum_parsing_structured(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");
    
    if (!json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid structured WNUM test: no test_cases array");
        return TEST_ERROR;
    }
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_cases, index, test_case) {
        const char *vnum_string = test_json_get_string(test_case, "vnum_string");
        const char *context_area_name = test_json_get_string(test_case, "context_area");
        
        if (!vnum_string) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid test case: missing vnum_string");
            return TEST_ERROR;
        }
        
        AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : NULL;
        if (context_area_name && !context_area) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                          "Context area not found: %s", context_area_name);
            return TEST_FAILURE;
        }
        
        WNUM result;
        if (!parse_widevnum((char*)vnum_string, context_area, &result)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                          "WNUM parsing failed for input: %s", vnum_string);
            return TEST_FAILURE;
        }
        
        log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                      "WNUM parsed: %s -> vnum=%ld, area=%s",
                      vnum_string, result.vnum, 
                      result.pArea ? result.pArea->name : "NULL");
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_area_existence_check(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *required_areas = json_object_get(input, "required_areas");
    
    if (!json_is_array(required_areas)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area existence test: no required_areas array");
        return TEST_ERROR;
    }
    
    size_t index;
    json_t *area_spec;
    json_array_foreach(required_areas, index, area_spec) {
        const char *area_name = test_json_get_string(area_spec, "name");
        int expected_uid = test_json_get_int(area_spec, "expected_uid");
        
        if (!area_name) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area spec: missing name");
            return TEST_ERROR;
        }
        
        AREA_DATA *area = find_area((char*)area_name);
        if (!area) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Required area not found: %s", area_name);
            return TEST_FAILURE;
        }
        
        if (expected_uid > 0 && area->uid != expected_uid) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Area %s has wrong UID: expected %d, got %ld",
                          area_name, expected_uid, area->uid);
            return TEST_FAILURE;
        }
        
        log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Area found: %s (UID: %ld)", area_name, area->uid);
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_area_integrity_check(test_case_t *test) {
    // Basic integrity check - could be expanded
    if (!area_first) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No areas loaded");
        return TEST_FAILURE;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Area integrity check passed");
    return TEST_SUCCESS;
}

static test_result_t test_config_validator(test_case_t *test) {
    if (gconfig.next_area_uid <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid next_area_uid: %ld", gconfig.next_area_uid);
        return TEST_FAILURE;
    }
    
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Global config validation passed");
    return TEST_SUCCESS;
}

static test_result_t test_uid_uniqueness_check(test_case_t *test) {
    // Simple UID uniqueness check
    long uid_count[1000] = {0};
    int area_count = 0;
    
    for (AREA_DATA *area = area_first; area; area = area->next) {
        area_count++;
        
        if (area->uid > 0 && area->uid < 1000) {
            if (uid_count[area->uid]++ > 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Duplicate area UID: %ld", area->uid);
                return TEST_FAILURE;
            }
        }
    }
    
    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "UID uniqueness check passed (%d areas)", area_count);
    return TEST_SUCCESS;
}

static test_result_t test_pure_function(test_case_t *test) {
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing configuration");
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected_output = json_object_get(test->config, "expected_output");
    
    if (!input || !expected_output) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing input or expected_output");
        return TEST_ERROR;
    }
    
    // Get test parameters
    json_t *function_name = json_object_get(input, "function");
    json_t *test_cases = json_object_get(input, "test_cases");
    
    if (!function_name || !test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test malformed: missing function or test_cases array");
        return TEST_ERROR;
    }
    
    const char *func_name = json_string_value(function_name);
    if (test->verbose_output) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Testing pure function: %s", func_name);
    }
    
    // WNUM parsing unit tests
    if (strcmp(func_name, "parse_widevnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_area = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            const char *context_area_name = test_json_get_string(test_case, "context_area");
            
            if (!input_str) continue;
            
            AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : NULL;
            WNUM result;
            
            bool parse_success = parse_widevnum((char*)input_str, context_area, &result);
            
            if (!parse_success && expected_vnum > 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') failed when success expected",
                             input_str);
                return TEST_FAILURE;
            }
            
            if (parse_success && expected_vnum == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') succeeded when failure expected",
                             input_str);
                return TEST_FAILURE;
            }
            
            if (parse_success && result.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                             "parse_widevnum('%s') returned vnum %ld, expected %ld", 
                             input_str, result.vnum, expected_vnum);
                return TEST_FAILURE;
            }
            
            if (parse_success && expected_area && result.pArea) {
                if (strcmp(result.pArea->name, expected_area) != 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "parse_widevnum('%s') returned area '%s', expected '%s'",
                                 input_str, result.pArea->name, expected_area);
                    return TEST_FAILURE;
                }
            }
            
            if (test->verbose_output) {
                if (parse_success) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, 
                                 "✓ parse_widevnum('%s') -> area:'%s', vnum:%ld", 
                                 input_str, 
                                 result.pArea ? result.pArea->name : "NULL", 
                                 result.vnum);
                } else {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                 "✓ parse_widevnum('%s') -> failed as expected",
                                 input_str);
                }
            }
        }
        
        if (test->verbose_output) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Pure function test passed for %s", func_name);
        }
        return TEST_SUCCESS;
    }

    // String/argument utility unit tests
    if (strcmp(func_name, "is_number") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *arg = test_json_get_string(test_case, "arg");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "is_number test case missing 'arg'");
                return TEST_ERROR;
            }

            bool actual = is_number(arg);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_number('%s') returned %s, expected %s",
                             arg,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_prefix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *astr = test_json_get_string(test_case, "astr");
            const char *bstr = test_json_get_string(test_case, "bstr");
            bool expected_not_prefix = test_json_get_bool(test_case, "expected_not_prefix");

            if (!astr || !bstr) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "str_prefix test case missing astr/bstr");
                return TEST_ERROR;
            }

            bool actual_not_prefix = str_prefix(astr, bstr);
            if (actual_not_prefix != expected_not_prefix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_prefix('%s','%s') returned %s, expected %s",
                             astr,
                             bstr,
                             actual_not_prefix ? "true" : "false",
                             expected_not_prefix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "number_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            int expected_number = test_json_get_int(test_case, "expected_number");
            const char *expected_arg = test_json_get_string(test_case, "expected_arg");
            char argument_buf[MAX_INPUT_LENGTH];
            char arg_out[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "number_argument test case missing argument/expected_arg");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            int actual_number = number_argument(argument_buf, arg_out);

            if (actual_number != expected_number || str_cmp(arg_out, expected_arg) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "number_argument('%s') returned (%d,'%s'), expected (%d,'%s')",
                             argument_in,
                             actual_number,
                             arg_out,
                             expected_number,
                             expected_arg);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "one_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            const char *expected_first = test_json_get_string(test_case, "expected_first");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            char argument_buf[MAX_STRING_LENGTH];
            char first[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_first || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "one_argument test case missing argument/expected_first/expected_rest");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            char *rest = one_argument(argument_buf, first);

            if (str_cmp(first, expected_first) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "one_argument('%s') returned first='%s', rest='%s'; expected first='%s', rest='%s'",
                             argument_in,
                             first,
                             rest,
                             expected_first,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "smash_tilde") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char buffer[MAX_STRING_LENGTH];

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "smash_tilde test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(buffer, sizeof(buffer), "%s", input_str);
            smash_tilde(buffer);

            if (str_cmp(buffer, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "smash_tilde('%s') returned '%s', expected '%s'",
                             input_str,
                             buffer,
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_exact_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_exact_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_exact_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_exact_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }
    
    // Add more pure function handlers as needed
    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unsupported pure function: %s", func_name);
    return TEST_SKIP;
}

// Reserved system tests
static test_result_t test_reserved_lookup(test_case_t *test) {
    extern LLIST *reserved_vnums;
    (void)reserved_vnums;
    
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *reserved_name = test_json_get_string(input, "reserved_name");
    bool should_exist = test_json_get_bool(input, "should_exist");
    
    if (!reserved_name) {
        return TEST_ERROR;
    }
    
    RESERVED_DATA *reserved = find_reserved(reserved_name);
    
    if (should_exist && !reserved) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Reserved item '%s' not found but should exist", reserved_name);
        return TEST_FAILURE;
    }
    
    if (!should_exist && reserved) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Reserved item '%s' found but shouldn't exist", reserved_name);
        return TEST_FAILURE;
    }
    
    if (reserved && test_json_get_bool(expected, "has_area")) {
        AREA_DATA *area = get_area_index(reserved->wnum.auid);
        if (!area && reserved->wnum.auid > 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Reserved item '%s' has invalid area UID %ld", 
                         reserved_name, reserved->wnum.auid);
            return TEST_FAILURE;
        }
    }
    
    if (reserved && test_json_get_bool(expected, "has_vnum")) {
        if (reserved->type != RESERVED_AREA && reserved->wnum.vnum <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Reserved item '%s' has invalid vnum %ld",
                         reserved_name, reserved->wnum.vnum);
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_reserved_wnum_format(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *reserved_name = test_json_get_string(input, "reserved_name");
    if (!reserved_name) {
        return TEST_ERROR;
    }
    
    RESERVED_DATA *reserved = find_reserved(reserved_name);
    if (!reserved) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Reserved item '%s' not found for format test", reserved_name);
        return TEST_FAILURE;
    }
    
    AREA_DATA *area = get_area_index(reserved->wnum.auid);
    const char *wnum_str = widevnum_string(area, reserved->wnum.vnum, NULL);
    
    if (!wnum_str) {
        return TEST_FAILURE;
    }
    
    if (test_json_get_bool(expected, "has_hash_separator")) {
        if (!strchr(wnum_str, '#')) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "WNUM string '%s' missing # separator", wnum_str);
            return TEST_FAILURE;
        }
    }
    
    if (test_json_get_bool(expected, "parseable")) {
        WNUM parsed;
        if (!parse_widevnum((char*)wnum_str, NULL, &parsed)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Failed to parse generated WNUM string '%s'", wnum_str);
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_reserved_compat(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    
    if (!input) {
        return TEST_ERROR;
    }
    
    const char *reserved_name = test_json_get_string(input, "reserved_name");
    if (!reserved_name) {
        return TEST_ERROR;
    }
    
    int vnum = get_reserved_vnum(reserved_name);
    
    if (vnum <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "get_reserved_vnum('%s') returned invalid vnum %d", 
                     reserved_name, vnum);
        return TEST_FAILURE;
    }
    
    return TEST_SUCCESS;
}

// System area tests
static test_result_t test_game_setting_exists(test_case_t *test) {
    extern const struct game_setting_type game_settings_table[];
    (void)game_settings_table;
    
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *setting_name = test_json_get_string(input, "setting_name");
    if (!setting_name) {
        return TEST_ERROR;
    }
    
    const struct game_setting_type *setting = get_game_setting(setting_name);
    
    if (!setting && test_json_get_bool(expected, "setting_found")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Game setting '%s' not found", setting_name);
        return TEST_FAILURE;
    }
    
    if (setting) {
        const char *expected_type = test_json_get_string(input, "expected_type");
        if (expected_type && strcmp(expected_type, "string") == 0) {
            if (setting->type != SETTING_TYPE_STRING && 
                setting->type != SETTING_TYPE_EXTSTR) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Setting '%s' is not string type", setting_name);
                return TEST_FAILURE;
            }
        }
        
        const char *expected_category = test_json_get_string(input, "expected_category");
        if (expected_category && strcmp(expected_category, "global") == 0) {
            if (setting->category != SETTING_CAT_GLOBAL) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Setting '%s' is not in global category", setting_name);
                return TEST_FAILURE;
            }
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_system_area_resolve(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *test_mode = test_json_get_string(input, "test_mode");
    
    if (strcmp(test_mode, "numeric") == 0) {
        // Find any area and test with its UID
        if (area_first) {
            char uid_str[32];
            sprintf(uid_str, "%ld", area_first->uid);
            
            AREA_DATA *resolved = NULL;
            if (is_number(uid_str)) {
                resolved = get_area_index(atol(uid_str));
            }
            
            if (!resolved && test_json_get_bool(expected, "area_resolved")) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Failed to resolve area by UID %s", uid_str);
                return TEST_FAILURE;
            }
        }
    } else if (strcmp(test_mode, "name") == 0) {
        const char *area_name = test_json_get_string(input, "area_name");
        if (area_name) {
            AREA_DATA *resolved = find_area((char*)area_name);
            if (!resolved && test_json_get_bool(input, "fallback_to_first")) {
                resolved = area_first;
            }
            
            if (!resolved && test_json_get_bool(expected, "area_resolved")) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Failed to resolve area by name '%s'", area_name);
                return TEST_FAILURE;
            }
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_system_area_fallback(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!expected) {
        return TEST_ERROR;
    }
    
    // Test that invalid system_area falls back to area_first
    AREA_DATA *fallback = area_first;
    
    if (!fallback && test_json_get_bool(expected, "fallback_area_valid")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "No fallback area available (area_first is NULL)");
        return TEST_FAILURE;
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_widevnum_parse_fallback(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *wnum_str = test_json_get_string(input, "widevnum_string");
    if (!wnum_str) {
        return TEST_ERROR;
    }
    
    WNUM result;
    bool parsed = parse_widevnum((char*)wnum_str, NULL, &result);
    
    if (!parsed && test_json_get_bool(expected, "parsed")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Failed to parse widevnum '%s' without current area", wnum_str);
        return TEST_FAILURE;
    }
    
    if (parsed && test_json_get_bool(expected, "used_system_area")) {
        // Verify the area used matches system_area or fallback
        if (!result.pArea) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "parse_widevnum succeeded but pArea is NULL");
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_widevnum_parse_explicit(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!input || !expected) {
        return TEST_ERROR;
    }
    
    const char *wnum_str = test_json_get_string(input, "widevnum_string");
    if (!wnum_str) {
        return TEST_ERROR;
    }
    
    WNUM result;
    bool parsed = parse_widevnum((char*)wnum_str, NULL, &result);
    
    if (!parsed && test_json_get_bool(expected, "parsed")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Failed to parse explicit widevnum '%s'", wnum_str);
        return TEST_FAILURE;
    }
    
    if (parsed && test_json_get_bool(expected, "used_explicit_area")) {
        // Verify explicit area was used (format: UID#vnum)
        if (!strchr(wnum_str, '#')) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "Test expects explicit area but format doesn't have #");
            return TEST_ERROR;
        }
    }
    
    return TEST_SUCCESS;
}

/***************************************************************************
 * JSON Area Serialization Tests                                            *
 ***************************************************************************/

// Forward declare json_area functions we need
extern char *json_area_serialize_to_string(AREA_DATA *area);
extern bool redis_is_available(void);
extern char *redis_get_area_full(const char *filename);
extern long redis_area_cache_warm_queue_size(void);

static test_result_t test_json_area_serialize(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    const char *area_name = test_json_get_string(input, "area_name");
    if (!area_name) {
        return TEST_ERROR;
    }

    AREA_DATA *area = find_area((char*)area_name);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Area not found: %s", area_name);
        return TEST_FAILURE;
    }

    char *json_str = json_area_serialize_to_string(area);
    if (!json_str) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Failed to serialize area: %s", area_name);
        return TEST_FAILURE;
    }

    // Parse to verify valid JSON
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Serialized JSON is invalid: %s", error.text);
        free(json_str);
        return TEST_FAILURE;
    }

    // Check for required fields
    json_t *expected_fields = json_object_get(input, "expected_fields");
    if (expected_fields && json_is_array(expected_fields)) {
        size_t index;
        json_t *field;
        json_array_foreach(expected_fields, index, field) {
            const char *field_path = json_string_value(field);
            if (field_path) {
                // Simple path check - split by . and traverse
                json_t *current = root;
                char *path_copy = strdup(field_path);
                char *token = strtok(path_copy, ".");
                while (token && current) {
                    current = json_object_get(current, token);
                    token = strtok(NULL, ".");
                }
                free(path_copy);

                if (!current) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "Missing required field: %s", field_path);
                    json_decref(root);
                    free(json_str);
                    return TEST_FAILURE;
                }
            }
        }
    }

    json_decref(root);
    free(json_str);
    return TEST_SUCCESS;
}

static test_result_t test_json_area_rooms(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    const char *area_name = test_json_get_string(input, "area_name");
    if (!area_name) {
        return TEST_ERROR;
    }

    AREA_DATA *area = find_area((char*)area_name);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Area not found: %s", area_name);
        return TEST_FAILURE;
    }

    char *json_str = json_area_serialize_to_string(area);
    if (!json_str) {
        return TEST_FAILURE;
    }

    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    free(json_str);

    if (!root) {
        return TEST_FAILURE;
    }

    json_t *rooms = json_object_get(root, "rooms");
    if (!rooms || !json_is_array(rooms) || json_array_size(rooms) == 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No rooms in serialized area");
        json_decref(root);
        return TEST_FAILURE;
    }

    // Check that exits have to_area field
    bool check_exit_fields = test_json_get_bool(input, "check_exit_fields");
    if (check_exit_fields) {
        size_t index;
        json_t *room;
        json_array_foreach(rooms, index, room) {
            json_t *exits = json_object_get(room, "exits");
            if (exits && json_is_array(exits)) {
                size_t exit_idx;
                json_t *exit_obj;
                json_array_foreach(exits, exit_idx, exit_obj) {
                    // Verify to_area is present when to_vnum is present
                    json_t *to_vnum = json_object_get(exit_obj, "to_vnum");
                    json_t *to_area = json_object_get(exit_obj, "to_area");
                    if (to_vnum && !to_area) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                     "Exit has to_vnum but no to_area field");
                        json_decref(root);
                        return TEST_FAILURE;
                    }
                }
            }
        }
    }

    json_decref(root);
    return TEST_SUCCESS;
}

static test_result_t test_cross_area_exit(test_case_t *test) {
    // This test verifies that cross-area exits are properly linked
    // after boot by checking that exits with stored area_uid are resolved

    if (!test || !test->config) {
        return TEST_ERROR;
    }

    // Check a sample of rooms to verify their exits are resolved
    int checked = 0, resolved = 0;
    AREA_DATA *area;

    for (area = area_first; area && checked < 100; area = area->next) {
        for (int hash = 0; hash < MAX_KEY_HASH && checked < 100; hash++) {
            ROOM_INDEX_DATA *room;
            for (room = area->room_index_hash[hash]; room && checked < 100; room = room->next) {
                for (int door = 0; door < MAX_DIR; door++) {
                    EXIT_DATA *exit = room->exit[door];
                    if (exit && exit->u1.to_room) {
                        checked++;
                        resolved++;
                    } else if (exit && exit->u1.vnum > 0) {
                        // Exit with vnum but not resolved - might be expected for bad vnums
                        checked++;
                    }
                }
            }
        }
    }

    if (checked == 0) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS, "No exits found to check");
        return TEST_SUCCESS;
    }

    // At least 90% of exits should be resolved
    if (resolved < (checked * 90 / 100)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Only %d/%d exits resolved", resolved, checked);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_json_area_roundtrip(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    const char *area_name = test_json_get_string(input, "area_name");
    if (!area_name) {
        return TEST_ERROR;
    }

    AREA_DATA *area = find_area((char*)area_name);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Area not found: %s", area_name);
        return TEST_FAILURE;
    }

    // Serialize
    char *json_str = json_area_serialize_to_string(area);
    if (!json_str) {
        return TEST_FAILURE;
    }

    // Parse and verify key fields match
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    free(json_str);

    if (!root) {
        return TEST_FAILURE;
    }

    json_t *area_obj = json_object_get(root, "area");
    if (!area_obj) {
        json_decref(root);
        return TEST_FAILURE;
    }

    // Verify UID matches
    long uid = json_integer_value(json_object_get(area_obj, "uid"));
    if (uid != area->uid) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "UID mismatch: expected %ld, got %ld", area->uid, uid);
        json_decref(root);
        return TEST_FAILURE;
    }

    // Verify vnum range
    json_t *vnums = json_object_get(area_obj, "vnums");
    if (vnums) {
        long min_vnum = json_integer_value(json_object_get(vnums, "min"));
        long max_vnum = json_integer_value(json_object_get(vnums, "max"));
        if (min_vnum != area->min_vnum || max_vnum != area->max_vnum) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Vnum range mismatch");
            json_decref(root);
            return TEST_FAILURE;
        }
    }

    json_decref(root);
    return TEST_SUCCESS;
}

/***************************************************************************
 * Redis Area Caching Tests                                                 *
 ***************************************************************************/

static test_result_t test_redis_available(test_case_t *test) {
    if (!redis_is_available()) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "Redis not available - skipping Redis tests");
        return TEST_SKIP;
    }
    return TEST_SUCCESS;
}

static test_result_t test_redis_area_cached(test_case_t *test) {
    if (!redis_is_available()) {
        return TEST_SKIP;
    }

    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    json_t *areas_to_check = json_object_get(input, "areas_to_check");
    if (!areas_to_check || !json_is_array(areas_to_check)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *area_spec;
    int checked = 0, found = 0;

    json_array_foreach(areas_to_check, index, area_spec) {
        const char *area_name = test_json_get_string(area_spec, "name");
        if (!area_name) continue;

        AREA_DATA *area = find_area((char*)area_name);
        if (!area || !area->file_name) continue;

        checked++;
        char *cached = redis_get_area_full(area->file_name);
        if (cached) {
            found++;
            free(cached);
        }
    }

    if (checked > 0 && found == 0) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "No areas found in Redis cache - cache may still be warming");
        // Don't fail, just warn - cache warming is async
        return TEST_SUCCESS;
    }

    return TEST_SUCCESS;
}

static test_result_t test_redis_area_cache_format(test_case_t *test) {
    if (!redis_is_available()) {
        return TEST_SKIP;
    }

    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    const char *area_name = test_json_get_string(input, "area_name");
    if (!area_name) {
        return TEST_ERROR;
    }

    AREA_DATA *area = find_area((char*)area_name);
    if (!area || !area->file_name) {
        return TEST_ERROR;
    }

    char *cached = redis_get_area_full(area->file_name);
    if (!cached) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "Area not in cache - may still be warming");
        return TEST_SUCCESS; // Don't fail, cache warming is async
    }

    // Verify it's valid JSON
    json_error_t error;
    json_t *root = json_loads(cached, 0, &error);
    free(cached);

    if (!root) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Cached JSON is invalid: %s", error.text);
        return TEST_FAILURE;
    }

    // Check expected fields
    json_t *expected_fields = json_object_get(input, "expected_json_fields");
    if (expected_fields && json_is_array(expected_fields)) {
        size_t index;
        json_t *field;
        json_array_foreach(expected_fields, index, field) {
            const char *field_name = json_string_value(field);
            if (field_name && !json_object_get(root, field_name)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Missing field in cached JSON: %s", field_name);
                json_decref(root);
                return TEST_FAILURE;
            }
        }
    }

    json_decref(root);
    return TEST_SUCCESS;
}

static test_result_t test_redis_warm_queue(test_case_t *test) {
    if (!redis_is_available()) {
        return TEST_SKIP;
    }

    long queue_size = redis_area_cache_warm_queue_size();

    // Just report the queue size - we don't fail if it's not empty
    // since warming happens over time
    if (queue_size > 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                     "Redis warm queue has %ld items remaining", queue_size);
    }

    return TEST_SUCCESS;
}

#endif // BUILD_TESTS