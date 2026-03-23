#ifdef BUILD_TESTS

#include "../framework/test_framework.h"
#include "../../utils/strdict.h"
#include <string.h>

test_result_t run_strdict_test_case(test_case_t *test)
{
    json_t *input = json_object_get(test->config, "input");
    const char *scenario = test_json_get_string(input, "scenario");
    
    if (strcmp(scenario, "basic_ops") == 0) {
        // Test basic dictionary operations - create, set, get
        size_t initial_cap = (size_t)test_json_get_int(input, "initial_cap");
        json_t *key_values = json_object_get(input, "key_values");
        
        STRING_DICT *dict = strdict_new(initial_cap);
        TEST_ASSERT_NOT_NULL(dict);
        
        // Initially should be empty
        TEST_ASSERT_INT_EQ(0, (int)strdict_count(dict));
        
        // Set key-value pairs and verify
        if (json_is_array(key_values)) {
            size_t pairs_count = json_array_size(key_values);
            for (size_t i = 0; i < pairs_count; i++) {
                json_t *pair = json_array_get(key_values, i);
                if (json_is_array(pair) && json_array_size(pair) == 2) {
                    const char *key = json_string_value(json_array_get(pair, 0));
                    const char *value = json_string_value(json_array_get(pair, 1));
                    
                    bool set_result = strdict_set(dict, key, value);
                    TEST_ASSERT_TRUE(set_result);
                    
                    // Verify we can get the value back
                    const char *retrieved = strdict_get(dict, key);
                    TEST_ASSERT_NOT_NULL(retrieved);
                    TEST_ASSERT_STR_EQ(value, retrieved);
                }
            }
            
            // Verify count matches number of pairs
            TEST_ASSERT_INT_EQ((int)pairs_count, (int)strdict_count(dict));
        }
        
        strdict_free(dict);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "overwrite") == 0) {
        // Test overwriting existing keys
        const char *test_key = test_json_get_string(input, "test_key");
        const char *first_value = test_json_get_string(input, "first_value");
        const char *second_value = test_json_get_string(input, "second_value");
        
        STRING_DICT *dict = strdict_new(8);
        TEST_ASSERT_NOT_NULL(dict);
        
        // Set initial value
        bool first_set = strdict_set(dict, test_key, first_value);
        TEST_ASSERT_TRUE(first_set);
        TEST_ASSERT_INT_EQ(1, (int)strdict_count(dict));
        
        // Verify initial value
        const char *retrieved_first = strdict_get(dict, test_key);
        TEST_ASSERT_NOT_NULL(retrieved_first);
        TEST_ASSERT_STR_EQ(first_value, retrieved_first);
        
        // Overwrite with second value
        bool second_set = strdict_set(dict, test_key, second_value);
        TEST_ASSERT_TRUE(second_set);
        TEST_ASSERT_INT_EQ(1, (int)strdict_count(dict)); // Count shouldn't change
        
        // Verify overwritten value
        const char *retrieved_second = strdict_get(dict, test_key);
        TEST_ASSERT_NOT_NULL(retrieved_second);
        TEST_ASSERT_STR_EQ(second_value, retrieved_second);
        TEST_ASSERT_STR_NEQ(first_value, retrieved_second);
        
        strdict_free(dict);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "delete_ops") == 0) {
        // Test deleting keys
        json_t *initial_pairs = json_object_get(input, "initial_pairs");
        const char *delete_key = test_json_get_string(input, "delete_key");
        
        STRING_DICT *dict = strdict_new(16);
        TEST_ASSERT_NOT_NULL(dict);
        
        size_t initial_count = 0;
        
        // Add initial pairs
        if (json_is_array(initial_pairs)) {
            initial_count = json_array_size(initial_pairs);
            for (size_t i = 0; i < initial_count; i++) {
                json_t *pair = json_array_get(initial_pairs, i);
                if (json_is_array(pair) && json_array_size(pair) == 2) {
                    const char *key = json_string_value(json_array_get(pair, 0));
                    const char *value = json_string_value(json_array_get(pair, 1));
                    
                    bool set_result = strdict_set(dict, key, value);
                    TEST_ASSERT_TRUE(set_result);
                }
            }
        }
        
        TEST_ASSERT_INT_EQ((int)initial_count, (int)strdict_count(dict));
        
        // Verify delete_key exists before deletion
        const char *before_delete = strdict_get(dict, delete_key);
        TEST_ASSERT_NOT_NULL(before_delete);
        
        // Delete the key
        bool delete_result = strdict_delete(dict, delete_key);
        TEST_ASSERT_TRUE(delete_result);
        
        // Verify key is gone
        const char *after_delete = strdict_get(dict, delete_key);
        TEST_ASSERT_NULL(after_delete);
        
        // Verify count decreased
        TEST_ASSERT_INT_EQ((int)(initial_count - 1), (int)strdict_count(dict));
        
        // Try deleting non-existent key
        bool delete_nonexistent = strdict_delete(dict, "nonexistent_key");
        TEST_ASSERT_FALSE(delete_nonexistent);
        
        strdict_free(dict);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "iteration") == 0) {
        // Test iteration over dictionary entries
        json_t *test_pairs = json_object_get(input, "test_pairs");
        
        STRING_DICT *dict = strdict_new(16);
        TEST_ASSERT_NOT_NULL(dict);
        
        size_t expected_count = 0;
        
        // Add test pairs
        if (json_is_array(test_pairs)) {
            expected_count = json_array_size(test_pairs);
            for (size_t i = 0; i < expected_count; i++) {
                json_t *pair = json_array_get(test_pairs, i);
                if (json_is_array(pair) && json_array_size(pair) == 2) {
                    const char *key = json_string_value(json_array_get(pair, 0));
                    const char *value = json_string_value(json_array_get(pair, 1));
                    
                    bool set_result = strdict_set(dict, key, value);
                    TEST_ASSERT_TRUE(set_result);
                }
            }
        }
        
        TEST_ASSERT_INT_EQ((int)expected_count, (int)strdict_count(dict));
        
        // Iterate and verify all pairs are found
        STRING_DICT_ITER iter = strdict_iter(dict);
        const char *key;
        const char *value;
        size_t found_count = 0;
        
        while (strdict_next(&iter, &key, &value)) {
            TEST_ASSERT_NOT_NULL(key);
            TEST_ASSERT_NOT_NULL(value);
            found_count++;
            
            // Verify this key-value pair exists in our test data
            bool found_in_test_data = false;
            if (json_is_array(test_pairs)) {
                for (size_t i = 0; i < json_array_size(test_pairs); i++) {
                    json_t *pair = json_array_get(test_pairs, i);
                    if (json_is_array(pair) && json_array_size(pair) == 2) {
                        const char *test_key = json_string_value(json_array_get(pair, 0));
                        const char *test_value = json_string_value(json_array_get(pair, 1));
                        
                        if (strcmp(key, test_key) == 0 && strcmp(value, test_value) == 0) {
                            found_in_test_data = true;
                            break;
                        }
                    }
                }
            }
            TEST_ASSERT_TRUE(found_in_test_data);
        }
        
        // Verify we found all expected pairs
        TEST_ASSERT_INT_EQ((int)expected_count, (int)found_count);
        
        strdict_free(dict);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "clear_ops") == 0) {
        // Test clearing dictionary
        json_t *setup_pairs = json_object_get(input, "setup_pairs");
        
        STRING_DICT *dict = strdict_new(32);
        TEST_ASSERT_NOT_NULL(dict);
        
        // Add setup pairs
        if (json_is_array(setup_pairs)) {
            size_t pairs_count = json_array_size(setup_pairs);
            for (size_t i = 0; i < pairs_count; i++) {
                json_t *pair = json_array_get(setup_pairs, i);
                if (json_is_array(pair) && json_array_size(pair) == 2) {
                    const char *key = json_string_value(json_array_get(pair, 0));
                    const char *value = json_string_value(json_array_get(pair, 1));
                    
                    bool set_result = strdict_set(dict, key, value);
                    TEST_ASSERT_TRUE(set_result);
                }
            }
            
            // Verify entries were added
            TEST_ASSERT_INT_EQ((int)pairs_count, (int)strdict_count(dict));
            TEST_ASSERT_INT_GT((int)strdict_count(dict), 0);
        }
        
        // Clear the dictionary
        strdict_clear(dict);
        
        // Verify dictionary is empty
        TEST_ASSERT_INT_EQ(0, (int)strdict_count(dict));
        
        // Verify keys are no longer accessible
        if (json_is_array(setup_pairs)) {
            for (size_t i = 0; i < json_array_size(setup_pairs); i++) {
                json_t *pair = json_array_get(setup_pairs, i);
                if (json_is_array(pair) && json_array_size(pair) == 2) {
                    const char *key = json_string_value(json_array_get(pair, 0));
                    const char *retrieved = strdict_get(dict, key);
                    TEST_ASSERT_NULL(retrieved);
                }
            }
        }
        
        // Verify iteration returns no entries
        STRING_DICT_ITER iter = strdict_iter(dict);
        const char *key;
        const char *value;
        bool has_entries = strdict_next(&iter, &key, &value);
        TEST_ASSERT_FALSE(has_entries);
        
        strdict_free(dict);
        return TEST_SUCCESS;
    }
    
    return TEST_SKIP;
}

#endif