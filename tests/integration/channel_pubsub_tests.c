#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../channel_transport.h"
#include "../../channel_service.h"
#include "../framework/test_framework.h"
#include "../framework/test_utils.h"
#include <string.h>

static test_result_t test_channel_backend_mode(test_case_t *test);
static test_result_t test_channel_local_inbound_dispatch(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_area_uid(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_area_override(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_movement_delta(test_case_t *test);
static test_result_t test_channel_auto_backend_subscription_continuity(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_no_duplicates(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_group_scope(test_case_t *test);
static test_result_t test_channel_chtalk_delivery_muted_filter(test_case_t *test);
static test_result_t test_channel_chtalk_delivery_ignore_filter(test_case_t *test);
static test_result_t test_channel_quote_delivery_filtering(test_case_t *test);
static test_result_t test_channel_send_without_init_fallback(test_case_t *test);

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
    else if (strcmp(test->test_type, "channel_effective_subscriptions_area_uid_test") == 0) {
        result = test_channel_effective_subscriptions_area_uid(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_area_override_test") == 0) {
        result = test_channel_effective_subscriptions_area_override(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_movement_delta_test") == 0) {
        result = test_channel_effective_subscriptions_movement_delta(test);
    }
    else if (strcmp(test->test_type, "channel_auto_backend_subscription_continuity_test") == 0) {
        result = test_channel_auto_backend_subscription_continuity(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_no_duplicates_test") == 0) {
        result = test_channel_effective_subscriptions_no_duplicates(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_group_scope_test") == 0) {
        result = test_channel_effective_subscriptions_group_scope(test);
    }
    else if (strcmp(test->test_type, "channel_chtalk_delivery_muted_filter_test") == 0) {
        result = test_channel_chtalk_delivery_muted_filter(test);
    }
    else if (strcmp(test->test_type, "channel_chtalk_delivery_ignore_filter_test") == 0) {
        result = test_channel_chtalk_delivery_ignore_filter(test);
    }
    else if (strcmp(test->test_type, "channel_quote_delivery_filtering_test") == 0) {
        result = test_channel_quote_delivery_filtering(test);
    }
    else if (strcmp(test->test_type, "channel_send_without_init_fallback_test") == 0) {
        result = test_channel_send_without_init_fallback(test);
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

static test_result_t test_channel_effective_subscriptions_area_uid(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 123;
    area.area_topic = "";

    room.area = &area;
    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscription list for area UID test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:gossip") || !strstr(out, "rt:area:123")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected subscriptions to contain rt:gossip and rt:area:123, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_area_override(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 123;
    area.area_topic = "alendith-main";

    room.area = &area;
    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscription list for area override test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:area:alendith-main")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected subscriptions to contain rt:area:alendith-main, got: %s", out);
        return TEST_FAILURE;
    }

    if (strstr(out, "rt:area:123")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected area UID topic to be overridden, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_movement_delta(test_case_t *test)
{
    AREA_DATA area_a;
    AREA_DATA area_b;
    ROOM_INDEX_DATA room_a;
    ROOM_INDEX_DATA room_b;
    CHAR_DATA actor;
    char out_a[1024];
    char out_b[1024];
    int count_a;
    int count_b;

    (void)test;

    memset(&area_a, 0, sizeof(area_a));
    memset(&area_b, 0, sizeof(area_b));
    memset(&room_a, 0, sizeof(room_a));
    memset(&room_b, 0, sizeof(room_b));
    memset(&actor, 0, sizeof(actor));
    memset(out_a, 0, sizeof(out_a));
    memset(out_b, 0, sizeof(out_b));

    area_a.uid = 111;
    area_a.area_topic = "";
    area_b.uid = 222;
    area_b.area_topic = "";

    room_a.area = &area_a;
    room_b.area = &area_b;

    actor.in_room = &room_a;
    count_a = channel_service_describe_subscriptions(&actor, out_a, sizeof(out_a));

    actor.in_room = &room_b;
    count_b = channel_service_describe_subscriptions(&actor, out_b, sizeof(out_b));

    if (count_a <= 0 || count_b <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscription lists for movement delta test");
        return TEST_FAILURE;
    }

    if (!strstr(out_a, "rt:area:111")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected area A topic rt:area:111, got: %s", out_a);
        return TEST_FAILURE;
    }

    if (!strstr(out_b, "rt:area:222")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected area B topic rt:area:222, got: %s", out_b);
        return TEST_FAILURE;
    }

    if (strstr(out_b, "rt:area:111")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected area A topic to be absent after movement, got: %s", out_b);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_auto_backend_subscription_continuity(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char before[1024];
    char after[1024];
    int count_before;
    int count_after;
    bool success = true;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(before, 0, sizeof(before));
    memset(after, 0, sizeof(after));

    area.uid = 444;
    area.area_topic = "auto-stability";
    room.area = &area;
    actor.in_room = &room;

    count_before = channel_service_describe_subscriptions(&actor, before, sizeof(before));
    if (count_before <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty pre-backend subscription list in auto continuity test");
        return TEST_FAILURE;
    }

    game_settings.channel_backend = "auto";
    channel_transport_shutdown();
    if (!channel_transport_init()) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_transport_init failed in auto continuity test");
        game_settings.channel_backend = "legacy_iterative";
        return TEST_FAILURE;
    }

    channel_transport_pulse();
    channel_transport_pulse();

    count_after = channel_service_describe_subscriptions(&actor, after, sizeof(after));
    if (count_after <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty post-backend subscription list in auto continuity test");
        success = false;
    }

    if (str_cmp(before, after)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected stable subscriptions across auto backend cycle. before='%s' after='%s'",
                      before, after);
        success = false;
    }

    channel_transport_shutdown();
    game_settings.channel_backend = "legacy_iterative";

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_effective_subscriptions_no_duplicates(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char out[1024];
    char work[1024];
    char seen[64][128];
    int seen_count = 0;
    int count;
    char *token;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));
    memset(work, 0, sizeof(work));
    memset(seen, 0, sizeof(seen));

    area.uid = 999;
    area.area_topic = "";
    room.area = &area;
    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscription list for dedupe test");
        return TEST_FAILURE;
    }

    strlcpy(work, out, sizeof(work));
    token = strtok(work, ",");
    while (token) {
        int i;
        while (*token == ' ')
            token++;

        if (*token != '\0') {
            for (i = 0; i < seen_count; i++) {
                if (!str_cmp(seen[i], token)) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "Duplicate subscription topic found: %s in '%s'", token, out);
                    return TEST_FAILURE;
                }
            }

            if (seen_count >= 64) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                            "Too many topics while checking duplicates");
                return TEST_FAILURE;
            }

            strlcpy(seen[seen_count], token, sizeof(seen[seen_count]));
            seen_count++;
        }

        token = strtok(NULL, ",");
    }

    if (seen_count != count) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Topic count mismatch while checking duplicates: expected %d parsed %d ('%s')",
                      count, seen_count, out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_group_scope(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    GROUP_DATA group;
    CHAR_DATA actor;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&group, 0, sizeof(group));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 777;
    area.area_topic = "";
    room.area = &area;

    group.valid = true;
    group.id[0] = 4242;
    group.id[1] = 99;

    actor.in_room = &room;
    actor.group = &group;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscription list for group scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:group:4242:99")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected subscriptions to contain group scope topic, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_chtalk_delivery_muted_filter(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    CHAR_DATA *muted = NULL;
    CHAR_DATA *quiet = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *muted_desc = NULL;
    DESCRIPTOR_DATA *quiet_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    CHURCH_DATA church;
    const char *saved_backend;
    bool success = true;

    (void)test;

    memset(&church, 0, sizeof(church));
    church.uid = 321;
    church.colour1 = "{W";
    church.colour2 = "{B";

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc) ||
        !test_utils_create_fake_player(&muted, &muted_desc) ||
        !test_utils_create_fake_player(&quiet, &quiet_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        if (muted || muted_desc)
            test_utils_destroy_fake_player(muted, muted_desc);
        if (quiet || quiet_desc)
            test_utils_destroy_fake_player(quiet, quiet_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("Sender");
    free_string(receiver->name);
    receiver->name = str_dup("Receiver");
    free_string(muted->name);
    muted->name = str_dup("Muted");
    free_string(quiet->name);
    quiet->name = str_dup("Quiet");

    sender->church = &church;
    receiver->church = &church;
    muted->church = &church;
    quiet->church = &church;

    SET_BIT(muted->comm, COMM_NOCT);
    SET_BIT(quiet->comm, COMM_QUIET);

    saved_descriptor_list = descriptor_list;
    sender_desc->next = receiver_desc;
    receiver_desc->next = muted_desc;
    muted_desc->next = quiet_desc;
    quiet_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';
    muted_desc->outtop = 0;
    if (muted_desc->outbuf)
        muted_desc->outbuf[0] = '\0';
    quiet_desc->outtop = 0;
    if (quiet_desc->outbuf)
        quiet_desc->outbuf[0] = '\0';

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "legacy_iterative";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(quiet, quiet_desc);
        test_utils_destroy_fake_player(muted, muted_desc);
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send(sender, "chtalk", "test-message")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed for chtalk delivery test");
        success = false;
    }

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "test-message")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected church receiver to get chtalk message");
        success = false;
    }

    if (muted_desc->outtop > 0 && strstr(muted_desc->outbuf, "test-message")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected muted church receiver not to get chtalk message");
        success = false;
    }

    if (quiet_desc->outtop > 0 && strstr(quiet_desc->outbuf, "test-message")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected quiet church receiver not to get chtalk message");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(quiet, quiet_desc);
    test_utils_destroy_fake_player(muted, muted_desc);
    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_chtalk_delivery_ignore_filter(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    CHAR_DATA *ignoring = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *ignoring_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    CHURCH_DATA church;
    IGNORE_DATA *ignore_entry = NULL;
    const char *saved_backend;
    bool success = true;

    (void)test;

    memset(&church, 0, sizeof(church));
    church.uid = 654;
    church.colour1 = "{W";
    church.colour2 = "{B";

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc) ||
        !test_utils_create_fake_player(&ignoring, &ignoring_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        if (ignoring || ignoring_desc)
            test_utils_destroy_fake_player(ignoring, ignoring_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("Sender");
    free_string(receiver->name);
    receiver->name = str_dup("Receiver");
    free_string(ignoring->name);
    ignoring->name = str_dup("Ignoring");

    sender->church = &church;
    receiver->church = &church;
    ignoring->church = &church;

    ignore_entry = new_ignore();
    ignore_entry->name = str_dup(sender->name);
    ignore_entry->reason = str_dup("test");
    ignore_entry->next = ignoring->pcdata->ignoring;
    ignoring->pcdata->ignoring = ignore_entry;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = receiver_desc;
    receiver_desc->next = ignoring_desc;
    ignoring_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';
    ignoring_desc->outtop = 0;
    if (ignoring_desc->outbuf)
        ignoring_desc->outbuf[0] = '\0';

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "legacy_iterative";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(ignoring, ignoring_desc);
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send(sender, "chtalk", "test-ignore")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed for chtalk ignore delivery test");
        success = false;
    }

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "test-ignore")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-ignoring church receiver to get chtalk message");
        success = false;
    }

    if (ignoring_desc->outtop > 0 && strstr(ignoring_desc->outbuf, "test-ignore")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected ignoring church receiver not to get chtalk message");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(ignoring, ignoring_desc);
    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_quote_delivery_filtering(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    CHAR_DATA *noquote = NULL;
    CHAR_DATA *quiet = NULL;
    CHAR_DATA *ignoring = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *noquote_desc = NULL;
    DESCRIPTOR_DATA *quiet_desc = NULL;
    DESCRIPTOR_DATA *ignoring_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    IGNORE_DATA *ignore_entry = NULL;
    const char *saved_backend;
    bool success = true;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Quote Test Area";
    room.area = &area;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc) ||
        !test_utils_create_fake_player(&noquote, &noquote_desc) ||
        !test_utils_create_fake_player(&quiet, &quiet_desc) ||
        !test_utils_create_fake_player(&ignoring, &ignoring_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        if (noquote || noquote_desc)
            test_utils_destroy_fake_player(noquote, noquote_desc);
        if (quiet || quiet_desc)
            test_utils_destroy_fake_player(quiet, quiet_desc);
        if (ignoring || ignoring_desc)
            test_utils_destroy_fake_player(ignoring, ignoring_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("Sender");

    sender->in_room = &room;
    receiver->in_room = &room;
    noquote->in_room = &room;
    quiet->in_room = &room;
    ignoring->in_room = &room;

    REMOVE_BIT(receiver->comm, COMM_NOQUOTE);
    REMOVE_BIT(receiver->comm, COMM_QUIET);
    REMOVE_BIT(noquote->comm, COMM_QUIET);
    REMOVE_BIT(quiet->comm, COMM_NOQUOTE);
    REMOVE_BIT(ignoring->comm, COMM_NOQUOTE);
    REMOVE_BIT(ignoring->comm, COMM_QUIET);

    SET_BIT(noquote->comm, COMM_NOQUOTE);
    SET_BIT(quiet->comm, COMM_QUIET);

    ignore_entry = new_ignore();
    ignore_entry->name = str_dup(sender->name);
    ignore_entry->reason = str_dup("test");
    ignore_entry->next = ignoring->pcdata->ignoring;
    ignoring->pcdata->ignoring = ignore_entry;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = receiver_desc;
    receiver_desc->next = noquote_desc;
    noquote_desc->next = quiet_desc;
    quiet_desc->next = ignoring_desc;
    ignoring_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';
    noquote_desc->outtop = 0;
    if (noquote_desc->outbuf)
        noquote_desc->outbuf[0] = '\0';
    quiet_desc->outtop = 0;
    if (quiet_desc->outbuf)
        quiet_desc->outbuf[0] = '\0';
    ignoring_desc->outtop = 0;
    if (ignoring_desc->outbuf)
        ignoring_desc->outbuf[0] = '\0';

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "legacy_iterative";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(ignoring, ignoring_desc);
        test_utils_destroy_fake_player(quiet, quiet_desc);
        test_utils_destroy_fake_player(noquote, noquote_desc);
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send(sender, "quote", "quote-test")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed for quote delivery test");
        success = false;
    }

    if (noquote_desc->outtop > 0 && strstr(noquote_desc->outbuf, "quote-test")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected COMM_NOQUOTE receiver not to get quote message");
        success = false;
    }

    if (quiet_desc->outtop > 0 && strstr(quiet_desc->outbuf, "quote-test")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected COMM_QUIET receiver not to get quote message");
        success = false;
    }

    if (ignoring_desc->outtop > 0 && strstr(ignoring_desc->outbuf, "quote-test")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected ignoring receiver not to get quote message");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(ignoring, ignoring_desc);
    test_utils_destroy_fake_player(quiet, quiet_desc);
    test_utils_destroy_fake_player(noquote, noquote_desc);
    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_send_without_init_fallback(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    CHURCH_DATA church;
    bool success = true;

    (void)test;

    memset(&church, 0, sizeof(church));
    church.uid = 777;
    church.colour1 = "{W";
    church.colour2 = "{B";

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        return TEST_ERROR;
    }

    sender->church = &church;
    receiver->church = &church;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = receiver_desc;
    receiver_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    channel_service_shutdown();

    if (!channel_service_send(sender, "chtalk", "fallback-no-init")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected channel_service_send to succeed via local fallback while service is not initialized");
        success = false;
    }

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "fallback-no-init")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected chtalk fallback delivery while service is not initialized");
        success = false;
    }

    descriptor_list = saved_descriptor_list;
    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

#endif /* BUILD_TESTS */
