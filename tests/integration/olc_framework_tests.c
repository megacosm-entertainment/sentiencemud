/**
 * OLC Framework Integration Tests
 * 
 * Tests the Online Creation (OLC) framework including:
 * - Editor registry and command table integrity
 * - Editor lookup functions  
 * - OLC state management
 * - Editor name table synchronization
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../framework/test_framework.h"
#include "../../editors/common/olc_editor.h"
#include <string.h>

/* External declarations */
extern const struct editor_cmd_type editor_table[];
extern const char * const editor_name_table[];
extern const int editor_max_tabs_table[];

/* Forward declarations */
static test_result_t test_olc_editor_table_count(test_case_t *test);
static test_result_t test_olc_editor_table_integrity(test_case_t *test); 
static test_result_t test_olc_editor_name_table_sync(test_case_t *test);
static test_result_t test_olc_editor_lookup_known(test_case_t *test);
static test_result_t test_olc_editor_registry_basic(test_case_t *test);
static test_result_t test_olc_state_management_basic(test_case_t *test);

/**
 * Main dispatch function for OLC framework tests
 */
test_result_t run_olc_framework_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "olcfw_editor_table_count") == 0) {
        return test_olc_editor_table_count(test);
    }
    else if (strcmp(test->test_type, "olcfw_editor_table_integrity") == 0) {
        return test_olc_editor_table_integrity(test);
    }
    else if (strcmp(test->test_type, "olcfw_editor_name_table_sync") == 0) {
        return test_olc_editor_name_table_sync(test);
    }
    else if (strcmp(test->test_type, "olcfw_editor_lookup_known") == 0) {
        return test_olc_editor_lookup_known(test);
    }
    else if (strcmp(test->test_type, "olcfw_editor_registry_basic") == 0) {
        return test_olc_editor_registry_basic(test);
    }
    else if (strcmp(test->test_type, "olcfw_state_management_basic") == 0) {
        return test_olc_state_management_basic(test);
    }

    return TEST_SKIP;
}

/**
 * Test that editor table has minimum expected entries
 */
static test_result_t test_olc_editor_table_count(test_case_t *test)
{
    int min_count = 10; // Default minimum
    
    // Check for min_editor_count in test input
    if (test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            json_t *min_param = json_object_get(input, "min_editor_count");
            if (min_param && json_is_integer(min_param)) {
                min_count = json_integer_value(min_param);
            }
        }
    }

    TEST_ASSERT_NOT_NULL(editor_table);

    int count = 0;
    while (editor_table[count].name != NULL) {
        count++;
    }

    if (count < min_count) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Editor table should have at least %d entries, found %d", 
                     min_count, count);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test all editor table entries have valid names and function pointers
 */
static test_result_t test_olc_editor_table_integrity(test_case_t *test)
{
    TEST_ASSERT_NOT_NULL(editor_table);

    int null_names = 0;
    int empty_names = 0;
    int null_functions = 0;
    int count = 0;
    
    for (int i = 0; editor_table[i].name != NULL; i++) {
        const struct editor_cmd_type *entry = &editor_table[i];
        
        if (!entry->name) {
            null_names++;
        } else if (strlen(entry->name) == 0) {
            empty_names++;
        }
        
        if (!entry->do_fun) {
            null_functions++;
        }
        
        count++;
    }

    if (null_names > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Found %d entries with NULL names", null_names);
        return TEST_FAILURE;
    }
    
    if (empty_names > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Found %d entries with empty names", empty_names);
        return TEST_FAILURE;
    }
    
    if (null_functions > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Found %d entries with NULL function pointers", null_functions);
        return TEST_FAILURE;
    }

    if (count == 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No valid editor entries found");
        return TEST_FAILURE;
    }
    
    return TEST_SUCCESS;
}

/**
 * Test editor name table is synchronized with editor constants
 */
static test_result_t test_olc_editor_name_table_sync(test_case_t *test)
{
    TEST_ASSERT_NOT_NULL(editor_name_table);
    TEST_ASSERT_NOT_NULL(editor_max_tabs_table);

    // Test some known editor constants are in valid range
    if (ED_AREA <= 0 || ED_AREA >= 50) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "ED_AREA (%d) not in valid range", ED_AREA);
        return TEST_FAILURE;
    }
    
    if (ED_ROOM <= 0 || ED_ROOM >= 50) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "ED_ROOM (%d) not in valid range", ED_ROOM);
        return TEST_FAILURE;
    }
    
    // Test that we have names for basic editors (can be NULL, but array should exist)
    if (ED_AREA > 0 && ED_AREA < 50) {
        TEST_ASSERT_NOT_NULL(editor_name_table[ED_AREA]);
    }
    
    if (ED_ROOM > 0 && ED_ROOM < 50) {
        TEST_ASSERT_NOT_NULL(editor_name_table[ED_ROOM]);
    }

    return TEST_SUCCESS;
}

/**
 * Test lookup of known editor commands
 */
static test_result_t test_olc_editor_lookup_known(test_case_t *test)
{
    TEST_ASSERT_NOT_NULL(editor_table);

    const char *default_known[] = {"area", "room", "object", "mobile", "help"};
    json_t *known_editors_param = NULL;
    int known_count = 5;

    // Check for known_editors array in test input  
    if (test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            known_editors_param = json_object_get(input, "known_editors");
            if (known_editors_param && json_is_array(known_editors_param)) {
                known_count = json_array_size(known_editors_param);
            }
        }
    }

    for (int i = 0; i < known_count; i++) {
        const char *editor_name;
        
        if (known_editors_param && json_is_array(known_editors_param)) {
            json_t *editor_json = json_array_get(known_editors_param, i);
            if (!editor_json || !json_is_string(editor_json)) {
                continue;
            }
            editor_name = json_string_value(editor_json);
        } else {
            editor_name = default_known[i];
        }

        // Search for this editor in the table
        bool found = false;
        for (int j = 0; editor_table[j].name != NULL; j++) {
            if (strcmp(editor_table[j].name, editor_name) == 0) {
                found = true;
                TEST_ASSERT_NOT_NULL(editor_table[j].do_fun);
                break;
            }
        }
        
        if (!found) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                         "Editor '%s' not found in editor_table", editor_name);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test basic editor registry functionality
 */
static test_result_t test_olc_editor_registry_basic(test_case_t *test)
{
    // Test the basic editor constants and table alignment
    TEST_ASSERT_INT_EQ(ED_AREA, 1);
    TEST_ASSERT_INT_EQ(ED_ROOM, 2);  
    TEST_ASSERT_INT_EQ(ED_OBJECT, 3);
    TEST_ASSERT_INT_EQ(ED_MOBILE, 4);

    return TEST_SUCCESS;
}

/**
 * Test OLC editor state management functions  
 */
static test_result_t test_olc_state_management_basic(test_case_t *test)
{
    // Test that olc_ed_name function exists and works for valid editor IDs
    CHAR_DATA test_ch = {0};
    DESCRIPTOR_DATA test_desc = {0};
    test_ch.desc = &test_desc;
    
    // Test with valid editor types
    test_desc.editor = ED_AREA;
    char *name = olc_ed_name(&test_ch);
    TEST_ASSERT_NOT_NULL(name);
    
    test_desc.editor = ED_ROOM; 
    name = olc_ed_name(&test_ch);
    TEST_ASSERT_NOT_NULL(name);
    
    // Test with invalid editor type
    test_desc.editor = 999;
    name = olc_ed_name(&test_ch);
    TEST_ASSERT_NOT_NULL(name);
    
    // Test olc_ed_tabs function
    test_desc.editor = ED_AREA;
    int tabs = olc_ed_tabs(&test_ch);
    TEST_ASSERT_INT_GTE(tabs, 0);

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */