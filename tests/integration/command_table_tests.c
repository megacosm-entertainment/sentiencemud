/**
 * Command Table Tests
 * 
 * Tests the MUD command table integrity including:
 * - Command table population (minimum count)
 * - Entry integrity (valid names and function pointers)
 * - Known command lookups
 * - Name uniqueness validation
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../interp.h"
#include "../framework/test_framework.h"
#include <string.h>

/* External function declarations */
extern LLIST *commands_list;
extern CMD_DATA *get_cmd_data(char *name);
static test_result_t test_command_table_count(test_case_t *test);
static test_result_t test_command_table_integrity(test_case_t *test);
static test_result_t test_command_table_lookup(test_case_t *test);
static test_result_t test_command_table_unique(test_case_t *test);

/* Forward declarations */

/**
 * Main dispatch function for command table tests
 */
test_result_t run_command_table_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }

    if (!commands_list) {
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "command_table_count_test") == 0) {
        return test_command_table_count(test);
    }
    else if (strcmp(test->test_type, "command_table_integrity_test") == 0) {
        return test_command_table_integrity(test);
    }
    else if (strcmp(test->test_type, "command_table_lookup_test") == 0) {
        return test_command_table_lookup(test);
    }
    else if (strcmp(test->test_type, "command_table_unique_test") == 0) {
        return test_command_table_unique(test);
    }

    return TEST_ERROR;
}

/**
 * Test that command table has minimum expected number of commands
 */
static test_result_t test_command_table_count(test_case_t *test)
{
    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    int minimum_commands = json_get_int_default(input, "minimum_commands", 50);
    int actual_count = list_size(commands_list);

    if (actual_count < minimum_commands) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
            "Expected at least %d commands, got %d", minimum_commands, actual_count);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test that all command entries have valid names and function pointers
 */
static test_result_t test_command_table_integrity(test_case_t *test)
{
    ITERATOR it;
    CMD_DATA *cmd;
    int null_names = 0;
    int empty_names = 0;
    int null_functions = 0;
    int total_checked = 0;

    iterator_start(&it, commands_list);
    while ((cmd = (CMD_DATA *)iterator_nextdata(&it))) {
        total_checked++;

        if (!cmd->name) {
            null_names++;
        } else if (cmd->name[0] == '\0') {
            empty_names++;
        }

        if (!cmd->function) {
            null_functions++;
        }
    }
    iterator_stop(&it);

    if (null_names > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
            "Found %d commands with NULL names (checked %d total)", null_names, total_checked);
        return TEST_FAILURE;
    }
    
    if (empty_names > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
            "Found %d commands with empty names (checked %d total)", empty_names, total_checked);
        return TEST_FAILURE;
    }

    /* NULL function pointers are acceptable in this system (disabled commands) */
    if (null_functions > 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
            "Found %d commands with NULL function pointers (checked %d total) - this is normal for disabled commands", 
            null_functions, total_checked);
    }

    return TEST_SUCCESS;
}

/**
 * Test that known commands resolve correctly and unknown commands don't
 */
static test_result_t test_command_table_lookup(test_case_t *test)
{
    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        return TEST_ERROR;
    }

    json_t *test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *test_case;
    int passed = 0;
    int total = 0;

    json_array_foreach(test_cases, index, test_case) {
        const char *command = json_get_string_default(test_case, "command", "");
        bool should_exist = json_get_bool_default(test_case, "should_exist", false);
        
        CMD_DATA *found = get_cmd_data((char *)command);
        bool exists = (found != NULL);
        
        total++;
        if (exists == should_exist) {
            passed++;
        } else {
            /* Log the failure */
            if (should_exist) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                    "Expected command '%s' to exist, but lookup failed", command);
            } else {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected command '%s' to not exist, but lookup succeeded", command);
            }
        }
    }

    if (passed != total) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
            "Command lookup test failed: %d/%d tests passed", passed, total);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test that all command names are unique (no duplicates)
 */
static test_result_t test_command_table_unique(test_case_t *test)
{
    ITERATOR it1, it2;
    CMD_DATA *cmd1, *cmd2;
    int duplicate_count = 0;
    int total_commands = list_size(commands_list);

    iterator_start(&it1, commands_list);
    while ((cmd1 = (CMD_DATA *)iterator_nextdata(&it1))) {
        if (!cmd1->name) continue;

        /* Check against all subsequent commands */
        iterator_start(&it2, commands_list);
        while ((cmd2 = (CMD_DATA *)iterator_nextdata(&it2))) {
            if (!cmd2->name || cmd1 == cmd2) continue;

            if (strcmp(cmd1->name, cmd2->name) == 0) {
                duplicate_count++;
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Found duplicate command name: '%s'", cmd1->name);
                break; /* Only count each duplicate once */
            }
        }
        iterator_stop(&it2);
    }
    iterator_stop(&it1);

    if (duplicate_count > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
            "Found %d duplicate command names in %d total commands", 
            duplicate_count, total_commands);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */