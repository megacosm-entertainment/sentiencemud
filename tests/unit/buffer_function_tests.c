#ifdef BUILD_TESTS

#include <string.h>

#include "../framework/test_framework.h"
#include "../../log.h"
#include "buffer_function_cases.h"

test_result_t run_buffer_function_test_case(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *func_name;
    size_t index;
    json_t *test_case;

    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Buffer function test missing configuration");
        return TEST_ERROR;
    }

    input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Buffer function test missing input");
        return TEST_ERROR;
    }

    func_name = test_json_get_string(input, "function");
    if (!func_name || strcmp(func_name, "buffer_functions") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Unsupported buffer function group: %s",
                     func_name ? func_name : "(null)");
        return TEST_SKIP;
    }

    test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "buffer_functions requires test_cases array");
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, test_case) {
        const char *scenario = test_json_get_string(test_case, "scenario");
        test_result_t result;

        if (!scenario) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "buffer_functions test case missing scenario");
            return TEST_ERROR;
        }

        if (is_buffer_core_scenario(scenario)) {
            result = run_buffer_core_scenario(scenario, test_case);
            if (result != TEST_SUCCESS)
                return result;
            continue;
        }

        if (is_buffer_permutation_scenario(scenario)) {
            result = run_buffer_permutation_scenario(scenario, test_case);
            if (result != TEST_SUCCESS)
                return result;
            continue;
        }

        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Unsupported buffer_functions scenario: %s",
                     scenario);
        return TEST_ERROR;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
