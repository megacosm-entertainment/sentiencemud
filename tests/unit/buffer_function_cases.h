#ifndef TEST_BUFFER_FUNCTION_CASES_H
#define TEST_BUFFER_FUNCTION_CASES_H

#ifdef BUILD_TESTS

#include <jansson.h>
#include "../framework/test_framework.h"

bool is_buffer_core_scenario(const char *scenario);
bool is_buffer_permutation_scenario(const char *scenario);

test_result_t run_buffer_core_scenario(const char *scenario, json_t *test_case);
test_result_t run_buffer_permutation_scenario(const char *scenario, json_t *test_case);

#endif /* BUILD_TESTS */

#endif /* TEST_BUFFER_FUNCTION_CASES_H */
