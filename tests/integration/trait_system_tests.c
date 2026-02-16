/**
 * Trait System Tests
 * 
 * Tests the multi-layer trait system including:
 * - Trait definition loading and counting
 * - Trait lookup by ID and name
 * - Trait definition structural integrity
 * - Trait type coverage (bool/int/string)
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../traits.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_trait_def_count(test_case_t *test);
static test_result_t test_trait_def_lookup(test_case_t *test);
static test_result_t test_trait_def_integrity(test_case_t *test);
static test_result_t test_trait_type_coverage(test_case_t *test);

/**
 * Main test dispatcher for trait system tests
 */
test_result_t run_trait_system_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "trait_def_count_test") == 0) {
        result = test_trait_def_count(test);
    }
    else if (strcmp(test->test_type, "trait_def_lookup_test") == 0) {
        result = test_trait_def_lookup(test);
    }
    else if (strcmp(test->test_type, "trait_def_integrity_test") == 0) {
        result = test_trait_def_integrity(test);
    }
    else if (strcmp(test->test_type, "trait_type_coverage_test") == 0) {
        result = test_trait_type_coverage(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown trait system test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test that trait definitions have been loaded
 */
static test_result_t test_trait_def_count(test_case_t *test)
{
    int min_expected = 1;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = json_get_int(input, "minimum_expected");
            if (cfg_min > 0) min_expected = cfg_min;
        }
    }

    if (trait_def_count < min_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "trait_def_count = %d, below minimum expected %d",
                     trait_def_count, min_expected);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Trait definition count: %d (minimum %d)",
                 trait_def_count, min_expected);
    return TEST_SUCCESS;
}

/**
 * Test trait definition lookup by ID
 */
static test_result_t test_trait_def_lookup(test_case_t *test)
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
        const char *id = json_get_string(tc, "id");
        bool should_find = json_get_bool(tc, "should_find");

        if (!id) continue;

        TRAIT_DEF *def = trait_def_lookup(id);

        if (should_find && !def) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "trait_def_lookup('%s') returned NULL, expected match", id);
            return TEST_FAILURE;
        }

        if (!should_find && def) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "trait_def_lookup('%s') returned match, expected NULL", id);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test trait definition structural integrity
 */
static test_result_t test_trait_def_integrity(test_case_t *test)
{
    if (trait_def_count <= 0) {
        return TEST_SKIP;
    }

    /* Track IDs for uniqueness check */
    #define MAX_TRAIT_CHECK 512
    const char *seen_ids[MAX_TRAIT_CHECK];
    int seen_count = 0;

    TRAIT_DEF *def;
    for (def = trait_def_list; def; def = def->next) {
        if (!def->valid) continue;

        /* Must have an ID */
        if (!def->id || strlen(def->id) == 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Trait definition at index %d has empty ID", def->index);
            return TEST_FAILURE;
        }

        /* Must have a name */
        if (!def->name || strlen(def->name) == 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Trait '%s' has empty name", def->id);
            return TEST_FAILURE;
        }

        /* Valid type */
        if (def->type != TRAIT_BOOLEAN && def->type != TRAIT_INTEGER &&
            def->type != TRAIT_STRING) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Trait '%s' has invalid type %d", def->id, def->type);
            return TEST_FAILURE;
        }

        /* Check for duplicate IDs */
        for (int i = 0; i < seen_count && i < MAX_TRAIT_CHECK; i++) {
            if (strcmp(seen_ids[i], def->id) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate trait ID: %s", def->id);
                return TEST_FAILURE;
            }
        }

        if (seen_count < MAX_TRAIT_CHECK) {
            seen_ids[seen_count++] = def->id;
        }
    }
    #undef MAX_TRAIT_CHECK

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d trait definitions have valid structure", seen_count);
    return TEST_SUCCESS;
}

/**
 * Test that trait definitions cover multiple types
 */
static test_result_t test_trait_type_coverage(test_case_t *test)
{
    bool has_bool = false;
    bool has_int = false;
    bool has_string = false;

    TRAIT_DEF *def;
    for (def = trait_def_list; def; def = def->next) {
        if (!def->valid) continue;

        switch (def->type) {
            case TRAIT_BOOLEAN: has_bool = true; break;
            case TRAIT_INTEGER: has_int = true; break;
            case TRAIT_STRING:  has_string = true; break;
        }
    }

    if (!has_bool) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "No boolean traits found in definitions");
    }

    if (!has_int) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "No integer traits found in definitions");
    }

    /* Boolean and integer are the most common; string is optional */
    if (!has_bool && !has_int) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Neither boolean nor integer traits found");
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Trait type coverage: bool=%s, int=%s, string=%s",
                 has_bool ? "yes" : "no",
                 has_int ? "yes" : "no",
                 has_string ? "yes" : "no");
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
