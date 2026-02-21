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
            int cfg_min = test_json_get_int(input, "minimum_expected");
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
        bool use_first_loaded = false;
        const char *lookup_mode = test_json_get_string(tc, "lookup_mode");
        const char *id = test_json_get_string(tc, "id");
        const char *name = test_json_get_string(tc, "name");
        bool should_find = test_json_get_bool(tc, "should_find");
        TRAIT_DEF *def = NULL;

        if (!lookup_mode) {
            lookup_mode = "id";
        }

        if (json_object_get(tc, "use_first_loaded")) {
            use_first_loaded = test_json_get_bool(tc, "use_first_loaded");
        }

        if (use_first_loaded) {
            TRAIT_DEF *first_valid = NULL;
            for (first_valid = trait_def_list; first_valid; first_valid = first_valid->next) {
                if (first_valid->valid) {
                    break;
                }
            }

            if (!first_valid) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "trait_def_lookup_test requested use_first_loaded but no valid trait definition exists");
                return TEST_FAILURE;
            }

            if (!str_cmp(lookup_mode, "name")) {
                def = trait_def_lookup_name(first_valid->name);
            } else {
                def = trait_def_lookup(first_valid->id);
            }
        } else if (!str_cmp(lookup_mode, "name")) {
            if (!name || name[0] == '\0') {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "trait_def_lookup_test case missing 'name' for lookup_mode=name");
                return TEST_ERROR;
            }
            def = trait_def_lookup_name(name);
        } else {
            if (!id || id[0] == '\0') {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "trait_def_lookup_test case missing 'id' for lookup_mode=id");
                return TEST_ERROR;
            }
            def = trait_def_lookup(id);
        }

        if (should_find && !def) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "trait definition lookup failed (mode=%s, id=%s, name=%s), expected match",
                         lookup_mode,
                         id ? id : "(null)",
                         name ? name : "(null)");
            return TEST_FAILURE;
        }

        if (!should_find && def) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "trait definition lookup returned match (mode=%s, id=%s, name=%s), expected NULL",
                         lookup_mode,
                         id ? id : "(null)",
                         name ? name : "(null)");
            return TEST_FAILURE;
        }

        if (should_find && def) {
            if (!def->id || def->id[0] == '\0' || !def->name || def->name[0] == '\0') {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "Lookup returned trait with missing id/name");
                return TEST_FAILURE;
            }
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test trait definition structural integrity
 */
static test_result_t test_trait_def_integrity(test_case_t *test)
{
    (void)test;
    if (trait_def_count <= 0) {
        return TEST_SKIP;
    }

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

        if (def->index < 0 || def->index >= trait_def_count) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Trait '%s' has out-of-range index %d (trait_def_count=%d)",
                         def->id, def->index, trait_def_count);
            return TEST_FAILURE;
        }

        for (TRAIT_DEF *other = def->next; other; other = other->next) {
            if (!other->valid || !other->id) {
                continue;
            }

            if (strcmp(other->id, def->id) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate trait ID: %s", def->id);
                return TEST_FAILURE;
            }
        }

        seen_count++;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d trait definitions have valid structure", seen_count);
    return TEST_SUCCESS;
}

/**
 * Test that trait definitions cover multiple types
 */
static test_result_t test_trait_type_coverage(test_case_t *test)
{
    bool require_bool = true;
    bool require_int = true;
    bool require_string = false;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (json_is_object(input)) {
            if (json_object_get(input, "require_bool")) {
                require_bool = test_json_get_bool(input, "require_bool");
            }
            if (json_object_get(input, "require_int")) {
                require_int = test_json_get_bool(input, "require_int");
            }
            if (json_object_get(input, "require_string")) {
                require_string = test_json_get_bool(input, "require_string");
            }
        }
    }

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

    if (require_bool && !has_bool) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Trait type coverage missing required boolean traits");
        return TEST_FAILURE;
    }

    if (require_int && !has_int) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Trait type coverage missing required integer traits");
        return TEST_FAILURE;
    }

    if (require_string && !has_string) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Trait type coverage missing required string traits");
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
