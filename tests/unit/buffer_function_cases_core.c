#ifdef BUILD_TESTS

#include <stdlib.h>
#include <string.h>

#include "buffer_function_cases.h"
#include "../../merc.h"
#include "../../log.h"
#include "../../utils/buffer.h"

bool is_buffer_core_scenario(const char *scenario)
{
    return scenario && (
        strcmp(scenario, "new_buf_defaults") == 0 ||
        strcmp(scenario, "append_and_format") == 0 ||
        strcmp(scenario, "clear_resets_state") == 0 ||
        strcmp(scenario, "new_buf_size_honors_request") == 0 ||
        strcmp(scenario, "overflow_expected") == 0 ||
        strcmp(scenario, "remaining_consistency") == 0 ||
        strcmp(scenario, "new_buf_size_base_floor") == 0 ||
        strcmp(scenario, "null_and_empty_noop") == 0 ||
        strcmp(scenario, "growth_ladder") == 0 ||
        strcmp(scenario, "exact_limit_fill_then_fail") == 0 ||
        strcmp(scenario, "overflow_clear_recover") == 0 ||
        strcmp(scenario, "null_argument_guards") == 0 ||
        strcmp(scenario, "freed_buffer_guards") == 0
    );
}

test_result_t run_buffer_core_scenario(const char *scenario, json_t *test_case)
{
    if (strcmp(scenario, "new_buf_defaults") == 0) {
        BUFFER *buffer = new_buf();
        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf returned NULL");
            return TEST_FAILURE;
        }

        if (buf_len(buffer) != 0 || buffer->state != BUFFER_SAFE) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf defaults invalid: len=%zu state=%d",
                         buf_len(buffer),
                         buffer->state);
            free_buf(buffer);
            return TEST_FAILURE;
        }

        if (buf_capacity(buffer) < BASE_BUF) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf capacity too small: %zu",
                         buf_capacity(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        if (str_cmp(buf_string(buffer), "") != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf string not empty: '%s'",
                         buf_string(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        if (buf_remaining(buffer) != (buf_capacity(buffer) - 1)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf remaining mismatch: got %zu expected %zu",
                         buf_remaining(buffer),
                         buf_capacity(buffer) - 1);
            free_buf(buffer);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "append_and_format") == 0) {
        BUFFER *buffer = new_buf_size(16);
        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size returned NULL");
            return TEST_FAILURE;
        }

        if (!add_buf(buffer, "alpha") ||
            !add_buf_char(buffer, '-') ||
            !bprintf(buffer, "%s-%d", "beta", 42))
        {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "append/format operations failed unexpectedly");
            free_buf(buffer);
            return TEST_FAILURE;
        }

        if (str_cmp(buf_string(buffer), "alpha-beta-42") != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "buffer content mismatch: got '%s'",
                         buf_string(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        if (buf_len(buffer) != strlen("alpha-beta-42")) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "buffer len mismatch: got %zu expected %zu",
                         buf_len(buffer),
                         strlen("alpha-beta-42"));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "clear_resets_state") == 0) {
        BUFFER *buffer = new_buf();
        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf returned NULL");
            return TEST_FAILURE;
        }

        if (!add_buf(buffer, "xyz")) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf failed before clear");
            free_buf(buffer);
            return TEST_FAILURE;
        }

        buffer->state = BUFFER_OVERFLOW;
        clear_buf(buffer);

        if (buffer->state != BUFFER_SAFE || buf_len(buffer) != 0 || str_cmp(buf_string(buffer), "") != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "clear_buf did not reset state/len/string (state=%d len=%zu str='%s')",
                         buffer->state,
                         buf_len(buffer),
                         buf_string(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "new_buf_size_honors_request") == 0) {
        int requested = test_json_get_int(test_case, "requested_size");
        BUFFER *buffer;

        if (requested <= 0) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size_honors_request requires positive requested_size");
            return TEST_ERROR;
        }

        buffer = new_buf_size(requested);
        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size returned NULL");
            return TEST_FAILURE;
        }

        if (buf_capacity(buffer) < (size_t)requested) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf_size capacity too small: requested=%d got=%zu",
                         requested,
                         buf_capacity(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "overflow_expected") == 0) {
        BUFFER *buffer = new_buf_size(MAX_BUF_TOTAL);
        char *payload;
        bool ok;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size(MAX_BUF_TOTAL) returned NULL");
            return TEST_FAILURE;
        }

        payload = malloc((size_t)MAX_BUF_TOTAL + 1);
        if (!payload) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "overflow_expected payload allocation failed");
            return TEST_ERROR;
        }

        memset(payload, 'X', (size_t)MAX_BUF_TOTAL);
        payload[MAX_BUF_TOTAL] = '\0';

        ok = add_buf(buffer, payload);
        free(payload);

        if (ok) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf unexpectedly succeeded past MAX_BUF_TOTAL");
            return TEST_FAILURE;
        }

        if (buffer->state != BUFFER_OVERFLOW) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "expected BUFFER_OVERFLOW state, got %d",
                         buffer->state);
            return TEST_FAILURE;
        }

        if (add_buf(buffer, "still-fails")) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf succeeded after overflow state");
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "remaining_consistency") == 0) {
        BUFFER *buffer = new_buf_size(16);
        size_t before;
        size_t after;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size returned NULL");
            return TEST_FAILURE;
        }

        before = buf_remaining(buffer);
        if (!add_buf(buffer, "12345")) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf failed in remaining_consistency");
            return TEST_FAILURE;
        }

        after = buf_remaining(buffer);
        if (before < 5 || after != (before - 5)) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "buf_remaining mismatch: before=%zu after=%zu",
                         before,
                         after);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "new_buf_size_base_floor") == 0) {
        BUFFER *buffer = new_buf_size(1);

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size(1) returned NULL");
            return TEST_FAILURE;
        }

        if (buf_capacity(buffer) < BASE_BUF) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "new_buf_size(1) did not honor BASE_BUF floor: %zu",
                         buf_capacity(buffer));
            free_buf(buffer);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "null_and_empty_noop") == 0) {
        BUFFER *buffer = new_buf();
        size_t before_len;
        size_t before_remaining;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf returned NULL");
            return TEST_FAILURE;
        }

        if (!add_buf(buffer, "seed")) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf seed failed unexpectedly");
            return TEST_FAILURE;
        }

        before_len = buf_len(buffer);
        before_remaining = buf_remaining(buffer);

        if (!add_buf(buffer, NULL) || !add_buf(buffer, "")) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf NULL/empty should be successful no-op");
            return TEST_FAILURE;
        }

        if (buf_len(buffer) != before_len || buf_remaining(buffer) != before_remaining) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "NULL/empty no-op changed state: len %zu->%zu remaining %zu->%zu",
                         before_len,
                         buf_len(buffer),
                         before_remaining,
                         buf_remaining(buffer));
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "growth_ladder") == 0) {
        BUFFER *buffer = new_buf_size(16);
        size_t initial_cap;
        size_t max_cap;
        char *chunk;
        int i;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size(16) returned NULL");
            return TEST_FAILURE;
        }

        initial_cap = buf_capacity(buffer);
        if (initial_cap < BASE_BUF) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "initial capacity below BASE_BUF: %zu",
                         initial_cap);
            return TEST_FAILURE;
        }

        chunk = malloc(601);
        if (!chunk) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "growth_ladder chunk allocation failed");
            return TEST_ERROR;
        }
        memset(chunk, 'L', 600);
        chunk[600] = '\0';

        max_cap = initial_cap;
        for (i = 0; i < 8; i++) {
            if (!add_buf(buffer, chunk)) {
                free(chunk);
                free_buf(buffer);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "growth ladder append failed at iteration %d", i);
                return TEST_FAILURE;
            }

            if (buf_capacity(buffer) > max_cap)
                max_cap = buf_capacity(buffer);

            if (buf_capacity(buffer) < buf_len(buffer) + 1) {
                free(chunk);
                free_buf(buffer);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "capacity invariant violated at iteration %d: len=%zu cap=%zu",
                             i, buf_len(buffer), buf_capacity(buffer));
                return TEST_FAILURE;
            }
        }

        free(chunk);

        if (max_cap <= initial_cap) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "growth ladder never increased capacity: initial=%zu max=%zu",
                         initial_cap, max_cap);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "exact_limit_fill_then_fail") == 0) {
        BUFFER *buffer = new_buf_size(MAX_BUF_TOTAL);
        char *fit;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "new_buf_size(MAX_BUF_TOTAL) returned NULL");
            return TEST_FAILURE;
        }

        fit = malloc((size_t)MAX_BUF_TOTAL);
        if (!fit) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "exact_limit_fill_then_fail allocation failed");
            return TEST_ERROR;
        }

        memset(fit, 'Y', (size_t)MAX_BUF_TOTAL - 1);
        fit[MAX_BUF_TOTAL - 1] = '\0';

        if (!add_buf(buffer, fit)) {
            free(fit);
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "exact-limit add_buf failed unexpectedly");
            return TEST_FAILURE;
        }

        free(fit);

        if (buf_len(buffer) != (size_t)MAX_BUF_TOTAL - 1 || buf_remaining(buffer) != 0) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "exact-limit fill mismatch: len=%zu remaining=%zu",
                         buf_len(buffer), buf_remaining(buffer));
            return TEST_FAILURE;
        }

        if (add_buf_char(buffer, 'Z')) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "add_buf_char succeeded after exact limit fill");
            return TEST_FAILURE;
        }

        if (buffer->state != BUFFER_OVERFLOW) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "expected overflow state after exact limit exceed, got %d",
                         buffer->state);
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "overflow_clear_recover") == 0) {
        BUFFER *buffer = new_buf_size(MAX_BUF_TOTAL);
        char *payload = malloc((size_t)MAX_BUF_TOTAL + 1);

        if (!buffer || !payload) {
            if (payload) free(payload);
            if (buffer) free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "overflow_clear_recover setup failed");
            return TEST_ERROR;
        }

        memset(payload, 'Q', (size_t)MAX_BUF_TOTAL);
        payload[MAX_BUF_TOTAL] = '\0';

        (void)add_buf(buffer, payload);
        free(payload);

        if (buffer->state != BUFFER_OVERFLOW) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "expected overflow state before clear in recovery scenario");
            return TEST_FAILURE;
        }

        clear_buf(buffer);

        if (buffer->state != BUFFER_SAFE || buf_len(buffer) != 0) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "clear did not recover overflowed buffer: state=%d len=%zu",
                         buffer->state,
                         buf_len(buffer));
            return TEST_FAILURE;
        }

        if (!add_buf(buffer, "recover-ok") || str_cmp(buf_string(buffer), "recover-ok") != 0) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "buffer did not recover after clear from overflow");
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "null_argument_guards") == 0) {
        BUFFER *buffer = new_buf();

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "null_argument_guards setup failed");
            return TEST_FAILURE;
        }

        if (add_buf(NULL, "abc") ||
            add_buf_char(NULL, 'x') ||
            bprintf(NULL, "%s", "abc"))
        {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "NULL buffer guard expected failure");
            return TEST_FAILURE;
        }

        if (bprintf(buffer, NULL)) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "NULL format guard expected failure");
            return TEST_FAILURE;
        }

        clear_buf(NULL);

        if (buf_len(NULL) != 0 || buf_capacity(NULL) != 0 || buf_remaining(NULL) != 0) {
            free_buf(buffer);
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "NULL accessors expected zero: len=%zu cap=%zu rem=%zu",
                         buf_len(NULL),
                         buf_capacity(NULL),
                         buf_remaining(NULL));
            return TEST_FAILURE;
        }

        if (buf_string(NULL) == NULL || str_cmp(buf_string(NULL), "") != 0) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "buf_string(NULL) expected empty non-NULL string");
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    if (strcmp(scenario, "freed_buffer_guards") == 0) {
        BUFFER *buffer = new_buf();

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "freed_buffer_guards setup failed");
            return TEST_FAILURE;
        }

        if (!add_buf(buffer, "seed")) {
            free_buf(buffer);
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "freed_buffer_guards seed append failed");
            return TEST_FAILURE;
        }

        free_buf(buffer);

        if (add_buf(buffer, "x") ||
            add_buf_char(buffer, 'y') ||
            bprintf(buffer, "%s", "z"))
        {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "freed buffer mutation guard expected failure");
            return TEST_FAILURE;
        }

        clear_buf(buffer);

        if (buf_len(buffer) != 0 || buf_capacity(buffer) != 0 || buf_remaining(buffer) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "freed buffer accessors expected zero: len=%zu cap=%zu rem=%zu",
                         buf_len(buffer),
                         buf_capacity(buffer),
                         buf_remaining(buffer));
            return TEST_FAILURE;
        }

        if (buf_string(buffer) == NULL || str_cmp(buf_string(buffer), "") != 0) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "freed buffer string accessor expected empty non-NULL string");
            return TEST_FAILURE;
        }

        free_buf(buffer);
        return TEST_SUCCESS;
    }

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
