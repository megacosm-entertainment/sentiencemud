#ifdef BUILD_TESTS

#include <string.h>
#include <stdio.h>
#include "../../merc.h"
#include "../../recycle.h"
#include "../../utils/buffer.h"
#include "../framework/test_framework.h"

test_result_t run_memory_util_test_case(test_case_t *test)
{
    if (!test) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory util test missing test case");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory util test missing input configuration");
        return TEST_ERROR;
    }

    const char *scenario = test_json_get_string(input, "scenario");
    if (!scenario) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Memory util test missing scenario");
        return TEST_ERROR;
    }

    /* alloc_mem/free_mem basic operations */
    if (strcmp(scenario, "alloc_free_basic") == 0) {
        int alloc_size = test_json_get_int(input, "alloc_size");
        if (alloc_size <= 0) alloc_size = 256;
        
        void *ptr = alloc_mem(alloc_size);
        TEST_ASSERT_NOT_NULL(ptr);
        
        /* Write test pattern to verify memory is accessible */
        memset(ptr, 0xAB, alloc_size);
        
        /* Verify pattern */
        unsigned char *bytes = (unsigned char *)ptr;
        for (int i = 0; i < alloc_size; i++) {
            TEST_ASSERT_INT_EQ(bytes[i], 0xAB);
        }
        
        free_mem(ptr, alloc_size);
        return TEST_SUCCESS;
    }

    /* Test zero size allocation */
    if (strcmp(scenario, "alloc_zero_size") == 0) {
        /* Zero size should still return valid pointer or handle gracefully */
        void *ptr = alloc_mem(0);
        /* The implementation uses calloc(1, sMem), so calloc(1, 0) returns valid ptr */
        TEST_ASSERT_NOT_NULL(ptr);
        free_mem(ptr, 0);
        return TEST_SUCCESS;
    }

    /* alloc_perm operations */
    if (strcmp(scenario, "alloc_perm_basic") == 0) {
        int alloc_size = test_json_get_int(input, "alloc_size");
        if (alloc_size <= 0) alloc_size = 512;
        
        void *ptr = alloc_perm(alloc_size);
        TEST_ASSERT_NOT_NULL(ptr);
        
        /* Write test pattern to verify memory is accessible */
        memset(ptr, 0xCD, alloc_size);
        
        /* Verify pattern */
        unsigned char *bytes = (unsigned char *)ptr;
        for (int i = 0; i < alloc_size; i++) {
            TEST_ASSERT_INT_EQ(bytes[i], 0xCD);
        }
        
        /* Note: alloc_perm memory is never freed by design */
        return TEST_SUCCESS;
    }

    /* str_dup with valid string */
    if (strcmp(scenario, "str_dup_basic") == 0) {
        const char *test_string = test_json_get_string(input, "test_string");
        if (!test_string) test_string = "test string";
        
        char *dup_str = str_dup(test_string);
        TEST_ASSERT_NOT_NULL(dup_str);
        TEST_ASSERT_STR_EQ(dup_str, test_string);
        TEST_ASSERT_TRUE(dup_str != test_string); /* Different pointers */
        
        /* Free the duplicated string */
        free_string(dup_str);
        return TEST_SUCCESS;
    }

    /* str_dup with empty string */
    if (strcmp(scenario, "str_dup_empty") == 0) {
        char *dup_str = str_dup("");
        TEST_ASSERT_NOT_NULL(dup_str);
        /* Empty string should return reference to str_empty */
        return TEST_SUCCESS;
    }

    /* str_dup with null string */
    if (strcmp(scenario, "str_dup_null") == 0) {
        char *dup_str = str_dup(NULL);
        TEST_ASSERT_NOT_NULL(dup_str);
        /* NULL string should return reference to str_empty */
        return TEST_SUCCESS;
    }

    /* Buffer basic operations */
    if (strcmp(scenario, "buffer_basic") == 0) {
        BUFFER *buf = new_buf();
        TEST_ASSERT_NOT_NULL(buf);
        TEST_ASSERT_NOT_NULL(buf->string);
        TEST_ASSERT_TRUE(buf->size > 0);
        
        free_buf(buf);
        return TEST_SUCCESS;
    }

    /* Buffer string operations */
    if (strcmp(scenario, "buffer_string_ops") == 0) {
        const char *test_string = test_json_get_string(input, "test_string");
        if (!test_string) test_string = "test buffer content";
        
        BUFFER *buf = new_buf();
        TEST_ASSERT_NOT_NULL(buf);
        
        bool result = add_buf(buf, test_string);
        TEST_ASSERT_TRUE(result);
        
        char *buf_content = buf_string(buf);
        TEST_ASSERT_NOT_NULL(buf_content);
        TEST_ASSERT_STR_EQ(buf_content, test_string);
        
        size_t len = buf_len(buf);
        TEST_ASSERT_INT_EQ(len, strlen(test_string));
        
        free_buf(buf);
        return TEST_SUCCESS;
    }

    /* Buffer with custom size */
    if (strcmp(scenario, "buffer_custom_size") == 0) {
        int buffer_size = test_json_get_int(input, "buffer_size");
        if (buffer_size <= 0) buffer_size = 2048;
        
        BUFFER *buf = new_buf_size(buffer_size);
        TEST_ASSERT_NOT_NULL(buf);
        TEST_ASSERT_TRUE(buf->size >= buffer_size);
        
        size_t capacity = buf_capacity(buf);
        TEST_ASSERT_TRUE(capacity >= (size_t)buffer_size);
        
        free_buf(buf);
        return TEST_SUCCESS;
    }

    /* Multiple allocations */
    if (strcmp(scenario, "multiple_alloc") == 0) {
        int alloc_count = test_json_get_int(input, "alloc_count");
        int alloc_size = test_json_get_int(input, "alloc_size");
        if (alloc_count <= 0) alloc_count = 5;
        if (alloc_size <= 0) alloc_size = 128;
        
        void **ptrs = alloc_mem(sizeof(void*) * alloc_count);
        TEST_ASSERT_NOT_NULL(ptrs);
        
        /* Allocate multiple blocks */
        for (int i = 0; i < alloc_count; i++) {
            ptrs[i] = alloc_mem(alloc_size);
            TEST_ASSERT_NOT_NULL(ptrs[i]);
            
            /* Write unique pattern to each block */
            memset(ptrs[i], 0x10 + i, alloc_size);
        }
        
        /* Verify patterns */
        for (int i = 0; i < alloc_count; i++) {
            unsigned char *bytes = (unsigned char *)ptrs[i];
            for (int j = 0; j < alloc_size; j++) {
                TEST_ASSERT_INT_EQ(bytes[j], 0x10 + i);
            }
        }
        
        /* Free all blocks */
        for (int i = 0; i < alloc_count; i++) {
            free_mem(ptrs[i], alloc_size);
        }
        
        free_mem(ptrs, sizeof(void*) * alloc_count);
        return TEST_SUCCESS;
    }

    /* Mixed memory operations */
    if (strcmp(scenario, "mixed_operations") == 0) {
        /* Mix alloc_mem, alloc_perm, str_dup, and buffer operations */
        void *mem_ptr = alloc_mem(512);
        TEST_ASSERT_NOT_NULL(mem_ptr);
        
        void *perm_ptr = alloc_perm(256);
        TEST_ASSERT_NOT_NULL(perm_ptr);
        
        char *dup_str = str_dup("mixed test string");
        TEST_ASSERT_NOT_NULL(dup_str);
        TEST_ASSERT_STR_EQ(dup_str, "mixed test string");
        
        BUFFER *buf = new_buf();
        TEST_ASSERT_NOT_NULL(buf);
        TEST_ASSERT_TRUE(add_buf(buf, "Buffer content"));
        
        char *buf_content = buf_string(buf);
        TEST_ASSERT_STR_EQ(buf_content, "Buffer content");
        
        /* Cleanup what we can */
        free_mem(mem_ptr, 512);
        free_string(dup_str);
        free_buf(buf);
        /* Note: perm_ptr is permanent and not freed */
        
        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                  "Unknown memory util scenario: %s", scenario);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */