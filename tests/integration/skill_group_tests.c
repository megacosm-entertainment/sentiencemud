/**
 * Skill Group System Tests
 * 
 * Tests the skill group system including:
 * - Group count verification
 * - Name lookup (exact and prefix)
 * - Iteration and structural integrity
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../skill_group.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_skill_group_count(test_case_t *test);
static test_result_t test_skill_group_lookup(test_case_t *test);
static test_result_t test_skill_group_iteration(test_case_t *test);

/**
 * Main test dispatcher for skill group tests
 */
test_result_t run_skill_group_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "skill_group_count_test") == 0) {
        result = test_skill_group_count(test);
    }
    else if (strcmp(test->test_type, "skill_group_lookup_test") == 0) {
        result = test_skill_group_lookup(test);
    }
    else if (strcmp(test->test_type, "skill_group_iteration_test") == 0) {
        result = test_skill_group_iteration(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown skill group test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test that skill groups have been loaded
 */
static test_result_t test_skill_group_count(test_case_t *test)
{
    int count = skill_group_count();
    int min_expected = 1;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = json_get_int(input, "minimum_expected");
            if (cfg_min > 0) min_expected = cfg_min;
        }
    }

    if (count < min_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "skill_group_count() = %d, below minimum expected %d",
                     count, min_expected);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Skill group count: %d (minimum %d)", count, min_expected);
    return TEST_SUCCESS;
}

/**
 * Test skill group lookup by name
 */
static test_result_t test_skill_group_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = json_get_string(tc, "name");
        bool should_find = json_get_bool(tc, "should_find");

        if (!name) continue;

        SKILL_GROUP *group = skill_group_find(name);

        if (should_find && !group) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_group_find('%s') returned NULL, expected match", name);
            return TEST_FAILURE;
        }

        if (!should_find && group) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_group_find('%s') returned match, expected NULL", name);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test iteration over all skill groups
 */
static test_result_t test_skill_group_iteration(test_case_t *test)
{
    int expected_count = skill_group_count();
    int iterated = 0;

    SKILL_GROUP *group;
    for (group = skill_group_first(); group; group = group->next) {
        if (!group->valid) continue;
        iterated++;

        if (!group->name || strlen(group->name) == 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Skill group #%d has empty name", iterated);
            return TEST_FAILURE;
        }
    }

    if (iterated != expected_count) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Iteration count %d != skill_group_count() %d",
                     iterated, expected_count);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Iterated %d skill groups, all have names", iterated);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
