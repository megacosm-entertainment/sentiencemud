#ifdef BUILD_TESTS

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "test_framework.h"
#include "test_modules.h"
#include "../../log.h"

static void print_test_result(test_case_t *test, test_result_t result, clock_t start_time);

static const char *presence_to_string(bool present)
{
    return present ? "present" : "missing";
}

static const char *result_to_expected_label(test_result_t expected_result)
{
    switch (expected_result) {
        case TEST_SUCCESS: return "PASS";
        case TEST_FAILURE: return "FAIL";
        case TEST_ERROR:   return "ERROR";
        case TEST_SKIP:    return "SKIP";
        default:           return "UNKNOWN";
    }
}

static void log_json_compact_snippet(const char *label, json_t *value)
{
    if (!label || !value) {
        return;
    }

    char *dump = json_dumps(value, JSON_COMPACT);
    if (!dump) {
        return;
    }

    char snippet[257];
    size_t len = strlen(dump);
    if (len <= 256) {
        snprintf(snippet, sizeof(snippet), "%s", dump);
    } else {
        snprintf(snippet, sizeof(snippet), "%.252s...", dump);
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "    %s=%s", label, snippet);
    free(dump);
}

test_result_t run_test_case(test_case_t *test)
{
    if (!test) {
        return TEST_ERROR;
    }

    test_config_t *config = get_test_config();
    bool show_names = config ? config->verbose_test_names : true;
    bool show_details = config ? config->verbose_test_details : false;
    bool show_timing = config ? config->show_execution_time : true;

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

    clock_t start_time = 0;
    if (show_timing) {
        start_time = clock();
    }

    test_result_t result = TEST_SKIP;

    if (test->test_type) {
        if (strcmp(test->test_type, "pure_function_test") == 0) {
            result = run_pure_function_test_case(test);
        } else if (strcmp(test->test_type, "buffer_function_test") == 0) {
            result = run_buffer_function_test_case(test);
        } else if (strstr(test->test_type, "string_editor_") != NULL) {
            result = run_string_editor_test_case(test);
        } else if (strcmp(test->test_type, "reset_cross_area_creation") == 0 ||
            strcmp(test->test_type, "reset_serialization") == 0 ||
            strcmp(test->test_type, "reset_legacy_vnum") == 0) {
            result = run_reset_test_case(test);
        } else if (strcmp(test->test_type, "shop_stock_cross_area_creation") == 0 ||
                   strcmp(test->test_type, "shop_stock_serialization") == 0 ||
                   strcmp(test->test_type, "shop_stock_legacy_vnum") == 0 ||
                   strcmp(test->test_type, "shop_stock_reference_integrity") == 0) {
            result = run_shop_stock_test_case(test);
        } else if (strstr(test->test_type, "church_") != NULL) {
            result = run_church_test_case(test);
        } else if (strstr(test->test_type, "instance_") != NULL ||
                   strstr(test->test_type, "blueprint_") != NULL ||
                   strstr(test->test_type, "dungeon_") != NULL ||
                   strstr(test->test_type, "ship_") != NULL ||
                   strstr(test->test_type, "wnum_json_") != NULL ||
                   strstr(test->test_type, "persist_directory") != NULL) {
            result = run_instance_test_case(test);
        } else if (strstr(test->test_type, "chat_room_") != NULL) {
            result = run_chat_room_test_case(test);
        } else if (strstr(test->test_type, "skill_group_") != NULL) {
            result = run_skill_group_test_case(test);
        } else if (strstr(test->test_type, "skill_") != NULL ||
                   strstr(test->test_type, "spell_fun_") != NULL) {
            result = run_skill_data_test_case(test);
        } else if (strcmp(test->test_type, "damage_class_lookup_test") == 0) {
            result = run_lookup_table_test_case(test);
        } else if (strstr(test->test_type, "class_") != NULL) {
            result = run_class_data_test_case(test);
        } else if (strstr(test->test_type, "item_type_") != NULL) {
            result = run_item_type_test_case(test);
        } else if (strstr(test->test_type, "song_") != NULL) {
            result = run_song_data_test_case(test);
        } else if (strstr(test->test_type, "trait_") != NULL) {
            result = run_trait_system_test_case(test);
        } else if (strstr(test->test_type, "script_engine_") != NULL) {
            result = run_script_engine_test_case(test);
        } else if (strstr(test->test_type, "channel_") != NULL) {
            result = run_channel_pubsub_test_case(test);
        } else if (strcmp(test->test_type, "reserved_lookup_test") == 0 ||
                   strcmp(test->test_type, "reserved_wnum_format_test") == 0 ||
                   strcmp(test->test_type, "reserved_compat_test") == 0) {
            result = run_wnum_test_case(test);
        } else if (strstr(test->test_type, "_lookup_test") != NULL ||
                   strstr(test->test_type, "flag_table_") != NULL) {
            result = run_lookup_table_test_case(test);
        } else {
            result = run_wnum_test_case(test);
        }
    } else if (test->execute) {
        result = test->execute(test);
    }

    print_test_result(test, result, start_time);
    return result;
}

static void print_test_result(test_case_t *test, test_result_t result, clock_t start_time)
{
    test_config_t *config = get_test_config();
    bool show_names = config ? config->verbose_test_names : true;
    bool show_details = config ? config->verbose_test_details : false;
    bool show_timing = config ? config->show_execution_time : true;
    bool verbose = test->verbose_output || (config ? config->verbose_output : false);

    const char *result_str = test_result_to_string(result);
    double elapsed = 0.0;
    test_result_t expected_result = TEST_SUCCESS;
    bool expected_result_known = false;
    bool has_input = false;
    bool has_expected_output = false;
    json_t *input = NULL;
    json_t *expected_output = NULL;

    if (test && test->config && json_is_object(test->config)) {
        input = json_object_get(test->config, "input");
        expected_output = json_object_get(test->config, "expected_output");
        has_input = (input != NULL);
        has_expected_output = (expected_output != NULL);

        if (expected_output && json_is_object(expected_output)) {
            json_t *success = json_object_get(expected_output, "success");
            if (json_is_boolean(success)) {
                expected_result_known = true;
                expected_result = json_is_true(success) ? TEST_SUCCESS : TEST_FAILURE;
            }
        }
    }

    if (result == TEST_SKIP) {
        expected_result = TEST_SKIP;
        expected_result_known = true;
    }

    if (show_timing && start_time > 0) {
        elapsed = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
    }

    if (show_names) {
        if (show_timing && start_time > 0) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "%s (%.3fs)", result_str, elapsed);
        } else {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "%s", result_str);
        }

        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                      "    contract: input=%s expected_output=%s expected=%s actual=%s",
                      presence_to_string(has_input),
                      presence_to_string(has_expected_output),
                      expected_result_known ? result_to_expected_label(expected_result) : "(unspecified)",
                      result_str);

        if (verbose || show_details) {
            if (input) {
                log_json_compact_snippet("input", input);
            }
            if (expected_output) {
                log_json_compact_snippet("expected_output", expected_output);
            }
        }
    }

    if (verbose) {
        if (show_timing && start_time > 0) {
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

    test_log_test_result(test,
                         result,
                         elapsed,
                         has_input,
                         has_expected_output,
                         expected_result_known,
                         expected_result);
}

#endif /* BUILD_TESTS */