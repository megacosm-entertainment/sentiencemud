#ifdef BUILD_TESTS

#include <string.h>
#include <time.h>
#include "test_framework.h"
#include "test_modules.h"
#include "../../log.h"

static void print_test_result(test_case_t *test, test_result_t result, clock_t start_time);

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
        } else if (strstr(test->test_type, "string_editor_") != NULL) {
            result = run_string_editor_test_case(test);
        } else if (strcmp(test->test_type, "reset_cross_area_creation") == 0 ||
            strcmp(test->test_type, "reset_serialization") == 0 ||
            strcmp(test->test_type, "reset_legacy_vnum") == 0) {
            result = run_reset_test_case(test);
        } else if (strcmp(test->test_type, "shop_stock_cross_area_creation") == 0 ||
                   strcmp(test->test_type, "shop_stock_serialization") == 0 ||
                   strcmp(test->test_type, "shop_stock_legacy_vnum") == 0) {
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
        } else if (strstr(test->test_type, "class_") != NULL) {
            result = run_class_data_test_case(test);
        } else if (strstr(test->test_type, "item_type_") != NULL) {
            result = run_item_type_test_case(test);
        } else if (strstr(test->test_type, "song_") != NULL) {
            result = run_song_data_test_case(test);
        } else if (strstr(test->test_type, "trait_") != NULL) {
            result = run_trait_system_test_case(test);
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

#endif /* BUILD_TESTS */