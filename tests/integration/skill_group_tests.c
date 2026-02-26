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
            int cfg_min = test_json_get_int(input, "minimum_expected");
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
        bool use_first_loaded = false;
        const char *lookup_mode = test_json_get_string(tc, "lookup_mode");
        const char *name = test_json_get_string(tc, "name");
        bool should_find = test_json_get_bool(tc, "should_find");
        const char *expected_name = test_json_get_string(tc, "expected_name");

        SKILL_GROUP *group = NULL;

        if (!lookup_mode) {
            lookup_mode = "exact";
        }

        if (json_object_get(tc, "use_first_loaded")) {
            use_first_loaded = test_json_get_bool(tc, "use_first_loaded");
        }

        if (use_first_loaded) {
            SKILL_GROUP *first = skill_group_first();
            if (!first || !first->name || first->name[0] == '\0') {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "skill_group_lookup_test requested use_first_loaded but no valid first group exists");
                return TEST_FAILURE;
            }

            if (!str_cmp(lookup_mode, "prefix")) {
                char prefix[4];
                size_t len = strlen(first->name);
                if (len == 0) {
                    log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                "First skill group has empty name for prefix lookup");
                    return TEST_FAILURE;
                }

                prefix[0] = first->name[0];
                prefix[1] = (len > 1) ? first->name[1] : '\0';
                prefix[2] = (len > 2) ? first->name[2] : '\0';
                prefix[3] = '\0';
                group = skill_group_search(prefix);
            } else {
                group = skill_group_find(first->name);
            }
        } else {
            if (!name) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "skill_group_lookup_test case missing 'name' when use_first_loaded is false");
                return TEST_ERROR;
            }

            if (!str_cmp(lookup_mode, "prefix")) {
                group = skill_group_search(name);
            } else {
                group = skill_group_find(name);
            }
        }

        if (should_find && !group) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill group lookup failed (mode=%s, name=%s), expected match",
                         lookup_mode,
                         name ? name : "<dynamic-first>");
            return TEST_FAILURE;
        }

        if (!should_find && group) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill group lookup returned match (mode=%s, name=%s), expected NULL",
                         lookup_mode,
                         name ? name : "<dynamic-first>");
            return TEST_FAILURE;
        }

        if (should_find && expected_name) {
            if (!group->name || str_cmp(group->name, expected_name)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "skill group lookup expected name '%s', got '%s'",
                             expected_name,
                             group->name ? group->name : "(null)");
                return TEST_FAILURE;
            }
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test iteration over all skill groups
 */
static test_result_t test_skill_group_iteration(test_case_t *test)
{
    (void)test;
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

    for (group = skill_group_first(); group; group = group->next) {
        SKILL_GROUP *other;

        if (!group->valid || !group->name || group->name[0] == '\0') {
            continue;
        }

        for (other = group->next; other; other = other->next) {
            if (!other->valid || !other->name || other->name[0] == '\0') {
                continue;
            }

            if (!str_cmp(group->name, other->name)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate skill group name detected: '%s'",
                             group->name);
                return TEST_FAILURE;
            }
        }
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Iterated %d skill groups, all have names", iterated);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
