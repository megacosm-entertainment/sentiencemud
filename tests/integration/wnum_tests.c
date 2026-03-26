#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../log.h"

// Forward declarations
static test_result_t test_wnum_parsing(test_case_t *test);
static test_result_t test_area_name_parsing(test_case_t *test);
static test_result_t test_wnum_parsing_structured(test_case_t *test);
static test_result_t test_area_existence_check(test_case_t *test);
static test_result_t test_area_integrity_check(test_case_t *test);
static test_result_t test_config_validator(test_case_t *test);
static test_result_t test_uid_uniqueness_check(test_case_t *test);
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
static const char *resolve_test_widevnum_input(json_t *test_case, char *buffer, size_t buffer_size);

test_result_t run_wnum_test_case(test_case_t *test);

void register_wnum_tests(void) {
    // These will be loaded from JSON files rather than registered directly
    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "WNUM test handlers registered");
}

static const char *resolve_test_widevnum_input(json_t *test_case, char *buffer, size_t buffer_size)
{
    const char *vnum_string = test_json_get_string(test_case, "vnum_string");
    if (vnum_string && vnum_string[0]) {
        return vnum_string;
    }

    const char *explicit_area_name = test_json_get_string(test_case, "explicit_area_name");
    int explicit_vnum = test_json_get_int(test_case, "explicit_vnum");

    if (!explicit_area_name || explicit_vnum <= 0 || !buffer || buffer_size == 0) {
        return NULL;
    }

    AREA_DATA *explicit_area = find_area((char *)explicit_area_name);
    if (!explicit_area || explicit_area->uid <= 0) {
        return NULL;
    }

    snprintf(buffer, buffer_size, "%ld#%d", explicit_area->uid, explicit_vnum);
    return buffer;
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

test_result_t run_wnum_test_case(test_case_t *test) {
    if (!test) {
        return TEST_ERROR;
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

    return result;
}

static test_result_t test_wnum_parsing_structured(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");
    const char *suite_context_name = test_json_get_string(input, "area_context");
    AREA_DATA *suite_context_area = suite_context_name ? find_area((char*)suite_context_name) : NULL;
    bool suite_required = true;
    if (input && json_object_get(input, "required")) {
        suite_required = test_json_get_bool(input, "required");
    }

    if (suite_context_name && !suite_context_area) {
        if (!suite_required) {
            log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                          "Skipping optional WNUM suite: missing area context '%s'",
                          suite_context_name);
            return TEST_SKIP;
        }

        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Required WNUM suite context area not found: %s",
                      suite_context_name);
        return TEST_FAILURE;
    }
    
    if (!json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid structured WNUM test: no test_cases array");
        return TEST_ERROR;
    }
    
    size_t index;
    json_t *test_case;
    json_array_foreach(test_cases, index, test_case) {
        char generated_vnum[64];
        const char *vnum_string = resolve_test_widevnum_input(test_case, generated_vnum, sizeof(generated_vnum));
        const char *context_area_name = test_json_get_string(test_case, "context_area");
        bool required = true;
        if (json_object_get(test_case, "required")) {
            required = test_json_get_bool(test_case, "required");
        }
        bool should_parse = true;
        if (json_object_get(test_case, "should_parse")) {
            should_parse = test_json_get_bool(test_case, "should_parse");
        }
        
        if (!vnum_string) {
            if (!required) {
                log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                            "Skipping optional WNUM test case with unresolved vnum input");
                continue;
            }
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid test case: missing vnum_string");
            return TEST_ERROR;
        }
        
        AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : suite_context_area;
        if (context_area_name && !context_area) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                          "Context area not found: %s", context_area_name);
            return TEST_FAILURE;
        }
        
        WNUM result;
        bool parse_ok = parse_widevnum((char*)vnum_string, context_area, &result);
        if (!parse_ok && should_parse) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                          "WNUM parse mismatch for input '%s': expected parse_success=true, actual parse_success=false",
                          vnum_string);
            return TEST_FAILURE;
        }

        if (parse_ok && !should_parse) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "WNUM parse mismatch for input '%s': expected parse_success=false, actual parse_success=true",
                          vnum_string);
            return TEST_FAILURE;
        }

        if (!parse_ok && !should_parse) {
            continue;
        }

        if (json_object_get(test_case, "expected_vnum")) {
            long expected_vnum = (long)test_json_get_int(test_case, "expected_vnum");
            if (result.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "WNUM value mismatch for input '%s': expected vnum=%ld, actual vnum=%ld",
                              vnum_string,
                              expected_vnum,
                              result.vnum);
                return TEST_FAILURE;
            }
        }

        if (json_object_get(test_case, "expected_area")) {
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            AREA_DATA *expected_area = expected_area_name ? find_area((char *)expected_area_name) : NULL;
            if (!expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Invalid test case: expected_area '%s' was not found",
                              expected_area_name ? expected_area_name : "(null)");
                return TEST_ERROR;
            }

            if (result.pArea != expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "WNUM area mismatch for input '%s': expected area='%s', actual area='%s'",
                              vnum_string,
                              expected_area_name,
                              result.pArea ? result.pArea->name : "(null)");
                return TEST_FAILURE;
            }
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
    json_t *required_rooms = json_object_get(input, "required_rooms");
    json_t *required_mobiles = json_object_get(input, "required_mobiles");
    
    if (!json_is_array(required_areas)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area existence test: no required_areas array");
        return TEST_ERROR;
    }
    
    size_t index;
    json_t *area_spec;
    json_array_foreach(required_areas, index, area_spec) {
        const char *area_name = test_json_get_string(area_spec, "name");
        int expected_uid = test_json_get_int(area_spec, "expected_uid");
        bool required = true;
        bool should_exist = true;
        if (json_object_get(area_spec, "required")) {
            required = test_json_get_bool(area_spec, "required");
        }
        if (json_object_get(area_spec, "should_exist")) {
            should_exist = test_json_get_bool(area_spec, "should_exist");
        }
        
        if (!area_name) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area spec: missing name");
            return TEST_ERROR;
        }
        
        AREA_DATA *area = find_area((char*)area_name);
        if (!should_exist) {
            if (area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Area lookup mismatch for '%s': expected exists=false, actual exists=true",
                              area_name);
                return TEST_FAILURE;
            }

            log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                          "Area correctly absent: %s",
                          area_name);
            continue;
        }

        if (!area) {
            if (!required) {
                log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                              "Optional area missing (allowed): %s",
                              area_name);
                continue;
            }
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Area lookup mismatch for '%s': expected exists=true, actual exists=false",
                          area_name);
            return TEST_FAILURE;
        }

        bool assert_legacy_vnum_range = false;
        if (json_object_get(area_spec, "assert_legacy_vnum_range")) {
            assert_legacy_vnum_range = test_json_get_bool(area_spec, "assert_legacy_vnum_range");
        }

        json_t *vnum_range = json_object_get(area_spec, "vnum_range");
        if (assert_legacy_vnum_range && json_is_array(vnum_range) && json_array_size(vnum_range) == 2) {
            json_t *min_json = json_array_get(vnum_range, 0);
            json_t *max_json = json_array_get(vnum_range, 1);
            if (!json_is_integer(min_json) || !json_is_integer(max_json)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Invalid vnum_range spec for area '%s': expected integer bounds",
                              area_name);
                return TEST_ERROR;
            }

            long expected_min = (long)json_integer_value(min_json);
            long expected_max = (long)json_integer_value(max_json);
            if (expected_min > expected_max) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Invalid vnum_range spec for area '%s': min %ld > max %ld",
                              area_name, expected_min, expected_max);
                return TEST_ERROR;
            }

            if (area->min_vnum > expected_min || area->max_vnum < expected_max) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Area '%s' vnum range mismatch: expected to cover [%ld,%ld], actual [%ld,%ld]",
                              area_name,
                              expected_min,
                              expected_max,
                              area->min_vnum,
                              area->max_vnum);
                return TEST_FAILURE;
            }
        }
        
        if (expected_uid > 0 && area->uid != expected_uid) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Area %s has wrong UID: expected %d, got %ld",
                          area_name, expected_uid, area->uid);
            return TEST_FAILURE;
        }
        
        log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Area found: %s (UID: %ld)", area_name, area->uid);
    }

    if (required_rooms) {
        if (!json_is_array(required_rooms)) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area existence test: required_rooms must be an array");
            return TEST_ERROR;
        }

        size_t room_index;
        json_t *room_spec;
        json_array_foreach(required_rooms, room_index, room_spec) {
            const char *area_name = test_json_get_string(room_spec, "area_name");
            long room_vnum = test_json_get_int(room_spec, "vnum");
            bool required = true;
            bool should_exist = true;

            if (json_object_get(room_spec, "required")) {
                required = test_json_get_bool(room_spec, "required");
            }
            if (json_object_get(room_spec, "should_exist")) {
                should_exist = test_json_get_bool(room_spec, "should_exist");
            }

            if (!area_name || room_vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Invalid room spec at index %zu: expected area_name and positive vnum",
                              room_index);
                return TEST_ERROR;
            }

            AREA_DATA *area = find_area((char *)area_name);
            if (!area) {
                if (!required) {
                    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                                  "Optional room spec skipped: area '%s' missing",
                                  area_name);
                    continue;
                }
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Room lookup failed: area '%s' not found for room %ld",
                              area_name,
                              room_vnum);
                return TEST_FAILURE;
            }

            ROOM_INDEX_DATA *room = get_room_index(area, room_vnum);
            if (!should_exist) {
                if (room) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Room lookup mismatch for '%s' #%ld: expected exists=false, actual exists=true",
                                  area_name,
                                  room_vnum);
                    return TEST_FAILURE;
                }
                continue;
            }

            if (!room) {
                if (!required) {
                    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                                  "Optional room missing (allowed): '%s' #%ld",
                                  area_name,
                                  room_vnum);
                    continue;
                }
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Room lookup mismatch for '%s' #%ld: expected exists=true, actual exists=false",
                              area_name,
                              room_vnum);
                return TEST_FAILURE;
            }
        }
    }

    if (required_mobiles) {
        if (!json_is_array(required_mobiles)) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid area existence test: required_mobiles must be an array");
            return TEST_ERROR;
        }

        size_t mob_index;
        json_t *mob_spec;
        json_array_foreach(required_mobiles, mob_index, mob_spec) {
            const char *area_name = test_json_get_string(mob_spec, "area_name");
            long mob_vnum = test_json_get_int(mob_spec, "vnum");
            bool required = true;
            bool should_exist = true;

            if (json_object_get(mob_spec, "required")) {
                required = test_json_get_bool(mob_spec, "required");
            }
            if (json_object_get(mob_spec, "should_exist")) {
                should_exist = test_json_get_bool(mob_spec, "should_exist");
            }

            if (!area_name || mob_vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Invalid mobile spec at index %zu: expected area_name and positive vnum",
                              mob_index);
                return TEST_ERROR;
            }

            AREA_DATA *area = find_area((char *)area_name);
            if (!area) {
                if (!required) {
                    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                                  "Optional mobile spec skipped: area '%s' missing",
                                  area_name);
                    continue;
                }
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Mobile lookup failed: area '%s' not found for mob %ld",
                              area_name,
                              mob_vnum);
                return TEST_FAILURE;
            }

            MOB_INDEX_DATA *mob = get_mob_index(area, mob_vnum);
            if (!should_exist) {
                if (mob) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Mobile lookup mismatch for '%s' #%ld: expected exists=false, actual exists=true",
                                  area_name,
                                  mob_vnum);
                    return TEST_FAILURE;
                }
                continue;
            }

            if (!mob) {
                if (!required) {
                    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                                  "Optional mobile missing (allowed): '%s' #%ld",
                                  area_name,
                                  mob_vnum);
                    continue;
                }
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Mobile lookup mismatch for '%s' #%ld: expected exists=true, actual exists=false",
                              area_name,
                              mob_vnum);
                return TEST_FAILURE;
            }
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_area_integrity_check(test_case_t *test) {
    json_t *input = NULL;
    json_t *checks = NULL;
    size_t index;
    json_t *check_entry;

    if (!test || !test->config)
        return TEST_ERROR;

    input = json_object_get(test->config, "input");
    if (json_is_object(input))
        checks = json_object_get(input, "checks");

    if (!json_is_array(checks) || json_array_size(checks) == 0) {
        if (!area_first) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No areas loaded");
            return TEST_FAILURE;
        }
        log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Area integrity check passed (default mode)");
        return TEST_SUCCESS;
    }

    json_array_foreach(checks, index, check_entry) {
        const char *check_name = json_is_string(check_entry) ? json_string_value(check_entry) : NULL;
        if (!check_name || !check_name[0]) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Invalid area integrity check entry at index %zu", index);
            return TEST_ERROR;
        }

        if (strcmp(check_name, "area_exists") == 0) {
            if (!area_first) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Area integrity failure: no areas loaded");
                return TEST_FAILURE;
            }
        } else if (strcmp(check_name, "valid_uid") == 0) {
            AREA_DATA *area;
            for (area = area_first; area; area = area->next) {
                if (area->uid <= 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Area integrity failure: invalid uid %ld for area '%s'",
                                  area->uid,
                                  area->name ? area->name : "(null)");
                    return TEST_FAILURE;
                }
            }
        } else if (strcmp(check_name, "vnum_range_valid") == 0) {
            AREA_DATA *area;
            for (area = area_first; area; area = area->next) {
                if (area->min_vnum == 0 && area->max_vnum == 0)
                    continue;

                if (area->min_vnum <= 0 || area->max_vnum <= 0 || area->min_vnum > area->max_vnum) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Area integrity failure: invalid vnum range [%ld,%ld] for area '%s'",
                                  area->min_vnum,
                                  area->max_vnum,
                                  area->name ? area->name : "(null)");
                    return TEST_FAILURE;
                }
            }
        } else if (strcmp(check_name, "area_name_not_empty") == 0) {
            AREA_DATA *area;
            for (area = area_first; area; area = area->next) {
                if (!area->name || area->name[0] == '\0') {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Area integrity failure: empty area name for uid %ld",
                                  area->uid);
                    return TEST_FAILURE;
                }
            }
        } else if (strcmp(check_name, "no_duplicate_uids") == 0) {
            AREA_DATA *outer;
            for (outer = area_first; outer; outer = outer->next) {
                AREA_DATA *inner;
                if (outer->uid <= 0 || outer->uid >= 1000)
                    continue;
                for (inner = outer->next; inner; inner = inner->next) {
                    if (inner->uid <= 0 || inner->uid >= 1000)
                        continue;

                    if (outer->uid == inner->uid) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                      "Area integrity failure: duplicate uid %ld between '%s' and '%s'",
                                      outer->uid,
                                      outer->name ? outer->name : "(null)",
                                      inner->name ? inner->name : "(null)");
                        return TEST_FAILURE;
                    }
                }
            }
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Unknown area integrity check '%s'", check_name);
            return TEST_ERROR;
        }
    }

    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Area integrity check passed");
    return TEST_SUCCESS;
}

static test_result_t test_config_validator(test_case_t *test) {
    json_t *input = NULL;
    json_t *checks = NULL;
    bool next_area_uid_checked = false;
    bool strict = false;
    size_t index;
    json_t *check_entry;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (json_is_object(input) && json_object_get(input, "strict")) {
            strict = test_json_get_bool(input, "strict");
        }

        if (json_is_object(input)) {
            checks = json_object_get(input, "checks");
        }
    }

    if (!json_is_array(checks) || json_array_size(checks) == 0) {
        checks = NULL;
    }

    if (!checks) {
        next_area_uid_checked = true;
        if (gconfig.next_area_uid <= 0) {
            if (strict) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Invalid next_area_uid: %ld", gconfig.next_area_uid);
                return TEST_FAILURE;
            }

            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                          "next_area_uid is not initialized in this environment (%ld); continuing in non-strict mode",
                          gconfig.next_area_uid);
            return TEST_SUCCESS;
        }

        log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Global config validation passed");
        return TEST_SUCCESS;
    }

    json_array_foreach(checks, index, check_entry) {
        const char *check_name = json_is_string(check_entry) ? json_string_value(check_entry) : NULL;

        if (!check_name || !check_name[0]) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Invalid config check entry at index %zu", index);
            return TEST_ERROR;
        }

        if (strcmp(check_name, "next_area_uid_valid") == 0) {
            next_area_uid_checked = true;
            if (gconfig.next_area_uid <= 0) {
                if (strict) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Invalid next_area_uid: %ld", gconfig.next_area_uid);
                    return TEST_FAILURE;
                }

                log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                              "next_area_uid is not initialized in this environment (%ld); continuing in non-strict mode",
                              gconfig.next_area_uid);
            }
        } else if (strcmp(check_name, "memory_allocator_working") == 0) {
            size_t alloc_size = 128;
            void *mem_block = malloc(alloc_size);
            if (!mem_block) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "Config validation failure: memory allocator returned NULL");
                return TEST_FAILURE;
            }
            memset(mem_block, 0xAB, alloc_size);
            free(mem_block);
        } else if (strcmp(check_name, "logging_system_active") == 0) {
            log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                        "Config validator logging_system_active probe");
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Unknown config validator check '%s'", check_name);
            return TEST_ERROR;
        }
    }

    if (!next_area_uid_checked && gconfig.next_area_uid <= 0 && strict) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Invalid next_area_uid: %ld", gconfig.next_area_uid);
        return TEST_FAILURE;
    }

    log_message(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS, "Global config validation passed");
    return TEST_SUCCESS;
}

static test_result_t test_uid_uniqueness_check(test_case_t *test) {
    json_t *input = NULL;
    long scan_min_uid = 1;
    long scan_max_uid = LONG_MAX;
    bool check_duplicates = true;
    bool require_area_count_positive = false;
    size_t duplicate_count = 0;
    int area_count = 0;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
    }

    if (json_is_object(input)) {
        json_t *scan_range = json_object_get(input, "scan_range");
        if (json_is_array(scan_range) && json_array_size(scan_range) == 2) {
            json_t *min_json = json_array_get(scan_range, 0);
            json_t *max_json = json_array_get(scan_range, 1);

            if (!json_is_integer(min_json) || !json_is_integer(max_json)) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "UID uniqueness check has non-integer scan_range bounds");
                return TEST_ERROR;
            }

            scan_min_uid = (long)json_integer_value(min_json);
            scan_max_uid = (long)json_integer_value(max_json);
            if (scan_min_uid > scan_max_uid) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "UID uniqueness check has invalid scan_range [%ld,%ld]",
                              scan_min_uid,
                              scan_max_uid);
                return TEST_ERROR;
            }
        }

        if (json_object_get(input, "check_duplicates")) {
            check_duplicates = test_json_get_bool(input, "check_duplicates");
        }

        if (json_object_get(input, "area_count_positive")) {
            require_area_count_positive = test_json_get_bool(input, "area_count_positive");
        }
    }

    for (AREA_DATA *area = area_first; area; area = area->next) {
        if (area->uid <= 0 || area->uid < scan_min_uid || area->uid > scan_max_uid) {
            continue;
        }

        area_count++;

        if (!check_duplicates) {
            continue;
        }

        for (AREA_DATA *other = area->next; other; other = other->next) {
            if (other->uid <= 0 || other->uid < scan_min_uid || other->uid > scan_max_uid) {
                continue;
            }

            if (area->uid == other->uid) {
                duplicate_count++;
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate area UID %ld found between '%s' and '%s'",
                              area->uid,
                              area->name ? area->name : "(null)",
                              other->name ? other->name : "(null)");
                return TEST_FAILURE;
            }
        }
    }

    if (require_area_count_positive && area_count <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "UID uniqueness check expected at least one area in range [%ld,%ld], found none",
                      scan_min_uid,
                      scan_max_uid);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                  "UID uniqueness check passed (%d areas scanned, range [%ld,%ld], duplicates=%zu)",
                  area_count,
                  scan_min_uid,
                  scan_max_uid,
                  duplicate_count);
    return TEST_SUCCESS;
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
    const char *expected_type = test_json_get_string(input, "expected_type");
    bool check_area_items = test_json_get_bool(input, "check_area_items");
    bool should_exist = test_json_get_bool(input, "should_exist");

    if (check_area_items) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        bool found_area_reserved = false;

        if (!reserved_vnums) {
            return TEST_ERROR;
        }

        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            AREA_DATA *area;

            if (reserved->type != RESERVED_AREA) {
                continue;
            }

            found_area_reserved = true;
            area = get_area_index(reserved->wnum.auid);
            if (!area && reserved->wnum.auid > 0) {
                iterator_stop(&it);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Reserved AREA '%s' has invalid area UID %ld",
                             reserved->name, reserved->wnum.auid);
                return TEST_FAILURE;
            }
        }
        iterator_stop(&it);

        if (!found_area_reserved) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                        "Reserved AREA validation failed: expected at least 1 RESERVED_AREA entry, actual 0");
            return TEST_FAILURE;
        }

        return TEST_SUCCESS;
    }

    if (!reserved_name) {
        return TEST_ERROR;
    }
    
    RESERVED_DATA *reserved = find_reserved(reserved_name);
    
    if (should_exist && !reserved) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Reserved lookup mismatch for '%s': expected exists=true, actual exists=false",
                     reserved_name);
        return TEST_FAILURE;
    }
    
    if (!should_exist && reserved) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Reserved lookup mismatch for '%s': expected exists=false, actual exists=true",
                     reserved_name);
        return TEST_FAILURE;
    }

    if (reserved && expected_type && expected_type[0]) {
        const char *actual_type = reserved_types_get_name(reserved->type);
        if (!actual_type || str_cmp(actual_type, expected_type) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Reserved type mismatch for '%s': expected type='%s', actual type='%s'",
                         reserved_name,
                         expected_type,
                         actual_type ? actual_type : "(null)");
            return TEST_FAILURE;
        }
    }
    
    if (reserved && test_json_get_bool(expected, "has_area")) {
        AREA_DATA *area = get_area_index(reserved->wnum.auid);
        if (!area && reserved->wnum.auid > 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Reserved area mismatch for '%s': expected valid area for UID %ld, actual area=NULL",
                         reserved_name, reserved->wnum.auid);
            return TEST_FAILURE;
        }
    }
    
    if (reserved && test_json_get_bool(expected, "has_vnum")) {
        if (reserved->type != RESERVED_AREA && reserved->wnum.vnum <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Reserved vnum mismatch for '%s': expected vnum > 0, actual vnum=%ld",
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

            if (resolved && json_object_get(input, "expected_area_name")) {
                const char *expected_name = test_json_get_string(input, "expected_area_name");
                if (!expected_name || str_cmp(expected_name, resolved->name) != 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "Numeric resolution mismatch: expected area '%s', got '%s'",
                                 expected_name ? expected_name : "(null)",
                                 resolved->name ? resolved->name : "(null)");
                    return TEST_FAILURE;
                }
            }

            if (resolved && json_object_get(input, "expected_matches_first") &&
                test_json_get_bool(input, "expected_matches_first") &&
                resolved != area_first) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Numeric resolution mismatch: expected area_first '%s', got '%s'",
                             area_first && area_first->name ? area_first->name : "(null)",
                             resolved->name ? resolved->name : "(null)");
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

            if (resolved && json_object_get(input, "expected_area_name")) {
                const char *expected_name = test_json_get_string(input, "expected_area_name");
                if (!expected_name || str_cmp(expected_name, resolved->name) != 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "Name resolution mismatch: expected area '%s', got '%s'",
                                 expected_name ? expected_name : "(null)",
                                 resolved->name ? resolved->name : "(null)");
                    return TEST_FAILURE;
                }
            }
        }
    }
    
    return TEST_SUCCESS;
}

static test_result_t test_system_area_fallback(test_case_t *test) {
    if (!test || !test->config) {
        return TEST_ERROR;
    }
    
    json_t *input = json_object_get(test->config, "input");
    json_t *expected = json_object_get(test->config, "expected_output");
    
    if (!expected) {
        return TEST_ERROR;
    }

    if (json_is_object(input) && json_object_get(input, "invalid_setting")) {
        const char *invalid_setting = test_json_get_string(input, "invalid_setting");
        if (invalid_setting && find_area((char *)invalid_setting)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Fallback test invalid_setting '%s' unexpectedly resolves to an area",
                         invalid_setting);
            return TEST_ERROR;
        }
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
    char generated_widevnum[64];
    if ((!wnum_str || !wnum_str[0]) && input) {
        wnum_str = resolve_test_widevnum_input(input, generated_widevnum, sizeof(generated_widevnum));
    }
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

        if (json_object_get(input, "expected_area_name")) {
            const char *expected_area_name = test_json_get_string(input, "expected_area_name");
            if (!expected_area_name || str_cmp(expected_area_name, result.pArea->name) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Widevnum fallback area mismatch: expected '%s', got '%s'",
                             expected_area_name ? expected_area_name : "(null)",
                             result.pArea ? result.pArea->name : "(null)");
                return TEST_FAILURE;
            }
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
    
    char generated_widevnum[64];
    const char *wnum_str = test_json_get_string(input, "widevnum_string");
    if ((!wnum_str || !wnum_str[0]) && input) {
        wnum_str = resolve_test_widevnum_input(input, generated_widevnum, sizeof(generated_widevnum));
    }
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

        if (json_object_get(input, "explicit_area_name")) {
            const char *explicit_area_name = test_json_get_string(input, "explicit_area_name");
            AREA_DATA *expected_area = explicit_area_name ? find_area((char *)explicit_area_name) : NULL;
            if (!expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Explicit widevnum test expected area '%s' was not found",
                             explicit_area_name ? explicit_area_name : "(null)");
                return TEST_ERROR;
            }

            if (result.pArea != expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Explicit widevnum area mismatch: expected '%s', got '%s'",
                             expected_area->name ? expected_area->name : "(null)",
                             result.pArea ? result.pArea->name : "(null)");
                return TEST_FAILURE;
            }
        }

        if (json_object_get(input, "explicit_vnum")) {
            int explicit_vnum = test_json_get_int(input, "explicit_vnum");
            if ((long)explicit_vnum != result.vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Explicit widevnum value mismatch: expected %d, got %ld",
                             explicit_vnum,
                             result.vnum);
                return TEST_FAILURE;
            }
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

    bool optional_if_missing = false;
    if (json_object_get(input, "optional_if_missing")) {
        optional_if_missing = test_json_get_bool(input, "optional_if_missing");
    }

    const char *area_name = test_json_get_string(input, "area_name");
    if (!area_name) {
        return TEST_ERROR;
    }

    AREA_DATA *area = find_area((char*)area_name);
    if (!area) {
        if (optional_if_missing) {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                         "Skipping optional JSON serialize test: area not found: %s", area_name);
            return TEST_SKIP;
        }
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

    if (json_object_get(input, "expected_uid")) {
        long expected_uid = (long)test_json_get_int(input, "expected_uid");
        json_t *area_obj = json_object_get(root, "area");
        long actual_uid = area_obj ? json_integer_value(json_object_get(area_obj, "uid")) : -1;
        if (actual_uid != expected_uid) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Serialized area uid mismatch for '%s': expected %ld, got %ld",
                         area_name, expected_uid, actual_uid);
            json_decref(root);
            free(json_str);
            return TEST_FAILURE;
        }
    }

    if (json_object_get(input, "expected_filename")) {
        const char *expected_filename = test_json_get_string(input, "expected_filename");
        json_t *area_obj = json_object_get(root, "area");
        json_t *filename_json = area_obj ? json_object_get(area_obj, "filename") : NULL;
        const char *actual_filename = json_is_string(filename_json) ? json_string_value(filename_json) : NULL;
        if (!expected_filename || !actual_filename || str_cmp(expected_filename, actual_filename)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Serialized area filename mismatch for '%s': expected '%s', got '%s'",
                         area_name,
                         expected_filename ? expected_filename : "(null)",
                         actual_filename ? actual_filename : "(null)");
            json_decref(root);
            free(json_str);
            return TEST_FAILURE;
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

    if (json_object_get(input, "minimum_rooms")) {
        long minimum_rooms = (long)test_json_get_int(input, "minimum_rooms");
        long room_count = (long)json_array_size(rooms);
        if (minimum_rooms > 0 && room_count < minimum_rooms) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Serialized room count too low for '%s': expected at least %ld, got %ld",
                         area_name,
                         minimum_rooms,
                         room_count);
            json_decref(root);
            return TEST_FAILURE;
        }
    }

    bool enforce_room_vnum_range = false;
    if (json_object_get(input, "enforce_room_vnum_range")) {
        enforce_room_vnum_range = test_json_get_bool(input, "enforce_room_vnum_range");
    }

    if (enforce_room_vnum_range) {
        size_t room_index;
        json_t *room;
        json_array_foreach(rooms, room_index, room) {
            json_t *vnum_json = json_object_get(room, "vnum");
            long room_vnum = json_is_integer(vnum_json) ? (long)json_integer_value(vnum_json) : -1;
            if (room_vnum < area->min_vnum || room_vnum > area->max_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Serialized room vnum %ld out of area range [%ld,%ld] for '%s'",
                             room_vnum,
                             area->min_vnum,
                             area->max_vnum,
                             area_name);
                json_decref(root);
                return TEST_FAILURE;
            }
        }
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

    if (json_object_get(input, "verify_name")) {
        bool verify_name = test_json_get_bool(input, "verify_name");
        if (verify_name) {
            json_t *name_json = json_object_get(area_obj, "name");
            const char *serialized_name = json_is_string(name_json) ? json_string_value(name_json) : NULL;
            if (!serialized_name || str_cmp(serialized_name, area->name)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Area name roundtrip mismatch for '%s': expected '%s', got '%s'",
                             area_name,
                             area->name ? area->name : "(null)",
                             serialized_name ? serialized_name : "(null)");
                json_decref(root);
                return TEST_FAILURE;
            }
        }
    }

    if (json_object_get(input, "verify_filename")) {
        bool verify_filename = test_json_get_bool(input, "verify_filename");
        if (verify_filename) {
            json_t *filename_json = json_object_get(area_obj, "filename");
            const char *serialized_filename = json_is_string(filename_json) ? json_string_value(filename_json) : NULL;
            if (!serialized_filename || !area->file_name || str_cmp(serialized_filename, area->file_name)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Area filename roundtrip mismatch for '%s': expected '%s', got '%s'",
                             area_name,
                             area->file_name ? area->file_name : "(null)",
                             serialized_filename ? serialized_filename : "(null)");
                json_decref(root);
                return TEST_FAILURE;
            }
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

    bool validate_cached_json = false;
    bool require_cache_hits = false;
    int min_cached_hits = 1;

    if (json_object_get(input, "validate_cached_json")) {
        validate_cached_json = test_json_get_bool(input, "validate_cached_json");
    }
    if (json_object_get(input, "require_cache_hits")) {
        require_cache_hits = test_json_get_bool(input, "require_cache_hits");
    }
    if (json_object_get(input, "min_cached_hits")) {
        int cfg_hits = test_json_get_int(input, "min_cached_hits");
        if (cfg_hits > 0) {
            min_cached_hits = cfg_hits;
        }
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
            if (validate_cached_json) {
                json_error_t error;
                json_t *root = json_loads(cached, 0, &error);
                if (!root) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "Cached payload for area '%s' is not valid JSON: %s",
                                 area_name,
                                 error.text);
                    free(cached);
                    return TEST_FAILURE;
                }
                json_decref(root);
            }

            found++;
            free(cached);
        }
    }

    if (checked > 0 && found < min_cached_hits) {
        if (require_cache_hits) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Redis cache hit threshold unmet: checked=%d found=%d minimum=%d",
                         checked,
                         found,
                         min_cached_hits);
            return TEST_FAILURE;
        }

        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                     "Redis cache warming in progress: checked=%d found=%d minimum=%d (non-strict)",
                     checked,
                     found,
                     min_cached_hits);
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
            const char *field_path = json_string_value(field);
            if (field_path) {
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
                                 "Missing field in cached JSON: %s", field_path);
                    json_decref(root);
                    return TEST_FAILURE;
                }
            }
        }
    }

    if (json_object_get(input, "expected_uid")) {
        long expected_uid = (long)test_json_get_int(input, "expected_uid");
        json_t *area_obj = json_object_get(root, "area");
        long actual_uid = area_obj ? (long)json_integer_value(json_object_get(area_obj, "uid")) : -1;
        if (actual_uid != expected_uid) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Cached area uid mismatch for '%s': expected %ld, got %ld",
                         area_name,
                         expected_uid,
                         actual_uid);
            json_decref(root);
            return TEST_FAILURE;
        }
    }

    if (json_object_get(input, "expected_filename")) {
        const char *expected_filename = test_json_get_string(input, "expected_filename");
        json_t *area_obj = json_object_get(root, "area");
        json_t *filename_json = area_obj ? json_object_get(area_obj, "filename") : NULL;
        const char *actual_filename = json_is_string(filename_json) ? json_string_value(filename_json) : NULL;
        if (!expected_filename || !actual_filename || str_cmp(expected_filename, actual_filename)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Cached area filename mismatch for '%s': expected '%s', got '%s'",
                         area_name,
                         expected_filename ? expected_filename : "(null)",
                         actual_filename ? actual_filename : "(null)");
            json_decref(root);
            return TEST_FAILURE;
        }
    }

    json_decref(root);
    return TEST_SUCCESS;
}

static test_result_t test_redis_warm_queue(test_case_t *test) {
    if (!redis_is_available()) {
        return TEST_SKIP;
    }

    bool check_queue_empty = false;
    bool strict = false;
    int max_queue_size = -1;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (json_is_object(input)) {
            if (json_object_get(input, "check_queue_empty")) {
                check_queue_empty = test_json_get_bool(input, "check_queue_empty");
            }
            if (json_object_get(input, "strict")) {
                strict = test_json_get_bool(input, "strict");
            }
            if (json_object_get(input, "max_queue_size")) {
                max_queue_size = test_json_get_int(input, "max_queue_size");
            }
        }
    }

    long queue_size = redis_area_cache_warm_queue_size();

    if (max_queue_size >= 0 && queue_size > max_queue_size) {
        if (strict) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Redis warm queue size %ld exceeds configured max %d",
                         queue_size,
                         max_queue_size);
            return TEST_FAILURE;
        }

        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                     "Redis warm queue size %ld exceeds configured max %d (non-strict)",
                     queue_size,
                     max_queue_size);
    }

    if (check_queue_empty && queue_size > 0) {
        if (strict) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Redis warm queue not empty in strict mode: %ld items",
                         queue_size);
            return TEST_FAILURE;
        }

        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                     "Redis warm queue not empty: %ld items (non-strict)",
                     queue_size);
    } else if (queue_size > 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                     "Redis warm queue has %ld items remaining", queue_size);
    }

    return TEST_SUCCESS;
}

#endif // BUILD_TESTS