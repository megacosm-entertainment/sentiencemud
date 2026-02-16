/**
 * Class Data System Tests
 * 
 * Tests the data-driven class system including:
 * - Class counting and loading verification
 * - Name lookup (exact and prefix)
 * - UID integrity and roundtrip lookups
 * - Default class existence
 * - Class type validation
 * - XP table progression
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../tables.h"
#include "../../class_data.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_class_count(test_case_t *test);
static test_result_t test_class_lookup(test_case_t *test);
static test_result_t test_class_default(test_case_t *test);
static test_result_t test_class_uid_integrity(test_case_t *test);
static test_result_t test_class_uid_roundtrip(test_case_t *test);
static test_result_t test_class_name_accessor(test_case_t *test);
static test_result_t test_class_type_valid(test_case_t *test);
static test_result_t test_class_exp_table(test_case_t *test);

/**
 * Main test dispatcher for class data tests
 */
test_result_t run_class_data_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "class_count_test") == 0) {
        result = test_class_count(test);
    }
    else if (strcmp(test->test_type, "class_lookup_test") == 0) {
        result = test_class_lookup(test);
    }
    else if (strcmp(test->test_type, "class_default_test") == 0) {
        result = test_class_default(test);
    }
    else if (strcmp(test->test_type, "class_uid_integrity_test") == 0) {
        result = test_class_uid_integrity(test);
    }
    else if (strcmp(test->test_type, "class_uid_roundtrip_test") == 0) {
        result = test_class_uid_roundtrip(test);
    }
    else if (strcmp(test->test_type, "class_name_test") == 0) {
        result = test_class_name_accessor(test);
    }
    else if (strcmp(test->test_type, "class_type_valid_test") == 0) {
        result = test_class_type_valid(test);
    }
    else if (strcmp(test->test_type, "class_exp_table_test") == 0) {
        result = test_class_exp_table(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown class data test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test that classes have been loaded and count exceeds minimum
 */
static test_result_t test_class_count(test_case_t *test)
{
    int count = class_count();
    int min_expected = 3;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = test_json_get_int(input, "minimum_expected");
            if (cfg_min > 0) min_expected = cfg_min;
        }
    }

    if (count <= 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "class_count() returned %d, expected positive", count);
        return TEST_FAILURE;
    }

    if (count < min_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "class_count() = %d, below minimum expected %d",
                     count, min_expected);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Class count: %d (minimum %d)", count, min_expected);
    return TEST_SUCCESS;
}

/**
 * Test class lookup by name
 */
static test_result_t test_class_lookup(test_case_t *test)
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

        CLASS_DATA *clazz = class_find(name);

        if (should_exist && !clazz) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "class_find('%s') returned NULL, expected a class", name);
            return TEST_FAILURE;
        }

        if (!should_exist && clazz) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "class_find('%s') returned a class, expected NULL", name);
            return TEST_FAILURE;
        }

        if (clazz) {
            const char *cname = class_name(clazz);
            if (!cname || strlen(cname) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "class_find('%s') returned class with empty name", name);
                return TEST_FAILURE;
            }
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test that a default class is defined
 */
static test_result_t test_class_default(test_case_t *test)
{
    CLASS_DATA *def = class_get_default();

    if (!def) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "class_get_default() returned NULL");
        return TEST_FAILURE;
    }

    const char *name = class_name(def);
    if (!name || strlen(name) == 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Default class has empty name");
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Default class: %s", name);
    return TEST_SUCCESS;
}

/**
 * Test all classes have unique positive UIDs
 */
static test_result_t test_class_uid_integrity(test_case_t *test)
{
    int count = class_count();
    if (count <= 0) {
        return TEST_FAILURE;
    }

    #define MAX_UID_CHECK 1024
    bool seen[MAX_UID_CHECK];
    memset(seen, 0, sizeof(seen));

    int checked = 0;
    CLASS_DATA *clazz;
    for (clazz = class_first(); clazz; clazz = clazz->next) {
        if (!clazz->valid) continue;
        checked++;

        if (clazz->uid <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Class '%s' has non-positive UID %d",
                         clazz->name ? clazz->name : "?", clazz->uid);
            return TEST_FAILURE;
        }

        if (clazz->uid < MAX_UID_CHECK) {
            if (seen[clazz->uid]) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate class UID %d (class: %s)",
                             clazz->uid, clazz->name ? clazz->name : "?");
                return TEST_FAILURE;
            }
            seen[clazz->uid] = true;
        }
    }
    #undef MAX_UID_CHECK

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d classes have unique positive UIDs", checked);
    return TEST_SUCCESS;
}

/**
 * Test UID lookup roundtrip
 */
static test_result_t test_class_uid_roundtrip(test_case_t *test)
{
    int checked = 0;
    CLASS_DATA *clazz;

    for (clazz = class_first(); clazz; clazz = clazz->next) {
        if (!clazz->valid) continue;
        checked++;

        CLASS_DATA *found = class_find_uid(clazz->uid);
        if (found != clazz) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "class_find_uid(%d) did not return original class '%s'",
                         clazz->uid, clazz->name ? clazz->name : "?");
            return TEST_FAILURE;
        }
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "UID roundtrip verified for %d classes", checked);
    return TEST_SUCCESS;
}

/**
 * Test class_name accessor
 */
static test_result_t test_class_name_accessor(test_case_t *test)
{
    /* class_name(NULL) should return "none" */
    const char *null_name = class_name(NULL);
    if (!null_name || strcmp(null_name, "none") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "class_name(NULL) returned '%s', expected 'none'",
                     null_name ? null_name : "(NULL)");
        return TEST_FAILURE;
    }

    /* class_name(valid_class) should return non-empty string */
    CLASS_DATA *first = class_first();
    if (first) {
        const char *name = class_name(first);
        if (!name || strlen(name) == 0) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "class_name(first_class) returned empty/NULL");
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test all classes have a valid class type
 */
static test_result_t test_class_type_valid(test_case_t *test)
{
    int max_type = MAX_CLASS_TYPE;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_max = test_json_get_int(input, "max_class_type");
            if (cfg_max > 0) max_type = cfg_max;
        }
    }

    CLASS_DATA *clazz;
    for (clazz = class_first(); clazz; clazz = clazz->next) {
        if (!clazz->valid) continue;

        if (clazz->type < CLASS_TYPE_NONE || clazz->type >= max_type) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Class '%s' has invalid type %d (max %d)",
                         clazz->name ? clazz->name : "?", clazz->type, max_type);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test XP per level returns reasonable values
 */
static test_result_t test_class_exp_table(test_case_t *test)
{
    CLASS_DATA *clazz = class_first();
    if (!clazz) {
        return TEST_FAILURE;
    }

    /* Verify XP is positive and monotonically increasing for the first class */
    long prev_xp = 0;
    int test_levels[] = {1, 10, 50};
    int num_levels = 3;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        json_t *levels = json_object_get(input, "test_levels");
        if (levels && json_is_array(levels)) {
            num_levels = json_array_size(levels);
            if (num_levels > 3) num_levels = 3;
            for (int i = 0; i < num_levels; i++) {
                json_t *lvl = json_array_get(levels, i);
                if (json_is_integer(lvl)) {
                    test_levels[i] = (int)json_integer_value(lvl);
                }
            }
        }
    }

    for (int i = 0; i < num_levels; i++) {
        int level = test_levels[i];
        if (level > clazz->max_level) continue;

        long xp = class_exp_per_level(clazz, level);

        if (xp <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "class_exp_per_level('%s', %d) returned %ld, expected positive",
                         clazz->name, level, xp);
            return TEST_FAILURE;
        }

        if (xp <= prev_xp && level > 1) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "XP not increasing: level %d = %ld, prev = %ld",
                         level, xp, prev_xp);
            return TEST_FAILURE;
        }

        prev_xp = xp;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
