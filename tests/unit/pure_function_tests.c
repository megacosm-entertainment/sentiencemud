#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../log.h"

bool string_argremove_index(char *src, int argindex, char *buf);
bool string_argremove_phrase(char *src, char *phrase, char *buf);
char *string_linedel(char *string, int line);
char *string_lineadd(char *string, char *newstr, int line);
char *olc_getline(char *str, char *buf);
char *numlineas(char *string);

static bool ends_with_crlf(const char *str) {
    size_t len;

    if (!str) {
        return false;
    }

    len = strlen(str);
    return len >= 2 && str[len - 2] == '\n' && str[len - 1] == '\r';
}

test_result_t run_pure_function_test_case(test_case_t *test) {
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing configuration");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *expected_output = json_object_get(test->config, "expected_output");

    if (!input || !expected_output) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing input or expected_output");
        return TEST_ERROR;
    }

    json_t *function_name = json_object_get(input, "function");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!function_name || !test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test malformed: missing function or test_cases array");
        return TEST_ERROR;
    }

    const char *func_name = json_string_value(function_name);
    if (test->verbose_output) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Testing pure function: %s", func_name);
    }

    if (strcmp(func_name, "parse_widevnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_area = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            const char *context_area_name = test_json_get_string(test_case, "context_area");
            const char *input_area_uid_from = test_json_get_string(test_case, "input_area_uid_from");
            long input_vnum = test_json_get_int(test_case, "input_vnum");
            char dynamic_input[MAX_INPUT_LENGTH];

            dynamic_input[0] = '\0';

            if ((input_str == NULL || input_str[0] == '\0')
                && input_area_uid_from != NULL
                && input_vnum > 0)
            {
                AREA_DATA *uid_area = find_area((char *)input_area_uid_from);
                if (!uid_area) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "parse_widevnum dynamic input failed: area '%s' not found",
                                 input_area_uid_from);
                    return TEST_ERROR;
                }

                snprintf(dynamic_input, sizeof(dynamic_input), "%ld#%ld", uid_area->uid, input_vnum);
                input_str = dynamic_input;
            }

            if (!input_str) continue;

            AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : NULL;
            WNUM result;

            bool parse_success = parse_widevnum((char*)input_str, context_area, &result);

            if (!parse_success && expected_vnum > 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') failed when success expected",
                             input_str);
                return TEST_FAILURE;
            }

            if (parse_success && expected_vnum == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') succeeded when failure expected",
                             input_str);
                return TEST_FAILURE;
            }

            if (parse_success && result.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') returned vnum %ld, expected %ld",
                             input_str, result.vnum, expected_vnum);
                return TEST_FAILURE;
            }

            if (parse_success && expected_area && result.pArea) {
                if (strcmp(result.pArea->name, expected_area) != 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "parse_widevnum('%s') returned area '%s', expected '%s'",
                                 input_str, result.pArea->name, expected_area);
                    return TEST_FAILURE;
                }
            }

            if (test->verbose_output) {
                if (parse_success) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                 "✓ parse_widevnum('%s') -> area:'%s', vnum:%ld",
                                 input_str,
                                 result.pArea ? result.pArea->name : "NULL",
                                 result.vnum);
                } else {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                 "✓ parse_widevnum('%s') -> failed as expected",
                                 input_str);
                }
            }
        }

        if (test->verbose_output) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Pure function test passed for %s", func_name);
        }
        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_number") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *arg = test_json_get_string(test_case, "arg");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "is_number test case missing 'arg'");
                return TEST_ERROR;
            }

            bool actual = is_number(arg);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_number('%s') returned %s, expected %s",
                             arg,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_prefix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *astr = test_json_get_string(test_case, "astr");
            const char *bstr = test_json_get_string(test_case, "bstr");
            bool expected_not_prefix = test_json_get_bool(test_case, "expected_not_prefix");

            if (!astr || !bstr) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "str_prefix test case missing astr/bstr");
                return TEST_ERROR;
            }

            bool actual_not_prefix = str_prefix(astr, bstr);
            if (actual_not_prefix != expected_not_prefix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_prefix('%s','%s') returned %s, expected %s",
                             astr,
                             bstr,
                             actual_not_prefix ? "true" : "false",
                             expected_not_prefix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "number_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            int expected_number = test_json_get_int(test_case, "expected_number");
            const char *expected_arg = test_json_get_string(test_case, "expected_arg");
            char argument_buf[MAX_INPUT_LENGTH];
            char arg_out[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "number_argument test case missing argument/expected_arg");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            int actual_number = number_argument(argument_buf, arg_out);

            if (actual_number != expected_number || str_cmp(arg_out, expected_arg) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "number_argument('%s') returned (%d,'%s'), expected (%d,'%s')",
                             argument_in,
                             actual_number,
                             arg_out,
                             expected_number,
                             expected_arg);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "one_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            const char *expected_first = test_json_get_string(test_case, "expected_first");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            char argument_buf[MAX_STRING_LENGTH];
            char first[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_first || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "one_argument test case missing argument/expected_first/expected_rest");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            char *rest = one_argument(argument_buf, first);

            if (str_cmp(first, expected_first) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "one_argument('%s') returned first='%s', rest='%s'; expected first='%s', rest='%s'",
                             argument_in,
                             first,
                             rest,
                             expected_first,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "smash_tilde") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char buffer[MAX_STRING_LENGTH];

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "smash_tilde test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(buffer, sizeof(buffer), "%s", input_str);
            smash_tilde(buffer);

            if (str_cmp(buffer, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "smash_tilde('%s') returned '%s', expected '%s'",
                             input_str,
                             buffer,
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_exact_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_exact_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_exact_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_exact_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_cmp") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *left = test_json_get_string(test_case, "left");
            const char *right = test_json_get_string(test_case, "right");
            bool expected_not_equal = test_json_get_bool(test_case, "expected_not_equal");

            if (!left || !right) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_cmp test case missing left/right");
                return TEST_ERROR;
            }

            bool actual_not_equal = str_cmp(left, right);
            if (actual_not_equal != expected_not_equal) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_cmp('%s','%s') returned %s, expected %s",
                             left,
                             right,
                             actual_not_equal ? "true" : "false",
                             expected_not_equal ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_infix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "needle");
            const char *haystack = test_json_get_string(test_case, "haystack");
            bool expected_not_infix = test_json_get_bool(test_case, "expected_not_infix");

            if (!needle || !haystack) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_infix test case missing needle/haystack");
                return TEST_ERROR;
            }

            bool actual_not_infix = str_infix(needle, haystack);
            if (actual_not_infix != expected_not_infix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_infix('%s','%s') returned %s, expected %s",
                             needle,
                             haystack,
                             actual_not_infix ? "true" : "false",
                             expected_not_infix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_suffix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *astr = test_json_get_string(test_case, "astr");
            const char *bstr = test_json_get_string(test_case, "bstr");
            bool expected_not_suffix = test_json_get_bool(test_case, "expected_not_suffix");

            if (!astr || !bstr) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_suffix test case missing astr/bstr");
                return TEST_ERROR;
            }

            bool actual_not_suffix = str_suffix(astr, bstr);
            if (actual_not_suffix != expected_not_suffix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_suffix('%s','%s') returned %s, expected %s",
                             astr,
                             bstr,
                             actual_not_suffix ? "true" : "false",
                             expected_not_suffix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "first_arg") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            const char *expected_first = test_json_get_string(test_case, "expected_first");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            bool fcase = test_json_get_bool(test_case, "fcase");
            char argument_buf[MAX_STRING_LENGTH];
            char first[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_first || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "first_arg test case missing argument/expected_first/expected_rest");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            char *rest = first_arg(argument_buf, first, fcase);

            if (str_cmp(first, expected_first) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "first_arg('%s') returned first='%s', rest='%s'; expected first='%s', rest='%s'",
                             argument_in,
                             first,
                             rest,
                             expected_first,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_argremove_index") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *src = test_json_get_string(test_case, "src");
            int argindex = test_json_get_int(test_case, "argindex");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            bool expected_result = test_json_get_bool(test_case, "expected_result");
            char output[MAX_STRING_LENGTH];
            char src_buf[MAX_STRING_LENGTH];

            if (!src || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_argremove_index test case missing src/expected_output");
                return TEST_ERROR;
            }

            snprintf(src_buf, sizeof(src_buf), "%s", src);
            bool actual_result = string_argremove_index(src_buf, argindex, output);

            if (actual_result != expected_result || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_argremove_index('%s', %d) returned (result=%s, output='%s'), expected (result=%s, output='%s')",
                             src,
                             argindex,
                             actual_result ? "true" : "false",
                             output,
                             expected_result ? "true" : "false",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_argremove_phrase") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *src = test_json_get_string(test_case, "src");
            const char *phrase = test_json_get_string(test_case, "phrase");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            bool expected_result = test_json_get_bool(test_case, "expected_result");
            char output[MAX_STRING_LENGTH];
            char src_buf[MAX_STRING_LENGTH];
            char phrase_buf[MAX_INPUT_LENGTH];

            if (!src || !phrase || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_argremove_phrase test case missing src/phrase/expected_output");
                return TEST_ERROR;
            }

            snprintf(src_buf, sizeof(src_buf), "%s", src);
            snprintf(phrase_buf, sizeof(phrase_buf), "%s", phrase);
            bool actual_result = string_argremove_phrase(src_buf, phrase_buf, output);

            if (actual_result != expected_result || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_argremove_phrase('%s', '%s') returned (result=%s, output='%s'), expected (result=%s, output='%s')",
                             src,
                             phrase,
                             actual_result ? "true" : "false",
                             output,
                             expected_result ? "true" : "false",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "format_string") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_contains = test_json_get_string(test_case, "expected_contains");
            bool expect_trailing_crlf = test_json_get_bool(test_case, "expect_trailing_crlf");
            char *input_dup;
            char *output;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_string test case missing input");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = format_string(input_dup);
            if (!output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_string returned NULL output");
                return TEST_FAILURE;
            }

            if (expected_contains && !strstr(output, expected_contains)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_string output '%s' does not contain '%s'",
                             output,
                             expected_contains);
                free_string(output);
                return TEST_FAILURE;
            }

            if (expect_trailing_crlf && !ends_with_crlf(output)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_string output does not end with \\n\\r: '%s'",
                             output);
                free_string(output);
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "format_paragraph") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_contains = test_json_get_string(test_case, "expected_contains");
            bool expect_trailing_crlf = test_json_get_bool(test_case, "expect_trailing_crlf");
            char *input_dup;
            char *output;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_paragraph test case missing input");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = format_paragraph(input_dup);
            if (!output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_paragraph returned NULL output");
                return TEST_FAILURE;
            }

            if (expected_contains && !strstr(output, expected_contains)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_paragraph output '%s' does not contain '%s'",
                             output,
                             expected_contains);
                free_string(output);
                return TEST_FAILURE;
            }

            if (expect_trailing_crlf && !ends_with_crlf(output)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_paragraph output does not end with \\n\\r: '%s'",
                             output);
                free_string(output);
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_linedel") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int line = test_json_get_int(test_case, "line");
            char *input_dup;
            char *output;

            if (!input_str || !expected_output || line <= 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_linedel test case missing input/expected_output/line");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = string_linedel(input_dup, line);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_linedel(line=%d) returned '%s', expected '%s'",
                             line,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_lineadd") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *newstr = test_json_get_string(test_case, "newstr");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int line = test_json_get_int(test_case, "line");
            char *input_dup;
            char newstr_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !newstr || !expected_output || line <= 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_lineadd test case missing input/newstr/expected_output/line");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            snprintf(newstr_buf, sizeof(newstr_buf), "%s", newstr);
            output = string_lineadd(input_dup, newstr_buf, line);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_lineadd(line=%d) returned '%s', expected '%s'",
                             line,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "olc_getline") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_line = test_json_get_string(test_case, "expected_line");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            char input_buf[MAX_STRING_LENGTH];
            char line_buf[MAX_STRING_LENGTH];
            char *rest;

            if (!input_str || !expected_line || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "olc_getline test case missing input/expected_line/expected_rest");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            rest = olc_getline(input_buf, line_buf);

            if (str_cmp(line_buf, expected_line) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "olc_getline('%s') returned line='%s' rest='%s', expected line='%s' rest='%s'",
                             input_str,
                             line_buf,
                             rest,
                             expected_line,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "numlineas") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char input_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "numlineas test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            output = numlineas(input_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "numlineas('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_replace") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *orig = test_json_get_string(test_case, "orig");
            const char *old = test_json_get_string(test_case, "old");
            const char *new_value = test_json_get_string(test_case, "new");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char *orig_dup;
            char *output;

            if (!orig || !old || !new_value || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_replace test case missing orig/old/new/expected_output");
                return TEST_ERROR;
            }

            orig_dup = str_dup(orig);
            output = string_replace(orig_dup, (char *)old, (char *)new_value);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_replace('%s','%s','%s') returned '%s', expected '%s'",
                             orig,
                             old,
                             new_value,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_replace_static") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *orig = test_json_get_string(test_case, "orig");
            const char *old = test_json_get_string(test_case, "old");
            const char *new_value = test_json_get_string(test_case, "new");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char orig_buf[MAX_STRING_LENGTH];
            char old_buf[MAX_INPUT_LENGTH];
            char new_buf[MAX_INPUT_LENGTH];
            char *output;

            if (!orig || !old || !new_value || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_replace_static test case missing orig/old/new/expected_output");
                return TEST_ERROR;
            }

            snprintf(orig_buf, sizeof(orig_buf), "%s", orig);
            snprintf(old_buf, sizeof(old_buf), "%s", old);
            snprintf(new_buf, sizeof(new_buf), "%s", new_value);
            output = string_replace_static(orig_buf, old_buf, new_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_replace_static('%s','%s','%s') returned '%s', expected '%s'",
                             orig,
                             old,
                             new_value,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_unpad") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char *input_dup;
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_unpad test case missing input/expected_output");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = string_unpad(input_dup);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_unpad('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_proper") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char input_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_proper test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            output = string_proper(input_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_proper('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_indent") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int indent = test_json_get_int(test_case, "indent");
            char *output;

            if (!input_str || !expected_output || indent < 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_indent test case missing input/expected_output/indent");
                return TEST_ERROR;
            }

            output = string_indent(input_str, indent);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_indent('%s', %d) returned '%s', expected '%s'",
                             input_str,
                             indent,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unsupported pure function: %s", func_name);
    return TEST_SKIP;
}

#endif // BUILD_TESTS
