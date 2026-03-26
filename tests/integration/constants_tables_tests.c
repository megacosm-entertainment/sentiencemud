/**
 * Constants and Tables Data Integrity Tests
 * 
 * Integration tests that validate game constant tables are correctly
 * populated at runtime. Tests table non-emptiness, entry existence,
 * name uniqueness, and sentinel termination.
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../tables.h"
#include "../../magic.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_table_nonempty(test_case_t *test);
static test_result_t test_table_entry_exists(test_case_t *test);
static test_result_t test_table_unique_names(test_case_t *test);
static test_result_t test_table_sentinel(test_case_t *test);

/* Helper functions */
static int count_position_entries(void);
static int count_sex_entries(void);
static int count_size_entries(void);
static int count_act_flags_entries(void);
static int count_do_func_entries(void);
static bool position_has_entry(const char *name);
static bool sex_has_entry(const char *name);
static bool size_has_entry(const char *name);
static bool act_flags_has_entry(const char *name);
static bool do_func_has_entry(const char *name);
static bool position_has_unique_names(void);
static bool sex_has_unique_names(void);
static bool size_has_unique_names(void);
static bool act_flags_has_unique_names(void);
static bool do_func_has_unique_names(void);
static bool position_has_null_sentinel(void);
static bool sex_has_null_sentinel(void);
static bool size_has_null_sentinel(void);
static bool act_flags_has_null_sentinel(void);
static bool do_func_has_null_sentinel(void);

/**
 * Main test dispatcher for constants/tables tests
 */
test_result_t run_constants_tables_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "const_table_nonempty_test") == 0) {
        return test_table_nonempty(test);
    }
    else if (strcmp(test->test_type, "const_table_entry_exists_test") == 0) {
        return test_table_entry_exists(test);
    }
    else if (strcmp(test->test_type, "const_table_unique_names_test") == 0) {
        return test_table_unique_names(test);
    }
    else if (strcmp(test->test_type, "const_table_sentinel_test") == 0) {
        return test_table_sentinel(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Unknown constants tables test type: %s", test->test_type);
        return TEST_ERROR;
    }
}

/**
 * Test that a table has at least one entry
 */
static test_result_t test_table_nonempty(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No input object in test config");
        return TEST_ERROR;
    }

    const char *table_name = test_json_get_string(input, "table_name");
    if (!table_name) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No table_name specified in test input");
        return TEST_ERROR;
    }

    int count = 0;

    if (strcmp(table_name, "position_table") == 0) {
        count = count_position_entries();
    }
    else if (strcmp(table_name, "sex_table") == 0) {
        count = count_sex_entries();
    }
    else if (strcmp(table_name, "size_table") == 0) {
        count = count_size_entries();
    }
    else if (strcmp(table_name, "act_flags") == 0) {
        count = count_act_flags_entries();
    }
    else if (strcmp(table_name, "do_func_table") == 0) {
        count = count_do_func_entries();
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Unknown table name: %s", table_name);
        return TEST_ERROR;
    }

    TEST_ASSERT_INT_GT(count, 0);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Table %s has %d entries", table_name, count);

    return TEST_SUCCESS;
}

/**
 * Test that a specific entry exists in a table
 */
static test_result_t test_table_entry_exists(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No input object in test config");
        return TEST_ERROR;
    }

    const char *table_name = test_json_get_string(input, "table_name");
    const char *entry_name = test_json_get_string(input, "entry_name");

    if (!table_name || !entry_name) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Missing table_name or entry_name in test input");
        return TEST_ERROR;
    }

    bool found = false;

    if (strcmp(table_name, "position_table") == 0) {
        found = position_has_entry(entry_name);
    }
    else if (strcmp(table_name, "sex_table") == 0) {
        found = sex_has_entry(entry_name);
    }
    else if (strcmp(table_name, "size_table") == 0) {
        found = size_has_entry(entry_name);
    }
    else if (strcmp(table_name, "act_flags") == 0) {
        found = act_flags_has_entry(entry_name);
    }
    else if (strcmp(table_name, "do_func_table") == 0) {
        found = do_func_has_entry(entry_name);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Unknown table name: %s", table_name);
        return TEST_ERROR;
    }

    TEST_ASSERT_TRUE(found);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Entry '%s' found in table %s", entry_name, table_name);

    return TEST_SUCCESS;
}

/**
 * Test that a table has unique names
 */
static test_result_t test_table_unique_names(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No input object in test config");
        return TEST_ERROR;
    }

    const char *table_name = test_json_get_string(input, "table_name");
    if (!table_name) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No table_name specified in test input");
        return TEST_ERROR;
    }

    bool unique = false;

    if (strcmp(table_name, "position_table") == 0) {
        unique = position_has_unique_names();
    }
    else if (strcmp(table_name, "sex_table") == 0) {
        unique = sex_has_unique_names();
    }
    else if (strcmp(table_name, "size_table") == 0) {
        unique = size_has_unique_names();
    }
    else if (strcmp(table_name, "act_flags") == 0) {
        unique = act_flags_has_unique_names();
    }
    else if (strcmp(table_name, "do_func_table") == 0) {
        unique = do_func_has_unique_names();
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Unknown table name: %s", table_name);
        return TEST_ERROR;
    }

    TEST_ASSERT_TRUE(unique);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Table %s has unique names", table_name);

    return TEST_SUCCESS;
}

/**
 * Test that a table is NULL-terminated
 */
static test_result_t test_table_sentinel(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No input object in test config");
        return TEST_ERROR;
    }

    const char *table_name = test_json_get_string(input, "table_name");
    if (!table_name) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "No table_name specified in test input");
        return TEST_ERROR;
    }

    bool has_sentinel = false;

    if (strcmp(table_name, "position_table") == 0) {
        has_sentinel = position_has_null_sentinel();
    }
    else if (strcmp(table_name, "sex_table") == 0) {
        has_sentinel = sex_has_null_sentinel();
    }
    else if (strcmp(table_name, "size_table") == 0) {
        has_sentinel = size_has_null_sentinel();
    }
    else if (strcmp(table_name, "act_flags") == 0) {
        has_sentinel = act_flags_has_null_sentinel();
    }
    else if (strcmp(table_name, "do_func_table") == 0) {
        has_sentinel = do_func_has_null_sentinel();
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                      "Unknown table name: %s", table_name);
        return TEST_ERROR;
    }

    TEST_ASSERT_TRUE(has_sentinel);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Table %s is properly NULL-terminated", table_name);

    return TEST_SUCCESS;
}

/* Helper function implementations */

static int count_position_entries(void)
{
    int count = 0;
    for (int i = 0; position_table[i].name != NULL; i++) {
        count++;
    }
    return count;
}

static int count_sex_entries(void)
{
    int count = 0;
    for (int i = 0; sex_table[i].name != NULL; i++) {
        count++;
    }
    return count;
}

static int count_size_entries(void)
{
    int count = 0;
    for (int i = 0; size_table[i].name != NULL; i++) {
        count++;
    }
    return count;
}

static int count_act_flags_entries(void)
{
    int count = 0;
    for (int i = 0; act_flags[i].name != NULL; i++) {
        count++;
    }
    return count;
}

static int count_do_func_entries(void)
{
    int count = 0;
    for (int i = 0; do_func_table[i].name != NULL; i++) {
        count++;
    }
    return count;
}

static bool position_has_entry(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; position_table[i].name != NULL; i++) {
        if (strcmp(position_table[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool sex_has_entry(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; sex_table[i].name != NULL; i++) {
        if (strcmp(sex_table[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool size_has_entry(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; size_table[i].name != NULL; i++) {
        if (strcmp(size_table[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool act_flags_has_entry(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; act_flags[i].name != NULL; i++) {
        if (strcmp(act_flags[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool do_func_has_entry(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; do_func_table[i].name != NULL; i++) {
        if (strcmp(do_func_table[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool position_has_unique_names(void)
{
    for (int i = 0; position_table[i].name != NULL; i++) {
        for (int j = i + 1; position_table[j].name != NULL; j++) {
            if (strcmp(position_table[i].name, position_table[j].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate position name found: '%s' at indices %d and %d",
                              position_table[i].name, i, j);
                return false;
            }
        }
    }
    return true;
}

static bool sex_has_unique_names(void)
{
    for (int i = 0; sex_table[i].name != NULL; i++) {
        for (int j = i + 1; sex_table[j].name != NULL; j++) {
            if (strcmp(sex_table[i].name, sex_table[j].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate sex name found: '%s' at indices %d and %d",
                              sex_table[i].name, i, j);
                return false;
            }
        }
    }
    return true;
}

static bool size_has_unique_names(void)
{
    for (int i = 0; size_table[i].name != NULL; i++) {
        for (int j = i + 1; size_table[j].name != NULL; j++) {
            if (strcmp(size_table[i].name, size_table[j].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate size name found: '%s' at indices %d and %d",
                              size_table[i].name, i, j);
                return false;
            }
        }
    }
    return true;
}

static bool act_flags_has_unique_names(void)
{
    for (int i = 0; act_flags[i].name != NULL; i++) {
        for (int j = i + 1; act_flags[j].name != NULL; j++) {
            if (strcmp(act_flags[i].name, act_flags[j].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate act_flags name found: '%s' at indices %d and %d",
                              act_flags[i].name, i, j);
                return false;
            }
        }
    }
    return true;
}

static bool do_func_has_unique_names(void)
{
    for (int i = 0; do_func_table[i].name != NULL; i++) {
        for (int j = i + 1; do_func_table[j].name != NULL; j++) {
            if (strcmp(do_func_table[i].name, do_func_table[j].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Duplicate do_func_table name found: '%s' at indices %d and %d",
                              do_func_table[i].name, i, j);
                return false;
            }
        }
    }
    return true;
}

static bool position_has_null_sentinel(void)
{
    // Find the last non-NULL entry and check that the next one is NULL
    int last_valid = -1;
    for (int i = 0; position_table[i].name != NULL; i++) {
        last_valid = i;
    }
    
    // If we found at least one entry, check the sentinel
    if (last_valid >= 0) {
        return position_table[last_valid + 1].name == NULL;
    }
    
    // Empty table should still have sentinel at position 0
    return position_table[0].name == NULL;
}

static bool sex_has_null_sentinel(void)
{
    int last_valid = -1;
    for (int i = 0; sex_table[i].name != NULL; i++) {
        last_valid = i;
    }
    
    if (last_valid >= 0) {
        return sex_table[last_valid + 1].name == NULL;
    }
    
    return sex_table[0].name == NULL;
}

static bool size_has_null_sentinel(void)
{
    int last_valid = -1;
    for (int i = 0; size_table[i].name != NULL; i++) {
        last_valid = i;
    }
    
    if (last_valid >= 0) {
        return size_table[last_valid + 1].name == NULL;
    }
    
    return size_table[0].name == NULL;
}

static bool act_flags_has_null_sentinel(void)
{
    int last_valid = -1;
    for (int i = 0; act_flags[i].name != NULL; i++) {
        last_valid = i;
    }
    
    if (last_valid >= 0) {
        return act_flags[last_valid + 1].name == NULL;
    }
    
    return act_flags[0].name == NULL;
}

static bool do_func_has_null_sentinel(void)
{
    int last_valid = -1;
    for (int i = 0; do_func_table[i].name != NULL; i++) {
        last_valid = i;
    }
    
    if (last_valid >= 0) {
        return do_func_table[last_valid + 1].name == NULL &&
               do_func_table[last_valid + 1].func == NULL;
    }
    
    return do_func_table[0].name == NULL && do_func_table[0].func == NULL;
}

#endif /* BUILD_TESTS */