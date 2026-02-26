#ifdef BUILD_TESTS

#include <string.h>
#include "../../merc.h"
#include "../framework/test_framework.h"
#include "../framework/test_utils.h"

static test_result_t test_string_editor_add_basic_append(test_case_t *test)
{
    CHAR_DATA *ch = NULL;
    DESCRIPTOR_DATA *desc = NULL;
    char *text = str_dup("");
    char argument_buf[MAX_INPUT_LENGTH];

    (void)test;

    if (!test_utils_create_fake_player(&ch, &desc)) {
        if (text) {
            free_string(text);
        }
        return TEST_ERROR;
    }

    desc->pString = &text;
    snprintf(argument_buf, sizeof(argument_buf), "%s", "hello world");
    string_add(ch, argument_buf);

    if (str_cmp(text, "hello world\n\r") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "string_edit/string_add produced '%s', expected 'hello world\\n\\r'",
                     text ? text : "(null)");
        free_string(text);
        test_utils_destroy_fake_player(ch, desc);
        return TEST_FAILURE;
    }

    free_string(text);
    test_utils_destroy_fake_player(ch, desc);
    return TEST_SUCCESS;
}

static test_result_t test_string_editor_add_multiline_append(test_case_t *test)
{
    CHAR_DATA *ch = NULL;
    DESCRIPTOR_DATA *desc = NULL;
    char *text = str_dup("alpha\n\r");
    char argument_buf[MAX_INPUT_LENGTH];

    (void)test;

    if (!test_utils_create_fake_player(&ch, &desc)) {
        if (text) {
            free_string(text);
        }
        return TEST_ERROR;
    }

    desc->pString = &text;
    snprintf(argument_buf, sizeof(argument_buf), "%s", "beta");
    string_add(ch, argument_buf);

    if (str_cmp(text, "alpha\n\rbeta\n\r") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "string_append/string_add produced '%s', expected 'alpha\\n\\rbeta\\n\\r'",
                     text ? text : "(null)");
        free_string(text);
        test_utils_destroy_fake_player(ch, desc);
        return TEST_FAILURE;
    }

    free_string(text);
    test_utils_destroy_fake_player(ch, desc);
    return TEST_SUCCESS;
}

static test_result_t test_string_editor_add_terminator(test_case_t *test)
{
    CHAR_DATA *ch = NULL;
    DESCRIPTOR_DATA *desc = NULL;
    char *text = str_dup("alpha\n\r");
    char argument_buf[MAX_INPUT_LENGTH];

    (void)test;

    if (!test_utils_create_fake_player(&ch, &desc)) {
        if (text) {
            free_string(text);
        }
        return TEST_ERROR;
    }

    desc->pString = &text;
    snprintf(argument_buf, sizeof(argument_buf), "%s", "@");
    string_add(ch, argument_buf);

    if (desc->pString != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "string_add('@') did not clear descriptor edit pointer");
        free_string(text);
        test_utils_destroy_fake_player(ch, desc);
        return TEST_FAILURE;
    }

    if (str_cmp(text, "alpha\n\r") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "string_add('@') unexpectedly modified text to '%s'",
                     text ? text : "(null)");
        free_string(text);
        test_utils_destroy_fake_player(ch, desc);
        return TEST_FAILURE;
    }

    free_string(text);
    test_utils_destroy_fake_player(ch, desc);
    return TEST_SUCCESS;
}

test_result_t run_string_editor_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "string_editor_add_basic_append_test") == 0) {
        return test_string_editor_add_basic_append(test);
    }

    if (strcmp(test->test_type, "string_editor_add_multiline_append_test") == 0) {
        return test_string_editor_add_multiline_append(test);
    }

    if (strcmp(test->test_type, "string_editor_add_terminator_test") == 0) {
        return test_string_editor_add_terminator(test);
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                 "Unknown string editor test type: %s", test->test_type);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */
