#ifdef BUILD_TESTS

#include <string.h>

#include "buffer_function_cases.h"
#include "../../merc.h"
#include "../../log.h"
#include "../../utils/buffer.h"

bool is_buffer_permutation_scenario(const char *scenario)
{
    return scenario && strcmp(scenario, "operation_permutations") == 0;
}

test_result_t run_buffer_permutation_scenario(const char *scenario, json_t *test_case)
{
    enum {
        OP_ADD_BUF = 0,
        OP_ADD_CHAR = 1,
        OP_BPRINTF = 2,
        OP_CLEAR = 3
    };
    int depth;
    int max_ops = 4;
    int total = 1;
    int seq;
    int d;

    if (!is_buffer_permutation_scenario(scenario))
        return TEST_SKIP;

    depth = test_json_get_int(test_case, "depth");
    if (depth <= 0 || depth > 6) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "operation_permutations invalid depth: %d",
                     depth);
        return TEST_ERROR;
    }

    for (d = 0; d < depth; d++)
        total *= max_ops;

    for (seq = 0; seq < total; seq++) {
        BUFFER *buffer = new_buf_size(16);
        int code = seq;

        if (!buffer) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "operation_permutations new_buf_size failed");
            return TEST_FAILURE;
        }

        for (d = 0; d < depth; d++) {
            int op = code % max_ops;
            code /= max_ops;

            switch (op) {
                case OP_ADD_BUF:
                    (void)add_buf(buffer, "ab");
                    break;
                case OP_ADD_CHAR:
                    (void)add_buf_char(buffer, 'Z');
                    break;
                case OP_BPRINTF:
                    (void)bprintf(buffer, "%s", "xy");
                    break;
                case OP_CLEAR:
                    clear_buf(buffer);
                    break;
                default:
                    break;
            }

            if (buffer->state == BUFFER_SAFE) {
                size_t actual_len = strlen(buf_string(buffer));
                if (buf_len(buffer) != actual_len) {
                    free_buf(buffer);
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "permutation len invariant failed: seq=%d step=%d tracked=%zu actual=%zu",
                                 seq,
                                 d,
                                 buf_len(buffer),
                                 actual_len);
                    return TEST_FAILURE;
                }

                if (buf_capacity(buffer) < buf_len(buffer) + 1) {
                    free_buf(buffer);
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "permutation capacity invariant failed: seq=%d step=%d cap=%zu len=%zu",
                                 seq,
                                 d,
                                 buf_capacity(buffer),
                                 buf_len(buffer));
                    return TEST_FAILURE;
                }

                if (buf_remaining(buffer) != (buf_capacity(buffer) - buf_len(buffer) - 1)) {
                    free_buf(buffer);
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "permutation remaining invariant failed: seq=%d step=%d rem=%zu cap=%zu len=%zu",
                                 seq,
                                 d,
                                 buf_remaining(buffer),
                                 buf_capacity(buffer),
                                 buf_len(buffer));
                    return TEST_FAILURE;
                }
            }
        }

        free_buf(buffer);
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
