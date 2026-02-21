#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../channel_transport.h"
#include "../framework/test_framework.h"
#include <string.h>

static test_result_t test_channel_backend_mode(test_case_t *test);
static test_result_t test_channel_local_inbound_dispatch(test_case_t *test);

static int g_inbound_count = 0;
static CHANNEL_MESSAGE g_last_msg;

static void test_inbound_handler(const CHANNEL_MESSAGE *msg)
{
    if (!msg)
        return;

    g_inbound_count++;
    g_last_msg = *msg;
}

test_result_t run_channel_pubsub_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "channel_backend_mode_test") == 0) {
        result = test_channel_backend_mode(test);
    }
    else if (strcmp(test->test_type, "channel_local_inbound_dispatch_test") == 0) {
        result = test_channel_local_inbound_dispatch(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown channel pubsub test type: %s", test->test_type);
    }

    return result;
}

static test_result_t test_channel_backend_mode(test_case_t *test)
{
    const char *mode;
    const char *expected_backend;
    json_t *expected_backends;
    bool expect_mode_enum = false;
    int expected_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;
    bool success = true;

    if (!test || !test->config)
        return TEST_ERROR;

    json_t *input = json_object_get(test->config, "input");
    if (!json_is_object(input))
        return TEST_ERROR;

    mode = test_json_get_string(input, "mode");
    expected_backend = test_json_get_string(input, "expected_backend");
    expected_backends = json_object_get(input, "expected_backends");

    if (json_object_get(input, "expected_mode")) {
        expect_mode_enum = true;
        expected_mode = test_json_get_int(input, "expected_mode");
    }

    game_settings.channel_backend = (char *)(mode ? mode : "legacy_iterative");
    channel_transport_shutdown();

    if (!channel_transport_init()) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_transport_init failed in backend mode test");
        return TEST_FAILURE;
    }

    if (expected_backends && json_is_array(expected_backends)) {
        bool match = false;
        size_t i;
        json_t *entry;
        const char *actual_backend = channel_transport_backend_name();

        json_array_foreach(expected_backends, i, entry) {
            if (json_is_string(entry) && !str_cmp(json_string_value(entry), actual_backend)) {
                match = true;
                break;
            }
        }

        if (!match) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "Backend '%s' not in expected_backends list", actual_backend);
            success = false;
        }
    } else if (expected_backend && str_cmp(expected_backend, channel_transport_backend_name())) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected backend '%s', got '%s'",
                      expected_backend, channel_transport_backend_name());
        success = false;
    }

    if (expect_mode_enum && channel_transport_backend_mode() != expected_mode) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected mode enum %d, got %d",
                      expected_mode, channel_transport_backend_mode());
        success = false;
    }

    channel_transport_shutdown();
    game_settings.channel_backend = "legacy_iterative";

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_local_inbound_dispatch(test_case_t *test)
{
    CHANNEL_MESSAGE msg;
    bool success = true;

    (void)test;

    game_settings.channel_backend = "local";
    channel_transport_shutdown();

    if (!channel_transport_init()) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_transport_init failed in local dispatch test");
        return TEST_FAILURE;
    }

    g_inbound_count = 0;
    memset(&g_last_msg, 0, sizeof(g_last_msg));
    channel_transport_set_inbound_handler(test_inbound_handler);

    msg.channel_id = "gossip";
    msg.topic = "rt:gossip";
    msg.sender_name = "Tester";
    msg.sender_id0 = 101;
    msg.sender_id1 = 202;
    msg.message_text = "hello local queue";
    msg.timestamp = current_time;

    if (!channel_transport_publish(msg.topic, &msg)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_transport_publish failed for local backend");
        success = false;
    }

    channel_transport_pulse();

    if (g_inbound_count != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected inbound callback count 1, got %d", g_inbound_count);
        success = false;
    }

    if (!g_last_msg.channel_id || str_cmp(g_last_msg.channel_id, "gossip")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Inbound callback message channel_id mismatch");
        success = false;
    }

    if (!g_last_msg.message_text || str_cmp(g_last_msg.message_text, "hello local queue")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Inbound callback message_text mismatch");
        success = false;
    }

    channel_transport_set_inbound_handler(NULL);
    channel_transport_shutdown();
    game_settings.channel_backend = "legacy_iterative";

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

#endif /* BUILD_TESTS */
