#ifdef BUILD_TESTS

#include <string.h>
#include <stdio.h>
#include "../framework/test_framework.h"
#include "../../utils/utf8.h"

static test_result_t test_utf8_byte_counts(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 byte count test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 byte count test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 byte count test missing scenario");
        return TEST_ERROR;
    }

    json_t *test_chars = json_object_get(input, "test_chars");
    if (!test_chars || !json_is_array(test_chars)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 byte count test missing test_chars array");
        return TEST_ERROR;
    }

    size_t array_size = json_array_size(test_chars);
    
    if (strcmp(scenario, "ascii_chars") == 0) {
        // All ASCII chars should return 1 byte
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            size_t bytes = utf8_bytes(ch);
            TEST_ASSERT_INT_EQ(bytes, 1);
        }
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "2byte_chars") == 0) {
        // All 2-byte chars should return 2 bytes
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            size_t bytes = utf8_bytes(ch);
            TEST_ASSERT_INT_EQ(bytes, 2);
        }
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "3byte_chars") == 0) {
        // All 3-byte chars should return 3 bytes
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            size_t bytes = utf8_bytes(ch);
            TEST_ASSERT_INT_EQ(bytes, 3);
        }
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "4byte_chars") == 0) {
        // All 4-byte chars should return 4 bytes
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            size_t bytes = utf8_bytes(ch);
            TEST_ASSERT_INT_EQ(bytes, 4);
        }
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "invalid_chars") == 0) {
        // Invalid chars should return bytes for replacement character (3 bytes for U+FFFD)
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            size_t bytes = utf8_bytes(ch);
            TEST_ASSERT_INT_EQ(bytes, 3); // Replacement character is 3 bytes
        }
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown byte count scenario: %s", scenario);
    return TEST_ERROR;
}

static test_result_t test_utf8_encode_decode(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 encode/decode test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 encode/decode test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 encode/decode test missing scenario");
        return TEST_ERROR;
    }

    json_t *test_chars = json_object_get(input, "test_chars");
    if (!test_chars || !json_is_array(test_chars)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 encode/decode test missing test_chars array");
        return TEST_ERROR;
    }

    size_t array_size = json_array_size(test_chars);
    
    // Test encoding with utf8_put and decoding with utf8_getchar
    for (size_t i = 0; i < array_size; i++) {
        json_t *char_val = json_array_get(test_chars, i);
        unichar_t original_ch = (unichar_t)json_integer_value(char_val);
        
        // Encode the character
        char buffer[8] = {0};
        char *end_ptr = utf8_put(buffer, original_ch);
        TEST_ASSERT_NOT_NULL(end_ptr);
        
        // Decode the character
        unichar_t decoded_ch = utf8_getchar(buffer);
        
        // They should match (or both be replacement char for invalid input)
        if (original_ch < 0 || original_ch > 0x10FFFF || 
            (original_ch >= 0xD800 && original_ch <= 0xDFFF)) {
            // Invalid input should become replacement character
            TEST_ASSERT_INT_EQ(decoded_ch, 0xFFFD);
        } else {
            TEST_ASSERT_INT_EQ(decoded_ch, original_ch);
        }
        
        // Also test utf8_getbytes
        char *encoded_bytes = utf8_getbytes(original_ch);
        TEST_ASSERT_NOT_NULL(encoded_bytes);
        
        // Decode from utf8_getbytes result
        unichar_t decoded_ch2 = utf8_getchar(encoded_bytes);
        if (original_ch < 0 || original_ch > 0x10FFFF || 
            (original_ch >= 0xD800 && original_ch <= 0xDFFF)) {
            TEST_ASSERT_INT_EQ(decoded_ch2, 0xFFFD);
        } else {
            TEST_ASSERT_INT_EQ(decoded_ch2, original_ch);
        }
    }

    return TEST_SUCCESS;
}

static test_result_t test_utf8_strlen(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 strlen test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 strlen test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 strlen test missing scenario");
        return TEST_ERROR;
    }

    const char *test_string = test_json_get_string(input, "test_string");
    if (!test_string) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 strlen test missing test_string");
        return TEST_ERROR;
    }

    int expected_length = test_json_get_int(input, "expected_length");
    
    size_t actual_length = utf8_strlen(test_string);
    TEST_ASSERT_INT_EQ(actual_length, expected_length);

    return TEST_SUCCESS;
}

static test_result_t test_utf8_validation(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 validation test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 validation test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 validation test missing scenario");
        return TEST_ERROR;
    }

    if (strcmp(scenario, "invalid_sequences") == 0) {
        // Test known invalid UTF-8 sequences
        char invalid1[] = {(char)0xFF, 0}; // Invalid start byte
        char invalid2[] = {(char)0xC0, (char)0x80, 0}; // Overlong encoding
        char invalid3[] = {(char)0xE0, (char)0x80, 0}; // Truncated sequence
        
        TEST_ASSERT_FALSE(is_utf8_string(invalid1));
        TEST_ASSERT_FALSE(is_utf8_string(invalid2));
        TEST_ASSERT_FALSE(is_utf8_string(invalid3));
        
        return TEST_SUCCESS;
    }

    const char *test_string = test_json_get_string(input, "test_string");
    if (!test_string) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 validation test missing test_string");
        return TEST_ERROR;
    }

    bool expected_valid = test_json_get_bool(input, "expected_valid");
    bool actual_valid = is_utf8_string(test_string);
    
    TEST_ASSERT_INT_EQ(actual_valid, expected_valid);

    return TEST_SUCCESS;
}

static test_result_t test_utf8_navigation(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 navigation test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 navigation test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 navigation test missing scenario");
        return TEST_ERROR;
    }

    const char *test_string = test_json_get_string(input, "test_string");
    if (!test_string) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 navigation test missing test_string");
        return TEST_ERROR;
    }

    if (strcmp(scenario, "forward_navigation") == 0) {
        // Test utf8_nextchar() navigation
        const char *ptr = test_string;
        int char_count = 0;
        
        while (*ptr) {
            char_count++;
            ptr = utf8_nextchar(ptr);
            TEST_ASSERT_NOT_NULL(ptr);
        }
        
        // Should match utf8_strlen result
        size_t expected_count = utf8_strlen(test_string);
        TEST_ASSERT_INT_EQ(char_count, expected_count);
        
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "backward_navigation") == 0) {
        // Test utf8_prevchar() navigation
        const char *start = test_string;
        const char *end = start + strlen(test_string);
        const char *ptr = end;
        int char_count = 0;
        
        while (ptr > start) {
            ptr = utf8_prevchar(ptr);
            TEST_ASSERT_NOT_NULL(ptr);
            TEST_ASSERT_TRUE(ptr >= start);
            char_count++;
        }
        
        // Should match utf8_strlen result
        size_t expected_count = utf8_strlen(test_string);
        TEST_ASSERT_INT_EQ(char_count, expected_count);
        
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown navigation scenario: %s", scenario);
    return TEST_ERROR;
}

static test_result_t test_utf8_case_conversion(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 case conversion test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 case conversion test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 case conversion test missing scenario");
        return TEST_ERROR;
    }

    json_t *test_chars = json_object_get(input, "test_chars");
    if (!test_chars || !json_is_array(test_chars)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 case conversion test missing test_chars array");
        return TEST_ERROR;
    }

    size_t array_size = json_array_size(test_chars);
    
    if (strcmp(scenario, "ascii_case") == 0) {
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            
            unichar_t upper = utf8_toupper(ch);
            unichar_t lower = utf8_tolower(ch);
            
            // For ASCII, we can test specific expected values
            if (ch >= 'a' && ch <= 'z') {
                TEST_ASSERT_INT_EQ(upper, ch - 'a' + 'A');
                TEST_ASSERT_INT_EQ(lower, ch);
            } else if (ch >= 'A' && ch <= 'Z') {
                TEST_ASSERT_INT_EQ(upper, ch);
                TEST_ASSERT_INT_EQ(lower, ch - 'A' + 'a');
            } else {
                // Non-letters should remain unchanged
                TEST_ASSERT_INT_EQ(upper, ch);
                TEST_ASSERT_INT_EQ(lower, ch);
            }
        }
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "accented_case") == 0) {
        // For accented characters, just verify the functions don't crash
        // and return valid values (specific case conversion depends on implementation)
        for (size_t i = 0; i < array_size; i++) {
            json_t *char_val = json_array_get(test_chars, i);
            unichar_t ch = (unichar_t)json_integer_value(char_val);
            
            unichar_t upper = utf8_toupper(ch);
            unichar_t lower = utf8_tolower(ch);
            
            // Functions should return valid Unicode codepoints
            TEST_ASSERT_TRUE(upper >= 0 && upper <= 0x10FFFF);
            TEST_ASSERT_TRUE(lower >= 0 && lower <= 0x10FFFF);
        }
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown case conversion scenario: %s", scenario);
    return TEST_ERROR;
}

static test_result_t test_utf8_string_compare(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 string compare test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 string compare test missing input");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 string compare test missing scenario");
        return TEST_ERROR;
    }

    const char *string_a = test_json_get_string(input, "string_a");
    const char *string_b = test_json_get_string(input, "string_b");
    
    if (!string_a || !string_b) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 string compare test missing string_a or string_b");
        return TEST_ERROR;
    }

    if (strcmp(scenario, "compare_equal") == 0) {
        int result = utf8_str_cmp(string_a, string_b);
        TEST_ASSERT_INT_EQ(result, 0);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "compare_ordering") == 0) {
        int result = utf8_str_cmp(string_a, string_b);
        TEST_ASSERT_TRUE(result < 0); // "apple" < "banana"
        
        int result_reverse = utf8_str_cmp(string_b, string_a);
        TEST_ASSERT_TRUE(result_reverse > 0); // "banana" > "apple"
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "test_prefix") == 0) {
        bool result = utf8_str_prefix(string_a, string_b);
        TEST_ASSERT_TRUE(result);
        
        bool result_reverse = utf8_str_prefix(string_b, string_a);
        TEST_ASSERT_FALSE(result_reverse);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "test_infix") == 0) {
        bool result = utf8_str_infix(string_a, string_b);
        TEST_ASSERT_TRUE(result);
        
        bool result_reverse = utf8_str_infix(string_b, string_a);
        TEST_ASSERT_FALSE(result_reverse);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "test_suffix") == 0) {
        bool result = utf8_str_suffix(string_a, string_b);
        TEST_ASSERT_TRUE(result);
        
        bool result_reverse = utf8_str_suffix(string_b, string_a);
        TEST_ASSERT_FALSE(result_reverse);
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown string compare scenario: %s", scenario);
    return TEST_ERROR;
}

static test_result_t test_utf8_skip_len(test_case_t *test)
{
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 skip_len test missing test case or config");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 skip_len test missing input");
        return TEST_ERROR;
    }

    const char *test_string = test_json_get_string(input, "test_string");
    if (!test_string) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 skip_len test missing test_string");
        return TEST_ERROR;
    }

    int expected_chars = test_json_get_int(input, "expected_chars");
    int expected_bytes = test_json_get_int(input, "expected_bytes");
    
    size_t actual_chars = 0;
    size_t actual_bytes = 0;
    
    bool result = utf8_skip_len(test_string, &actual_chars, &actual_bytes);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_INT_EQ(actual_chars, expected_chars);
    TEST_ASSERT_INT_EQ(actual_bytes, expected_bytes);

    return TEST_SUCCESS;
}

test_result_t run_utf8_test_case(test_case_t *test)
{
    if (!test) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 test missing test case");
        return TEST_ERROR;
    }

    if (!test->test_type) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "UTF-8 test missing test_type");
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "utf8_byte_counts") == 0) {
        return test_utf8_byte_counts(test);
    }
    if (strcmp(test->test_type, "utf8_encode_decode") == 0) {
        return test_utf8_encode_decode(test);
    }
    if (strcmp(test->test_type, "utf8_strlen") == 0) {
        return test_utf8_strlen(test);
    }
    if (strcmp(test->test_type, "utf8_validation") == 0) {
        return test_utf8_validation(test);
    }
    if (strcmp(test->test_type, "utf8_navigation") == 0) {
        return test_utf8_navigation(test);
    }
    if (strcmp(test->test_type, "utf8_case_conversion") == 0) {
        return test_utf8_case_conversion(test);
    }
    if (strcmp(test->test_type, "utf8_string_compare") == 0) {
        return test_utf8_string_compare(test);
    }
    if (strcmp(test->test_type, "utf8_skip_len") == 0) {
        return test_utf8_skip_len(test);
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown UTF-8 test type: %s", test->test_type);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */