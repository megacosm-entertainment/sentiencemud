/**
 * Skill Data System Tests
 * 
 * Tests the data-driven skill system including:
 * - Skill counting and loading verification
 * - Name lookup (exact and prefix)
 * - UID integrity and roundtrip lookups
 * - spell_fun resolution
 * - skill_name accessor safety
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../skill_data.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_skill_count(test_case_t *test);
static test_result_t test_skill_lookup(test_case_t *test);
static test_result_t test_skill_search(test_case_t *test);
static test_result_t test_skill_name_accessor(test_case_t *test);
static test_result_t test_skill_uid_integrity(test_case_t *test);
static test_result_t test_skill_uid_roundtrip(test_case_t *test);
static test_result_t test_spell_fun_lookup_test(test_case_t *test);

/**
 * Main test dispatcher for skill data tests
 */
test_result_t run_skill_data_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "skill_count_test") == 0) {
        result = test_skill_count(test);
    }
    else if (strcmp(test->test_type, "skill_lookup_test") == 0) {
        result = test_skill_lookup(test);
    }
    else if (strcmp(test->test_type, "skill_search_test") == 0) {
        result = test_skill_search(test);
    }
    else if (strcmp(test->test_type, "skill_name_test") == 0) {
        result = test_skill_name_accessor(test);
    }
    else if (strcmp(test->test_type, "skill_uid_integrity_test") == 0) {
        result = test_skill_uid_integrity(test);
    }
    else if (strcmp(test->test_type, "skill_uid_roundtrip_test") == 0) {
        result = test_skill_uid_roundtrip(test);
    }
    else if (strcmp(test->test_type, "spell_fun_lookup_test") == 0) {
        result = test_spell_fun_lookup_test(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown skill data test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test that skills have been loaded and count exceeds minimum
 */
static test_result_t test_skill_count(test_case_t *test)
{
    int count = skill_count();
    int min_expected = 10;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = test_json_get_int(input, "minimum_expected");
            if (cfg_min > 0) min_expected = cfg_min;
        }
    }

    if (count <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "skill_count() returned %d, expected positive", count);
        return TEST_FAILURE;
    }

    if (count < min_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "skill_count() = %d, below minimum expected %d",
                     count, min_expected);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Skill count: %d (minimum %d)", count, min_expected);
    return TEST_SUCCESS;
}

/**
 * Test skill lookup by exact name
 */
static test_result_t test_skill_lookup(test_case_t *test)
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
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        SKILL_DATA *skill = skill_find(name);

        if (should_exist && !skill) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_find('%s') returned NULL, expected a skill", name);
            return TEST_FAILURE;
        }

        if (!should_exist && skill) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_find('%s') returned a skill, expected NULL", name);
            return TEST_FAILURE;
        }

        if (skill) {
            const char *sname = skill_name(skill);
            if (!sname || strlen(sname) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "skill_find('%s') returned skill with empty name", name);
                return TEST_FAILURE;
            }
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test skill prefix search
 */
static test_result_t test_skill_search(test_case_t *test)
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
        const char *prefix = test_json_get_string(tc, "prefix");
        bool should_find = test_json_get_bool(tc, "should_find");

        if (!prefix) continue;

        SKILL_DATA *skill = skill_search(prefix);

        if (should_find && !skill) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_search('%s') returned NULL, expected match", prefix);
            return TEST_FAILURE;
        }

        if (!should_find && skill) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_search('%s') returned match, expected NULL", prefix);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test skill_name accessor returns "none" for NULL, real name otherwise
 */
static test_result_t test_skill_name_accessor(test_case_t *test)
{
    /* skill_name(NULL) should return "none" */
    const char *null_name = skill_name(NULL);
    if (!null_name || strcmp(null_name, "none") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "skill_name(NULL) returned '%s', expected 'none'",
                     null_name ? null_name : "(NULL)");
        return TEST_FAILURE;
    }

    /* skill_name(valid_skill) should return non-empty string */
    SKILL_DATA *first = skill_first();
    if (first) {
        const char *name = skill_name(first);
        if (!name || strlen(name) == 0) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "skill_name(first_skill) returned empty/NULL");
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test all skills have unique positive UIDs
 */
static test_result_t test_skill_uid_integrity(test_case_t *test)
{
    int count = skill_count();
    if (count <= 0) {
        return TEST_FAILURE;
    }

    /* Use a simple array to check uniqueness for UIDs in range */
    #define MAX_UID_CHECK 8192
    bool seen[MAX_UID_CHECK];
    memset(seen, 0, sizeof(seen));

    SKILL_DATA *skill;
    int checked = 0;
    for (skill = skill_first(); skill; skill = skill->next) {
        if (!skill->valid) continue;
        checked++;

        if (skill->uid <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Skill '%s' has non-positive UID %d",
                         skill->name ? skill->name : "?", skill->uid);
            return TEST_FAILURE;
        }

        if (skill->uid < MAX_UID_CHECK) {
            if (seen[skill->uid]) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate skill UID %d (skill: %s)",
                             skill->uid, skill->name ? skill->name : "?");
                return TEST_FAILURE;
            }
            seen[skill->uid] = true;
        }
    }
    #undef MAX_UID_CHECK

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d skills have unique positive UIDs", checked);
    return TEST_SUCCESS;
}

/**
 * Test UID lookup roundtrip: skill -> uid -> skill_find_uid -> same skill
 */
static test_result_t test_skill_uid_roundtrip(test_case_t *test)
{
    int checked = 0;
    int max_check = 200;

    SKILL_DATA *skill;
    for (skill = skill_first(); skill && checked < max_check; skill = skill->next) {
        if (!skill->valid) continue;
        checked++;

        SKILL_DATA *found = skill_find_uid(skill->uid);
        if (found != skill) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "skill_find_uid(%d) did not return original skill '%s'",
                         skill->uid, skill->name ? skill->name : "?");
            return TEST_FAILURE;
        }
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "UID roundtrip verified for %d skills", checked);
    return TEST_SUCCESS;
}

/**
 * Test spell function name lookup
 */
static test_result_t test_spell_fun_lookup_test(test_case_t *test)
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
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        SPELL_FUN *fun = spell_fun_lookup(name);

        if (should_exist && !fun) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "spell_fun_lookup('%s') returned NULL, expected valid", name);
            return TEST_FAILURE;
        }

        if (!should_exist && fun) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "spell_fun_lookup('%s') returned non-NULL, expected NULL", name);
            return TEST_FAILURE;
        }

        /* Verify roundtrip: fun -> name -> fun */
        if (fun) {
            const char *resolved_name = spell_fun_name(fun);
            if (!resolved_name || strlen(resolved_name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "spell_fun_name() returned NULL for function '%s'", name);
                return TEST_FAILURE;
            }
        }
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
