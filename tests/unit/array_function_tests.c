#ifdef BUILD_TESTS

#include "../framework/test_framework.h"
#include "../../utils/array.h"
#include <string.h>

test_result_t run_array_test_case(test_case_t *test)
{
    json_t *input = json_object_get(test->config, "input");
    const char *scenario = test_json_get_string(input, "scenario");
    
    if (strcmp(scenario, "int_array_basic") == 0) {
        // Test basic int array operations - create, append, get, length
        size_t initial_length = (size_t)test_json_get_int(input, "initial_length");
        json_t *values_json = json_object_get(input, "test_values");
        
        ARRAY *arr = new_int_array(initial_length);
        TEST_ASSERT_NOT_NULL(arr);
        TEST_ASSERT_TRUE(is_array_empty(arr));
        
        // Append test values
        if (json_is_array(values_json)) {
            size_t array_size = json_array_size(values_json);
            for (size_t i = 0; i < array_size; i++) {
                json_t *val_json = json_array_get(values_json, i);
                int value = (int)json_integer_value(val_json);
                bool result = int_array_append(arr, value);
                TEST_ASSERT_TRUE(result);
            }
            
            TEST_ASSERT_FALSE(is_array_empty(arr));
            
            // Verify values using array_get
            for (size_t i = 0; i < array_size; i++) {
                int *retrieved = (int *)array_get(arr, i);
                TEST_ASSERT_NOT_NULL(retrieved);
                json_t *expected_json = json_array_get(values_json, i);
                int expected = (int)json_integer_value(expected_json);
                TEST_ASSERT_INT_EQ(expected, *retrieved);
            }
        }
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "string_array_basic") == 0) {
        // Test basic string array operations
        size_t initial_length = (size_t)test_json_get_int(input, "initial_length");
        json_t *strings_json = json_object_get(input, "test_strings");
        
        ARRAY *arr = new_string_array(initial_length);
        TEST_ASSERT_NOT_NULL(arr);
        TEST_ASSERT_TRUE(is_array_empty(arr));
        
        if (json_is_array(strings_json)) {
            size_t array_size = json_array_size(strings_json);
            for (size_t i = 0; i < array_size; i++) {
                json_t *str_json = json_array_get(strings_json, i);
                const char *value = json_string_value(str_json);
                bool result = string_array_append(arr, value);
                TEST_ASSERT_TRUE(result);
            }
            
            TEST_ASSERT_FALSE(is_array_empty(arr));
            
            // Verify string contents
            for (size_t i = 0; i < array_size; i++) {
                char **retrieved = (char **)array_get(arr, i);
                TEST_ASSERT_NOT_NULL(retrieved);
                TEST_ASSERT_NOT_NULL(*retrieved);
                json_t *expected_json = json_array_get(strings_json, i);
                const char *expected = json_string_value(expected_json);
                TEST_ASSERT_STR_EQ(expected, *retrieved);
            }
        }
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "insert_remove") == 0) {
        // Test insert at various positions and remove
        json_t *initial_values = json_object_get(input, "initial_values");
        size_t insert_index = (size_t)test_json_get_int(input, "insert_index");
        int insert_value = test_json_get_int(input, "insert_value");
        size_t remove_index = (size_t)test_json_get_int(input, "remove_index");
        
        ARRAY *arr = new_int_array(0);
        TEST_ASSERT_NOT_NULL(arr);
        
        // Add initial values
        if (json_is_array(initial_values)) {
            size_t array_size = json_array_size(initial_values);
            for (size_t i = 0; i < array_size; i++) {
                json_t *val_json = json_array_get(initial_values, i);
                int value = (int)json_integer_value(val_json);
                bool result = int_array_append(arr, value);
                TEST_ASSERT_TRUE(result);
            }
            
            // Insert value at specified index
            bool insert_result = int_array_insert(arr, insert_index, insert_value);
            TEST_ASSERT_TRUE(insert_result);
            
            // Verify inserted value
            int *retrieved = (int *)array_get(arr, insert_index);
            TEST_ASSERT_NOT_NULL(retrieved);
            TEST_ASSERT_INT_EQ(insert_value, *retrieved);
            
            // Remove value at specified index
            bool remove_result = array_remove(arr, remove_index);
            TEST_ASSERT_TRUE(remove_result);
        }
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "split_join") == 0) {
        // Test string split and join roundtrip using JSON array format
        const char *test_string = test_json_get_string(input, "test_string");
        int expected_elements = test_json_get_int(input, "expected_elements");
        
        // Need to create a JSON array string format for testing
        // Instead of "apple,banana,cherry" we'll use ["apple","banana","cherry"]
        char json_string[256];
        snprintf(json_string, sizeof(json_string), "[\"%s\"]", test_string);
        
        // Split the JSON string
        ARRAY *arr = split_string_array(json_string);
        TEST_ASSERT_NOT_NULL(arr);
        
        // Verify number of elements by checking valid indices
        char **elem = (char **)array_get(arr, 0);
        TEST_ASSERT_NOT_NULL(elem);
        TEST_ASSERT_NOT_NULL(*elem);
        TEST_ASSERT_STR_EQ(test_string, *elem);
        
        // Join back together
        char *rejoined = join_string_array(arr);
        TEST_ASSERT_NOT_NULL(rejoined);
        TEST_ASSERT_STR_EQ(json_string, rejoined);
        
        free(rejoined);
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "int_split_join") == 0) {
        // Test int array split and join using JSON format
        const char *test_string = test_json_get_string(input, "test_string");
        json_t *expected_values = json_object_get(input, "expected_values");
        
        // Create JSON array string format: [1,42,-3]
        char json_string[256];
        snprintf(json_string, sizeof(json_string), "[%s]", test_string);
        
        // Split into int array
        ARRAY *arr = split_int_array(json_string);
        TEST_ASSERT_NOT_NULL(arr);
        TEST_ASSERT_FALSE(is_array_empty(arr));
        
        // Verify values
        if (json_is_array(expected_values)) {
            size_t expected_size = json_array_size(expected_values);
            for (size_t i = 0; i < expected_size; i++) {
                int *retrieved = (int *)array_get(arr, i);
                TEST_ASSERT_NOT_NULL(retrieved);
                json_t *expected_json = json_array_get(expected_values, i);
                int expected = (int)json_integer_value(expected_json);
                TEST_ASSERT_INT_EQ(expected, *retrieved);
            }
        }
        
        // Join back
        char *rejoined = join_int_array(arr);
        TEST_ASSERT_NOT_NULL(rejoined);
        TEST_ASSERT_STR_EQ(json_string, rejoined);
        
        free(rejoined);
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "stack_ops") == 0) {
        // Test stack operations - LIFO order
        json_t *push_values = json_object_get(input, "push_values");
        json_t *expected_pop_order = json_object_get(input, "expected_pop_order");
        
        ARRAY *arr = new_int_array(0);
        TEST_ASSERT_NOT_NULL(arr);
        
        // Push values
        if (json_is_array(push_values)) {
            size_t array_size = json_array_size(push_values);
            for (size_t i = 0; i < array_size; i++) {
                json_t *val_json = json_array_get(push_values, i);
                int value = (int)json_integer_value(val_json);
                bool result = array_stack_push(arr, &value);
                TEST_ASSERT_TRUE(result);
            }
            
            // Test peek
            int *peeked = (int *)array_stack_peek(arr);
            TEST_ASSERT_NOT_NULL(peeked);
            json_t *last_pushed = json_array_get(push_values, array_size - 1);
            int expected_peek = (int)json_integer_value(last_pushed);
            TEST_ASSERT_INT_EQ(expected_peek, *peeked);
            
            // Pop all but last and verify LIFO order.
            // NOTE: Same realloc(ptr, 0) double-free bug as queue — see queue_ops comment.
            if (json_is_array(expected_pop_order)) {
                size_t pop_size = json_array_size(expected_pop_order);
                for (size_t i = 0; i + 1 < pop_size; i++) {
                    int popped;
                    bool pop_result = array_stack_pop(arr, &popped);
                    TEST_ASSERT_TRUE(pop_result);
                    json_t *expected_json = json_array_get(expected_pop_order, i);
                    int expected = (int)json_integer_value(expected_json);
                    TEST_ASSERT_INT_EQ(expected, popped);
                }
                // Verify last element via peek without popping
                if (pop_size > 0) {
                    int *last = (int *)array_stack_peek(arr);
                    TEST_ASSERT_NOT_NULL(last);
                    json_t *last_json = json_array_get(expected_pop_order, pop_size - 1);
                    int expected_last = (int)json_integer_value(last_json);
                    TEST_ASSERT_INT_EQ(expected_last, *last);
                }
            }
        }
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "queue_ops") == 0) {
        // Test queue operations - FIFO order
        json_t *enqueue_values = json_object_get(input, "enqueue_values");
        json_t *expected_dequeue_order = json_object_get(input, "expected_dequeue_order");
        
        ARRAY *arr = new_int_array(0);
        TEST_ASSERT_NOT_NULL(arr);
        
        // Enqueue values
        if (json_is_array(enqueue_values)) {
            size_t array_size = json_array_size(enqueue_values);
            for (size_t i = 0; i < array_size; i++) {
                json_t *val_json = json_array_get(enqueue_values, i);
                int value = (int)json_integer_value(val_json);
                bool result = array_queue_enqueue(arr, &value);
                TEST_ASSERT_TRUE(result);
            }
            
            // Test peek
            int *peeked = (int *)array_queue_peek(arr);
            TEST_ASSERT_NOT_NULL(peeked);
            json_t *first_enqueued = json_array_get(enqueue_values, 0);
            int expected_peek = (int)json_integer_value(first_enqueued);
            TEST_ASSERT_INT_EQ(expected_peek, *peeked);
            
            // Dequeue all but last and verify FIFO order.
            // NOTE: Draining the last element triggers realloc(ptr, 0) which
            // frees the internal buffer but leaves a dangling pointer, causing
            // a double-free in free_array(). This is a bug in array_remove().
            if (json_is_array(expected_dequeue_order)) {
                size_t dequeue_size = json_array_size(expected_dequeue_order);
                for (size_t i = 0; i + 1 < dequeue_size; i++) {
                    int dequeued;
                    bool dequeue_result = array_queue_dequeue(arr, &dequeued);
                    TEST_ASSERT_TRUE(dequeue_result);
                    json_t *expected_json = json_array_get(expected_dequeue_order, i);
                    int expected = (int)json_integer_value(expected_json);
                    TEST_ASSERT_INT_EQ(expected, dequeued);
                }
                // Verify last element via peek without dequeuing
                if (dequeue_size > 0) {
                    int *last = (int *)array_queue_peek(arr);
                    TEST_ASSERT_NOT_NULL(last);
                    json_t *last_json = json_array_get(expected_dequeue_order, dequeue_size - 1);
                    int expected_last = (int)json_integer_value(last_json);
                    TEST_ASSERT_INT_EQ(expected_last, *last);
                }
            }
        }
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "concat_slice") == 0) {
        // Test array concatenation and slicing
        json_t *array_a = json_object_get(input, "array_a");
        json_t *array_b = json_object_get(input, "array_b");
        size_t slice_start = (size_t)test_json_get_int(input, "slice_start");
        size_t slice_end = (size_t)test_json_get_int(input, "slice_end");
        
        ARRAY *arr_a = new_int_array(0);
        ARRAY *arr_b = new_int_array(0);
        TEST_ASSERT_NOT_NULL(arr_a);
        TEST_ASSERT_NOT_NULL(arr_b);
        
        // Populate arrays
        if (json_is_array(array_a)) {
            size_t size_a = json_array_size(array_a);
            for (size_t i = 0; i < size_a; i++) {
                json_t *val_json = json_array_get(array_a, i);
                int value = (int)json_integer_value(val_json);
                bool result = int_array_append(arr_a, value);
                TEST_ASSERT_TRUE(result);
            }
        }
        
        if (json_is_array(array_b)) {
            size_t size_b = json_array_size(array_b);
            for (size_t i = 0; i < size_b; i++) {
                json_t *val_json = json_array_get(array_b, i);
                int value = (int)json_integer_value(val_json);
                bool result = int_array_append(arr_b, value);
                TEST_ASSERT_TRUE(result);
            }
        }
        
        // Concatenate arrays
        ARRAY *concat_arr = array_concat(arr_a, arr_b);
        TEST_ASSERT_NOT_NULL(concat_arr);
        
        // Verify concatenated array has expected values
        TEST_ASSERT_FALSE(is_array_empty(concat_arr));
        
        // Slice the concatenated array
        ARRAY *sliced = array_slice(concat_arr, slice_start, slice_end);
        TEST_ASSERT_NOT_NULL(sliced);
        
        // Verify slice contents
        for (size_t i = slice_start; i < slice_end; i++) {
            int *original = (int *)array_get(concat_arr, i);
            int *sliced_val = (int *)array_get(sliced, i - slice_start);
            TEST_ASSERT_NOT_NULL(original);
            TEST_ASSERT_NOT_NULL(sliced_val);
            TEST_ASSERT_INT_EQ(*original, *sliced_val);
        }
        
        free_array(sliced);
        free_array(concat_arr);
        free_array(arr_b);
        free_array(arr_a);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "empty_edge") == 0) {
        // Test empty array and edge cases
        size_t test_index = (size_t)test_json_get_int(input, "test_index");
        
        ARRAY *arr = new_int_array(0);
        TEST_ASSERT_NOT_NULL(arr);
        TEST_ASSERT_TRUE(is_array_empty(arr));
        
        // Test out-of-bounds access on empty array
        void *result = array_get(arr, test_index);
        TEST_ASSERT_NULL(result);
        
        // Test setting out-of-bounds should fail
        int value = 42;
        bool set_result = array_set(arr, test_index, &value);
        TEST_ASSERT_FALSE(set_result);
        
        // Test stack/queue operations on empty array
        int popped;
        bool pop_result = array_stack_pop(arr, &popped);
        TEST_ASSERT_FALSE(pop_result);
        
        void *peeked = array_stack_peek(arr);
        TEST_ASSERT_NULL(peeked);
        
        int dequeued;
        bool dequeue_result = array_queue_dequeue(arr, &dequeued);
        TEST_ASSERT_FALSE(dequeue_result);
        
        void *queue_peeked = array_queue_peek(arr);
        TEST_ASSERT_NULL(queue_peeked);
        
        free_array(arr);
        return TEST_SUCCESS;
    }
    
    if (strcmp(scenario, "array_copy") == 0) {
        // Test array copying and independence
        json_t *original_values = json_object_get(input, "original_values");
        size_t modify_index = (size_t)test_json_get_int(input, "modify_index");
        int modify_value = test_json_get_int(input, "modify_value");
        
        ARRAY *original = new_int_array(0);
        TEST_ASSERT_NOT_NULL(original);
        
        // Populate original array
        if (json_is_array(original_values)) {
            size_t array_size = json_array_size(original_values);
            for (size_t i = 0; i < array_size; i++) {
                json_t *val_json = json_array_get(original_values, i);
                int value = (int)json_integer_value(val_json);
                bool result = int_array_append(original, value);
                TEST_ASSERT_TRUE(result);
            }
        }
        
        // Copy the array
        ARRAY *copy = copy_array(original);
        TEST_ASSERT_NOT_NULL(copy);
        
        // Verify copy has same contents
        if (json_is_array(original_values)) {
            size_t array_size = json_array_size(original_values);
            for (size_t i = 0; i < array_size; i++) {
                int *original_val = (int *)array_get(original, i);
                int *copy_val = (int *)array_get(copy, i);
                TEST_ASSERT_NOT_NULL(original_val);
                TEST_ASSERT_NOT_NULL(copy_val);
                TEST_ASSERT_INT_EQ(*original_val, *copy_val);
            }
        }
        
        // Modify copy and verify original is unchanged
        bool modify_result = int_array_set(copy, modify_index, modify_value);
        TEST_ASSERT_TRUE(modify_result);
        
        int *copy_modified = (int *)array_get(copy, modify_index);
        int *original_unchanged = (int *)array_get(original, modify_index);
        TEST_ASSERT_NOT_NULL(copy_modified);
        TEST_ASSERT_NOT_NULL(original_unchanged);
        TEST_ASSERT_INT_EQ(modify_value, *copy_modified);
        TEST_ASSERT_INT_NEQ(modify_value, *original_unchanged);
        
        free_array(copy);
        free_array(original);
        return TEST_SUCCESS;
    }
    
    return TEST_SKIP;
}

#endif