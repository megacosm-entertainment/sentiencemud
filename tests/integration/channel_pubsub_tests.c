#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../wilds.h"
#include "../../interp.h"
#include "channel_transport.h"
#include "channel_service.h"
#include "channel_filter.h"
#include "../framework/test_framework.h"
#include "../framework/test_utils.h"
#include <string.h>
#include <unistd.h>

static test_result_t test_channel_backend_mode(test_case_t *test);
static test_result_t test_channel_local_inbound_dispatch(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_area_uid(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_area_override(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_movement_delta(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_room_regular(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_room_wilds(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_room_instance(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_instance_scope(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_dungeon_scope(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_area_isolated(test_case_t *test);
static test_result_t test_channel_scope_instance_channel_defs_gating(test_case_t *test);
static test_result_t test_channel_auto_backend_subscription_continuity(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_no_duplicates(test_case_t *test);
static test_result_t test_channel_effective_subscriptions_group_scope(test_case_t *test);
static test_result_t test_channel_chtalk_delivery_muted_filter(test_case_t *test);
static test_result_t test_channel_chtalk_delivery_ignore_filter(test_case_t *test);
static test_result_t test_channel_quote_delivery_filtering(test_case_t *test);
static test_result_t test_channel_send_without_init_fallback(test_case_t *test);
static test_result_t test_channel_compact_publish_hydrated_delivery(test_case_t *test);
static test_result_t test_channel_local_end_to_end_delivery(test_case_t *test);
static test_result_t test_channel_modifier_order_behavior(test_case_t *test);
static test_result_t test_channel_tell_directed_delivery(test_case_t *test);
static test_result_t test_channel_tell_ignore_filter(test_case_t *test);
static test_result_t test_channel_tell_afk_replay_parity(test_case_t *test);
static test_result_t test_channel_tell_linkdead_replay_parity(test_case_t *test);
static test_result_t test_channel_tell_linkdead_quiet_buffer_parity(test_case_t *test);
static test_result_t test_channel_intone_object_targeted_parity(test_case_t *test);
static test_result_t test_channel_intone_filter_block(test_case_t *test);
static test_result_t test_channel_intone_filter_redact(test_case_t *test);
static test_result_t test_channel_intone_filter_review(test_case_t *test);
static test_result_t test_channel_intone_filter_def_block(test_case_t *test);

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
    else if (strcmp(test->test_type, "channel_effective_subscriptions_room_regular_test") == 0) {
        result = test_channel_effective_subscriptions_room_regular(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_room_wilds_test") == 0) {
        result = test_channel_effective_subscriptions_room_wilds(test);
    }
    else if (strcmp(test->test_type, "channel_effective_subscriptions_room_instance_test") == 0) {
        result = test_channel_effective_subscriptions_room_instance(test);
    }
    else if (strcmp(test->test_type, "channel_scope_inst_subscriptions_test") == 0) {
        result = test_channel_effective_subscriptions_instance_scope(test);
    }
    else if (strcmp(test->test_type, "channel_scope_dng_subscriptions_test") == 0) {
        result = test_channel_effective_subscriptions_dungeon_scope(test);
    }
    else if (strcmp(test->test_type, "channel_area_isolated_subscriptions_test") == 0) {
        result = test_channel_effective_subscriptions_area_isolated(test);
    }
    else if (strcmp(test->test_type, "channel_scope_instance_defs_gating_test") == 0) {
        result = test_channel_scope_instance_channel_defs_gating(test);
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
    else if (strcmp(test->test_type, "channel_compact_publish_hydrated_delivery_test") == 0) {
        result = test_channel_compact_publish_hydrated_delivery(test);
    }
    else if (strcmp(test->test_type, "channel_local_end_to_end_delivery_test") == 0) {
        result = test_channel_local_end_to_end_delivery(test);
    }
    else if (strcmp(test->test_type, "channel_modifier_order_behavior_test") == 0) {
        result = test_channel_modifier_order_behavior(test);
    }
    else if (strcmp(test->test_type, "channel_tell_directed_delivery_test") == 0) {
        result = test_channel_tell_directed_delivery(test);
    }
    else if (strcmp(test->test_type, "channel_tell_ignore_filter_test") == 0) {
        result = test_channel_tell_ignore_filter(test);
    }
    else if (strcmp(test->test_type, "channel_tell_afk_replay_parity_test") == 0) {
        result = test_channel_tell_afk_replay_parity(test);
    }
    else if (strcmp(test->test_type, "channel_tell_linkdead_replay_parity_test") == 0) {
        result = test_channel_tell_linkdead_replay_parity(test);
    }
    else if (strcmp(test->test_type, "channel_tell_linkdead_quiet_buffer_parity_test") == 0) {
        result = test_channel_tell_linkdead_quiet_buffer_parity(test);
    }
    else if (strcmp(test->test_type, "channel_intone_object_targeted_parity_test") == 0) {
        result = test_channel_intone_object_targeted_parity(test);
    }
    else if (strcmp(test->test_type, "channel_intone_filter_block_test") == 0) {
        result = test_channel_intone_filter_block(test);
    }
    else if (strcmp(test->test_type, "channel_intone_filter_redact_test") == 0) {
        result = test_channel_intone_filter_redact(test);
    }
    else if (strcmp(test->test_type, "channel_intone_filter_review_test") == 0) {
        result = test_channel_intone_filter_review(test);
    }
    else if (strcmp(test->test_type, "channel_intone_filter_def_block_test") == 0) {
        result = test_channel_intone_filter_def_block(test);
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

    memset(&msg, 0, sizeof(msg));
    msg.channel_id = "gossip";
    msg.topic = "rt:gossip";
    msg.sender_name = "Tester";
    msg.sender_uid = "101:202";
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
    char out[8192];
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
    char out[8192];
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

static test_result_t test_channel_effective_subscriptions_room_regular(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char out[8192];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 9191;
    room.area = &area;
    room.vnum = 4201;

    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for regular room scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:room:v:9191:4201")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected regular room topic rt:room:v:9191:4201, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_room_wilds(test_case_t *test)
{
    AREA_DATA area;
    WILDS_DATA wilds;
    ROOM_INDEX_DATA room;
    CHAR_DATA actor;
    char out[8192];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&wilds, 0, sizeof(wilds));
    memset(&room, 0, sizeof(room));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 9292;
    wilds.uid = 501;
    room.area = &area;
    room.wilds = &wilds;
    room.x = 77;
    room.y = 88;

    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for wilds room scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:room:wv:501:77:88")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected wilds room topic rt:room:wv:501:77:88, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_room_instance(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    ROOM_INDEX_DATA source;
    CHAR_DATA actor;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&source, 0, sizeof(source));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 9393;
    source.vnum = 7000;
    room.area = &area;
    room.source = &source;
    room.id[0] = 12;
    room.id[1] = 34;

    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for instance room scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:room:inst:") || !strstr(out, ":12:34")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected instance room topic shape rt:room:inst:<source>:12:34, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_instance_scope(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    ROOM_INDEX_DATA entrance;
    ROOM_INDEX_DATA source;
    INSTANCE_SECTION section;
    INSTANCE instance;
    CHAR_DATA actor;
    CHANNEL_DEF_DATA def;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&entrance, 0, sizeof(entrance));
    memset(&source, 0, sizeof(source));
    memset(&section, 0, sizeof(section));
    memset(&instance, 0, sizeof(instance));
    memset(&actor, 0, sizeof(actor));
    memset(&def, 0, sizeof(def));
    memset(out, 0, sizeof(out));

    source.vnum = 8800;
    entrance.source = &source;
    entrance.id[0] = 77;
    entrance.id[1] = 88;

    instance.valid = true;
    instance.floor = 2;
    instance.entrance = &entrance;

    section.valid = true;
    section.instance = &instance;

    area.uid = 9494;
    room.area = &area;
    room.instance_section = &section;

    actor.in_room = &room;


    strlcpy(def.id, "tell", sizeof(def.id));
    strlcpy(def.name, "Instance Test Scope", sizeof(def.name));
    strlcpy(def.command, "tell", sizeof(def.command));
    def.scope = CHANNEL_SCOPE_INSTANCE_ID;
    if (!channel_registry_upsert(&def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Failed to upsert channel definition in instance scope test");
        return TEST_FAILURE;
    }

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for instance scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:instance:room:8800:77:88")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected instance scope topic rt:instance:room:8800:77:88, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_dungeon_scope(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    INSTANCE_SECTION section;
    INSTANCE instance;
    DUNGEON dungeon;
    CHAR_DATA actor;
    CHANNEL_DEF_DATA def;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&section, 0, sizeof(section));
    memset(&instance, 0, sizeof(instance));
    memset(&dungeon, 0, sizeof(dungeon));
    memset(&actor, 0, sizeof(actor));
    memset(&def, 0, sizeof(def));
    memset(out, 0, sizeof(out));

    dungeon.valid = true;
    dungeon.uid[0] = 12345;
    dungeon.uid[1] = 67890;

    instance.valid = true;
    instance.dungeon = &dungeon;

    section.valid = true;
    section.instance = &instance;

    area.uid = 9595;
    room.area = &area;
    room.instance_section = &section;

    actor.in_room = &room;


    strlcpy(def.id, "tell", sizeof(def.id));
    strlcpy(def.name, "Dungeon Test Scope", sizeof(def.name));
    strlcpy(def.command, "tell", sizeof(def.command));
    def.scope = CHANNEL_SCOPE_DUNGEON_ID;
    if (!channel_registry_upsert(&def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Failed to upsert channel definition in dungeon scope test");
        return TEST_FAILURE;
    }

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for dungeon scope test");
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:dungeon:12345:67890")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected dungeon scope topic rt:dungeon:12345:67890, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_effective_subscriptions_area_isolated(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    BLUEPRINT blueprint;
    INSTANCE_SECTION section;
    INSTANCE instance;
    CHAR_DATA actor;
    char out[1024];
    int count;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&blueprint, 0, sizeof(blueprint));
    memset(&section, 0, sizeof(section));
    memset(&instance, 0, sizeof(instance));
    memset(&actor, 0, sizeof(actor));
    memset(out, 0, sizeof(out));

    area.uid = 9696;
    room.area = &area;

    blueprint.valid = true;
    blueprint.flags = INSTANCE_ISOLATED;

    instance.valid = true;
    instance.blueprint = &blueprint;

    section.valid = true;
    section.instance = &instance;

    room.instance_section = &section;
    actor.in_room = &room;

    count = channel_service_describe_subscriptions(&actor, out, sizeof(out));
    if (count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions for area isolated scope test");
        return TEST_FAILURE;
    }

    if (strstr(out, "rt:area:9696")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected isolated context to suppress area topic, got: %s", out);
        return TEST_FAILURE;
    }

    if (!strstr(out, "rt:gossip")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected global channel subscriptions to remain present, got: %s", out);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_channel_scope_instance_channel_defs_gating(test_case_t *test)
{
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    ROOM_INDEX_DATA entrance;
    ROOM_INDEX_DATA source;
    BLUEPRINT blueprint;
    INSTANCE_SECTION section;
    INSTANCE instance;
    CHAR_DATA actor;
    CHANNEL_DEF_DATA def;
    char out_before[1024];
    char out_after[1024];
    int count_before;
    int count_after;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&entrance, 0, sizeof(entrance));
    memset(&source, 0, sizeof(source));
    memset(&blueprint, 0, sizeof(blueprint));
    memset(&section, 0, sizeof(section));
    memset(&instance, 0, sizeof(instance));
    memset(&actor, 0, sizeof(actor));
    memset(&def, 0, sizeof(def));
    memset(out_before, 0, sizeof(out_before));
    memset(out_after, 0, sizeof(out_after));

    source.vnum = 8811;
    entrance.source = &source;
    entrance.id[0] = 71;
    entrance.id[1] = 72;

    blueprint.valid = true;
    blueprint.channel_defs = list_create(false);
    list_appendlink(blueprint.channel_defs, str_dup("other_channel"));

    instance.valid = true;
    instance.floor = 1;
    instance.entrance = &entrance;
    instance.blueprint = &blueprint;

    section.valid = true;
    section.instance = &instance;

    area.uid = 9797;
    room.area = &area;
    room.instance_section = &section;

    actor.in_room = &room;

    strlcpy(def.id, "inst_gated", sizeof(def.id));
    strlcpy(def.name, "Instance Gated Scope", sizeof(def.name));
    strlcpy(def.command, "instgated", sizeof(def.command));
    def.scope = CHANNEL_SCOPE_INSTANCE_ID;
    strlcpy(def.topic_pattern, "rt:test:$channel_id:inst", sizeof(def.topic_pattern));
    if (!channel_registry_upsert(&def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Failed to upsert channel definition in instance channel_defs gating test");
        if (blueprint.channel_defs) {
            ITERATOR it;
            char *id;
            iterator_start(&it, blueprint.channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
                free_string(id);
            iterator_stop(&it);
            list_destroy(blueprint.channel_defs);
        }
        return TEST_FAILURE;
    }

    count_before = channel_service_describe_subscriptions(&actor, out_before, sizeof(out_before));
    if (count_before <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions before channel_defs allow-list update");
        if (blueprint.channel_defs) {
            ITERATOR it;
            char *id;
            iterator_start(&it, blueprint.channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
                free_string(id);
            iterator_stop(&it);
            list_destroy(blueprint.channel_defs);
        }
        return TEST_FAILURE;
    }

    if (strstr(out_before, "rt:test:inst_gated:inst")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected gated instance topic to be absent before allow-list match, got: %s",
                      out_before);
        if (blueprint.channel_defs) {
            ITERATOR it;
            char *id;
            iterator_start(&it, blueprint.channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
                free_string(id);
            iterator_stop(&it);
            list_destroy(blueprint.channel_defs);
        }
        return TEST_FAILURE;
    }

    list_appendlink(blueprint.channel_defs, str_dup("inst_gated"));

    count_after = channel_service_describe_subscriptions(&actor, out_after, sizeof(out_after));
    if (count_after <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Expected non-empty subscriptions after channel_defs allow-list update");
        if (blueprint.channel_defs) {
            ITERATOR it;
            char *id;
            iterator_start(&it, blueprint.channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
                free_string(id);
            iterator_stop(&it);
            list_destroy(blueprint.channel_defs);
        }
        return TEST_FAILURE;
    }

    if (!strstr(out_after, "rt:test:inst_gated:inst")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected gated instance topic to appear after allow-list match, got: %s",
                      out_after);
        if (blueprint.channel_defs) {
            ITERATOR it;
            char *id;
            iterator_start(&it, blueprint.channel_defs);
            while ((id = (char *)iterator_nextdata(&it)))
                free_string(id);
            iterator_stop(&it);
            list_destroy(blueprint.channel_defs);
        }
        return TEST_FAILURE;
    }

    if (blueprint.channel_defs) {
        ITERATOR it;
        char *id;
        iterator_start(&it, blueprint.channel_defs);
        while ((id = (char *)iterator_nextdata(&it)))
            free_string(id);
        iterator_stop(&it);
        list_destroy(blueprint.channel_defs);
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

static test_result_t test_channel_compact_publish_hydrated_delivery(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    bool saved_compact;
    bool success = true;
    int i;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("CompactSender");
    free_string(receiver->name);
    receiver->name = str_dup("CompactReceiver");

    sender->id[0] = 90001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    receiver->id[0] = 90002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;

    {
        static AREA_DATA compact_area;
        static ROOM_INDEX_DATA compact_room;
        memset(&compact_area, 0, sizeof(compact_area));
        memset(&compact_room, 0, sizeof(compact_room));
        compact_area.name = "Compact Test Area";
        compact_room.area = &compact_area;
        sender->in_room = &compact_room;
        receiver->in_room = &compact_room;
        sender->next_in_room = receiver;
        receiver->next_in_room = NULL;
        compact_room.people = sender;
    }

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

    saved_backend = game_settings.channel_backend;
    saved_compact = game_settings.channel_publish_compact;
    game_settings.channel_backend = "redis";
    game_settings.channel_publish_compact = true;

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        game_settings.channel_publish_compact = saved_compact;
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (str_cmp(channel_service_backend_name(), "redis")) {
        channel_service_shutdown();
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        game_settings.channel_publish_compact = saved_compact;
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                    "Skipping compact publish hydration test: redis backend unavailable");
        return TEST_SKIP;
    }

    /* Allow inbound worker thread time to establish PSUBSCRIBE before publishing. */
    usleep(100000);

    if (!channel_service_send(sender, "quote", "compact-hydration-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed in compact hydration test");
        success = false;
    }

    for (i = 0; i < 100 && (!receiver_desc->outbuf || !strstr(receiver_desc->outbuf, "compact-hydration-check")); i++) {
        channel_service_pulse();
        usleep(20000);
    }

    if (!receiver_desc->outbuf || !strstr(receiver_desc->outbuf, "compact-hydration-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Receiver did not get hydrated compact publish delivery");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
    game_settings.channel_publish_compact = saved_compact;

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_local_end_to_end_delivery(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    const char *saved_backend;
    bool success = true;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Local E2E Area";
    area.uid = 555;
    room.area = &area;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("LocalSender");
    free_string(receiver->name);
    receiver->name = str_dup("LocalReceiver");

    sender->id[0] = 80001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;
    receiver->id[0] = 80002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

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

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send(sender, "gossip", "local-e2e-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed in local e2e delivery test");
        success = false;
    }

    /* channel_service_send() intentionally echoes sender output for gossip.
     * Clear sender output before pulse so this test asserts transport behavior only. */
    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';

    channel_service_pulse();

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "local-e2e-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Receiver did not get local backend gossip delivery");
        success = false;
    }

    if (sender_desc->outtop > 0 && strstr(sender_desc->outbuf, "local-e2e-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender should not receive their own gossip via transport delivery");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

static test_result_t test_channel_modifier_order_behavior(test_case_t *test)
{
    CHAR_DATA *sender = NULL;
    CHAR_DATA *receiver = NULL;
    DESCRIPTOR_DATA *sender_desc = NULL;
    DESCRIPTOR_DATA *receiver_desc = NULL;
    DESCRIPTOR_DATA *saved_descriptor_list;
    AREA_DATA area;
    ROOM_INDEX_DATA room;
    const char *saved_backend;
    const CHANNEL_DEF_DATA *existing;
    CHANNEL_DEF_DATA original_def;
    CHANNEL_DEF_DATA modified_def;
    bool had_original = false;
    bool success = true;

    (void)test;

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&original_def, 0, sizeof(original_def));
    memset(&modified_def, 0, sizeof(modified_def));

    area.name = "Modifier Order Area";
    area.uid = 556;
    room.area = &area;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        if (sender || sender_desc)
            test_utils_destroy_fake_player(sender, sender_desc);
        if (receiver || receiver_desc)
            test_utils_destroy_fake_player(receiver, receiver_desc);
        return TEST_ERROR;
    }

    free_string(sender->name);
    sender->name = str_dup("ModOrderSender");
    free_string(receiver->name);
    receiver->name = str_dup("ModOrderReceiver");

    sender->id[0] = 83001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    receiver->id[0] = 83002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

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

    existing = channel_registry_find("gossip");
    if (existing) {
        original_def = *existing;
        had_original = true;
        modified_def = *existing;
    } else {
        strlcpy(modified_def.id, "gossip", sizeof(modified_def.id));
        strlcpy(modified_def.name, "Gossip", sizeof(modified_def.name));
        strlcpy(modified_def.command, "gossip", sizeof(modified_def.command));
        modified_def.scope = CHANNEL_SCOPE_GLOBAL;
        modified_def.allow_player_flags = true;
        modified_def.persistent = true;
    }

    modified_def.modifiers = CHANNEL_MOD_COLOR_STRIP | CHANNEL_MOD_CAPS_NORMALIZE;

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "legacy_iterative";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    strlcpy(modified_def.modifier_order, "color_strip,caps_normalize", sizeof(modified_def.modifier_order));
    if (!channel_registry_upsert(&modified_def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Failed to upsert gossip def for modifier order test (color->caps)");
        success = false;
    }

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    if (!channel_service_send(sender, "gossip", "{RHELLOWORLD{x")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed for modifier order test pass 1");
        success = false;
    }

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "Helloworld")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected output containing 'Helloworld' for color->caps order, got: %s",
                      receiver_desc->outbuf ? receiver_desc->outbuf : "(null)");
        success = false;
    }

    strlcpy(modified_def.modifier_order, "caps_normalize,color_strip", sizeof(modified_def.modifier_order));
    if (!channel_registry_upsert(&modified_def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Failed to upsert gossip def for modifier order test (caps->color)");
        success = false;
    }

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    if (!channel_service_send(sender, "gossip", "{RHELLOWORLD{x")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send failed for modifier order test pass 2");
        success = false;
    }

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "helloworld")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Expected output containing 'helloworld' for caps->color order, got: %s",
                      receiver_desc->outbuf ? receiver_desc->outbuf : "(null)");
        success = false;
    }

    if (receiver_desc->outbuf && strstr(receiver_desc->outbuf, "Helloworld")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Did not expect capitalized 'Helloworld' for caps->color order, got: %s",
                      receiver_desc->outbuf);
        success = false;
    }

    if (had_original)
        channel_registry_upsert(&original_def);

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_tell_directed_delivery
 *
 * Verifies that channel_service_send_directed delivers a tell to the specific
 * recipient via local transport.  The sender should NOT receive a copy through
 * the transport path (their echo is handled by do_tell, not the inbound path).
 */
static test_result_t test_channel_tell_directed_delivery(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *receiver;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *receiver_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Tell Test Area";
    room.area = &area;

    sender->id[0]   = 91001;
    sender->id[1]   = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    receiver->id[0]  = 91002;
    receiver->id[1]  = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room   = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

    saved_descriptor_list = descriptor_list;
    sender_desc->next   = receiver_desc;
    receiver_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send_directed(sender, "tell", receiver, "tell-e2e-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_directed failed in tell directed delivery test");
        success = false;
    }

    channel_service_pulse();

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "tell-e2e-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Receiver did not get tell via local backend directed delivery");
        success = false;
    }

    /* The sender's output should NOT include the recipient's copy — that's handled
     * by do_tell's echo, which is separate from the transport path. */
    if (sender_desc->outtop > 0 && strstr(sender_desc->outbuf, "TellReceiver tells you")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender should not receive a tell-back echo via transport");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_tell_ignore_filter
 *
 * Verifies that the delivery-side ignore check in channel_deliver_tell_legacy
 * blocks delivery when the recipient is ignoring the sender.
 */
static test_result_t test_channel_tell_ignore_filter(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *receiver;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *receiver_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    static AREA_DATA area2;
    static ROOM_INDEX_DATA room2;
    IGNORE_DATA ignore_entry;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        return TEST_ERROR;
    }

    memset(&area2, 0, sizeof(area2));
    memset(&room2, 0, sizeof(room2));
    area2.name = "Tell Ignore Area";
    room2.area = &area2;

    sender->id[0]    = 92001;
    sender->id[1]    = 1;
    sender->position = POS_STANDING;
    sender->in_room  = &room2;

    receiver->id[0]  = 92002;
    receiver->id[1]  = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room2;

    sender->next_in_room   = receiver;
    receiver->next_in_room = NULL;
    room2.people = sender;

    /* Set up ignore: receiver ignores sender. */
    memset(&ignore_entry, 0, sizeof(ignore_entry));
    ignore_entry.name   = sender->name;
    ignore_entry.reason = "test ignore";
    ignore_entry.next   = NULL;
    receiver->pcdata->ignoring = &ignore_entry;

    saved_descriptor_list = descriptor_list;
    sender_desc->next   = receiver_desc;
    receiver_desc->next = NULL;
    descriptor_list = sender_desc;

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        receiver->pcdata->ignoring = NULL;
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    channel_service_send_directed(sender, "tell", receiver, "ignored-tell");

    channel_service_pulse();

    if (receiver_desc->outtop > 0 && strstr(receiver_desc->outbuf, "ignored-tell")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Receiver got tell despite having sender on ignore list");
        success = false;
    }

    channel_service_shutdown();
    receiver->pcdata->ignoring = NULL;
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_tell_afk_replay_parity
 *
 * Verifies parity with legacy tell/replay behavior for directed tells delivered
 * through ChannelService: tells to AFK recipients are buffered (not immediately
 * echoed) and are later displayed/cleared by do_replay.
 */
static test_result_t test_channel_tell_afk_replay_parity(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *receiver;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *receiver_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Tell Replay Parity Area";
    room.area = &area;

    sender->id[0] = 93001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    receiver->id[0] = 93002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

    SET_BIT(receiver->comm, COMM_AFK);

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

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        REMOVE_BIT(receiver->comm, COMM_AFK);
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send_directed(sender, "tell", receiver, "replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_directed failed in AFK replay parity test");
        success = false;
    }

    channel_service_pulse();

    if (receiver_desc->outtop > 0 && strstr(receiver_desc->outbuf, "replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "AFK recipient should not receive immediate tell echo before replay");
        success = false;
    }

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    do_replay(receiver, "");

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Replay did not print buffered tell for AFK recipient");
        success = false;
    }

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    do_replay(receiver, "");

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "You have no tells to replay.")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Replay did not clear tell buffer after first playback");
        success = false;
    }

    channel_service_shutdown();
    REMOVE_BIT(receiver->comm, COMM_AFK);
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_tell_linkdead_replay_parity
 *
 * Verifies legacy parity for linkdead recipients under ChannelService directed
 * tell delivery: tells are buffered while linkdead and replay prints/clears
 * them after reconnect.
 */
static test_result_t test_channel_tell_linkdead_replay_parity(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *receiver;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *receiver_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    DESCRIPTOR_DATA *saved_receiver_desc;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Tell Linkdead Replay Parity Area";
    room.area = &area;

    sender->id[0] = 94001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    receiver->id[0] = 94002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

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

    saved_receiver_desc = receiver->desc;
    receiver->desc = NULL;

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        receiver->desc = saved_receiver_desc;
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send_directed(sender, "tell", receiver, "linkdead-replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_directed failed in linkdead replay parity test");
        success = false;
    }

    channel_service_pulse();

    if (receiver_desc->outtop > 0 && strstr(receiver_desc->outbuf, "linkdead-replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Linkdead recipient should not receive immediate tell echo before reconnect/replay");
        success = false;
    }

    receiver->desc = receiver_desc;
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    do_replay(receiver, "");

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "linkdead-replay-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Replay did not print buffered tell for linkdead recipient");
        success = false;
    }

    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    do_replay(receiver, "");

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "You have no tells to replay.")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Replay did not clear linkdead tell buffer after first playback");
        success = false;
    }

    channel_service_shutdown();
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_tell_linkdead_quiet_buffer_parity
 *
 * Legacy parity: if recipient is linkdead, tell is buffered regardless of
 * recipient quiet mode. Quiet gating applies only to connected recipients.
 */
static test_result_t test_channel_tell_linkdead_quiet_buffer_parity(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *receiver;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *receiver_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    const char *saved_backend;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    DESCRIPTOR_DATA *saved_receiver_desc;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&receiver, &receiver_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    area.name = "Tell Linkdead Quiet Parity Area";
    room.area = &area;

    sender->id[0] = 95001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    receiver->id[0] = 95002;
    receiver->id[1] = 1;
    receiver->position = POS_STANDING;
    receiver->in_room = &room;

    sender->next_in_room = receiver;
    receiver->next_in_room = NULL;
    room.people = sender;

    SET_BIT(receiver->comm, COMM_QUIET);

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

    saved_receiver_desc = receiver->desc;
    receiver->desc = NULL;

    saved_backend = game_settings.channel_backend;
    game_settings.channel_backend = "local";

    if (!channel_service_init()) {
        receiver->desc = saved_receiver_desc;
        REMOVE_BIT(receiver->comm, COMM_QUIET);
        descriptor_list = saved_descriptor_list;
        game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");
        test_utils_destroy_fake_player(receiver, receiver_desc);
        test_utils_destroy_fake_player(sender, sender_desc);
        return TEST_FAILURE;
    }

    if (!channel_service_send_directed(sender, "tell", receiver, "linkdead-quiet-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_directed failed in linkdead quiet parity test");
        success = false;
    }

    channel_service_pulse();

    receiver->desc = receiver_desc;
    receiver_desc->outtop = 0;
    if (receiver_desc->outbuf)
        receiver_desc->outbuf[0] = '\0';

    do_replay(receiver, "");

    if (receiver_desc->outtop <= 0 || !strstr(receiver_desc->outbuf, "linkdead-quiet-parity-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Replay did not print buffered tell for linkdead+quiet recipient parity case");
        success = false;
    }

    channel_service_shutdown();
    REMOVE_BIT(receiver->comm, COMM_QUIET);
    descriptor_list = saved_descriptor_list;
    game_settings.channel_backend = (char *)(saved_backend ? saved_backend : "legacy_iterative");

    test_utils_destroy_fake_player(receiver, receiver_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_intone_object_targeted_parity
 *
 * Verifies object-targeted intone goes through channel service API and keeps
 * visible sender/room output semantics.
 */
static test_result_t test_channel_intone_object_targeted_parity(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *observer;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *observer_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    static OBJ_DATA obj;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&observer, &observer_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&obj, 0, sizeof(obj));

    area.name = "Intone Service Parity Area";
    room.area = &area;

    sender->id[0] = 96001;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    observer->id[0] = 96002;
    observer->id[1] = 1;
    observer->position = POS_STANDING;
    observer->in_room = &room;

    sender->next_in_room = observer;
    observer->next_in_room = NULL;
    room.people = sender;

    obj.name = "service_test_object";
    obj.short_descr = "a service test object";
    obj.in_room = &room;
    obj.next_content = NULL;
    room.contents = &obj;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = observer_desc;
    observer_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    observer_desc->outtop = 0;
    if (observer_desc->outbuf)
        observer_desc->outbuf[0] = '\0';

    if (!channel_service_send_object_targeted(sender, "intone", &obj, "service-intone-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_object_targeted returned false in intone parity test");
        success = false;
    }

    if (sender_desc->outtop <= 0 || !strstr(sender_desc->outbuf, "You intone to")
        || !strstr(sender_desc->outbuf, "service-intone-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender output missing expected intone text in intone parity test");
        success = false;
    }

    if (observer_desc->outtop <= 0 || !strstr(observer_desc->outbuf, "intones to")
        || !strstr(observer_desc->outbuf, "service-intone-check")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer output missing expected room intone text in intone parity test");
        success = false;
    }

    room.contents = NULL;
    descriptor_list = saved_descriptor_list;

    test_utils_destroy_fake_player(observer, observer_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_intone_filter_block
 *
 * Verifies object-targeted intone goes through service filter evaluation and
 * blocks marker-triggered blocked text from room observers.
 */
static test_result_t test_channel_intone_filter_block(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *observer;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *observer_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    static OBJ_DATA obj;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&observer, &observer_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&obj, 0, sizeof(obj));

    area.name = "Intone Filter Block Area";
    room.area = &area;

    sender->id[0] = 96101;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    observer->id[0] = 96102;
    observer->id[1] = 1;
    observer->position = POS_STANDING;
    observer->in_room = &room;

    sender->next_in_room = observer;
    observer->next_in_room = NULL;
    room.people = sender;

    obj.name = "filter_test_object";
    obj.short_descr = "a filter test object";
    obj.in_room = &room;
    obj.next_content = NULL;
    room.contents = &obj;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = observer_desc;
    observer_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    observer_desc->outtop = 0;
    if (observer_desc->outbuf)
        observer_desc->outbuf[0] = '\0';

    if (!channel_service_send_object_targeted(sender, "intone", &obj, "[block] forbidden intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_object_targeted failed in intone filter block test");
        success = false;
    }

    if (sender_desc->outtop <= 0 || !strstr(sender_desc->outbuf, "blocked by channel filters")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender missing filter block feedback in intone filter block test");
        success = false;
    }

    if (observer_desc->outtop > 0 && strstr(observer_desc->outbuf, "forbidden intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer received blocked intone text in intone filter block test");
        success = false;
    }

    room.contents = NULL;
    descriptor_list = saved_descriptor_list;

    test_utils_destroy_fake_player(observer, observer_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_intone_filter_redact
 *
 * Verifies object-targeted intone redaction marker keeps delivery but
 * replaces visible text with redacted payload.
 */
static test_result_t test_channel_intone_filter_redact(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *observer;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *observer_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    static OBJ_DATA obj;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&observer, &observer_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&obj, 0, sizeof(obj));

    area.name = "Intone Filter Redact Area";
    room.area = &area;

    sender->id[0] = 96201;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    observer->id[0] = 96202;
    observer->id[1] = 1;
    observer->position = POS_STANDING;
    observer->in_room = &room;

    sender->next_in_room = observer;
    observer->next_in_room = NULL;
    room.people = sender;

    obj.name = "filter_test_object";
    obj.short_descr = "a filter test object";
    obj.in_room = &room;
    obj.next_content = NULL;
    room.contents = &obj;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = observer_desc;
    observer_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    observer_desc->outtop = 0;
    if (observer_desc->outbuf)
        observer_desc->outbuf[0] = '\0';

    if (!channel_service_send_object_targeted(sender, "intone", &obj, "[redact] forbidden intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_object_targeted failed in intone filter redact test");
        success = false;
    }

    if (sender_desc->outtop <= 0 || !strstr(sender_desc->outbuf, "You intone to")
        || !strstr(sender_desc->outbuf, "[redacted by channel filter]")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender missing redacted intone output in intone filter redact test");
        success = false;
    }

    if (strstr(sender_desc->outbuf, "forbidden intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender saw unredacted intone text in intone filter redact test");
        success = false;
    }

    if (observer_desc->outtop <= 0 || !strstr(observer_desc->outbuf, "intones to")
        || !strstr(observer_desc->outbuf, "[redacted by channel filter]")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer missing redacted intone output in intone filter redact test");
        success = false;
    }

    if (strstr(observer_desc->outbuf, "forbidden intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer saw unredacted intone text in intone filter redact test");
        success = false;
    }

    room.contents = NULL;
    descriptor_list = saved_descriptor_list;

    test_utils_destroy_fake_player(observer, observer_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_intone_filter_review
 *
 * Verifies review marker keeps delivery visible and enqueues a staff report.
 */
static test_result_t test_channel_intone_filter_review(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *observer;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *observer_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    static OBJ_DATA obj;
    bool success = true;
    int report_count_before;
    int report_count_after;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&observer, &observer_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&obj, 0, sizeof(obj));

    area.name = "Intone Filter Review Area";
    room.area = &area;

    sender->id[0] = 96301;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    observer->id[0] = 96302;
    observer->id[1] = 1;
    observer->position = POS_STANDING;
    observer->in_room = &room;

    sender->next_in_room = observer;
    observer->next_in_room = NULL;
    room.people = sender;

    obj.name = "filter_test_object";
    obj.short_descr = "a filter test object";
    obj.in_room = &room;
    obj.next_content = NULL;
    room.contents = &obj;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = observer_desc;
    observer_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    observer_desc->outtop = 0;
    if (observer_desc->outbuf)
        observer_desc->outbuf[0] = '\0';

    report_count_before = channel_service_staff_report_count();

    if (!channel_service_send_object_targeted(sender, "intone", &obj, "[review] review intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_object_targeted failed in intone filter review test");
        success = false;
    }

    if (sender_desc->outtop <= 0 || !strstr(sender_desc->outbuf, "You intone to")
        || !strstr(sender_desc->outbuf, "review intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender missing visible intone output in intone filter review test");
        success = false;
    }

    if (observer_desc->outtop <= 0 || !strstr(observer_desc->outbuf, "intones to")
        || !strstr(observer_desc->outbuf, "review intone")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer missing visible intone output in intone filter review test");
        success = false;
    }

    report_count_after = channel_service_staff_report_count();
    if (report_count_after <= report_count_before) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Review marker did not enqueue staff report in intone filter review test");
        success = false;
    }

    room.contents = NULL;
    descriptor_list = saved_descriptor_list;

    test_utils_destroy_fake_player(observer, observer_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

/*
 * test_channel_intone_filter_def_block
 *
 * Verifies object-targeted intone honors channel-definition simple filter
 * rules (not only marker-based block tags).
 */
static test_result_t test_channel_intone_filter_def_block(test_case_t *test)
{
    CHAR_DATA *sender;
    CHAR_DATA *observer;
    DESCRIPTOR_DATA *sender_desc;
    DESCRIPTOR_DATA *observer_desc;
    DESCRIPTOR_DATA *saved_descriptor_list;
    static AREA_DATA area;
    static ROOM_INDEX_DATA room;
    static OBJ_DATA obj;
    CHANNEL_DEF_DATA def;
    const CHANNEL_DEF_DATA *orig_def;
    bool success = true;

    (void)test;

    if (!test_utils_create_fake_player(&sender, &sender_desc) ||
        !test_utils_create_fake_player(&observer, &observer_desc)) {
        return TEST_ERROR;
    }

    memset(&area, 0, sizeof(area));
    memset(&room, 0, sizeof(room));
    memset(&obj, 0, sizeof(obj));

    area.name = "Intone Def Filter Block Area";
    room.area = &area;

    sender->id[0] = 96401;
    sender->id[1] = 1;
    sender->position = POS_STANDING;
    sender->in_room = &room;

    observer->id[0] = 96402;
    observer->id[1] = 1;
    observer->position = POS_STANDING;
    observer->in_room = &room;

    sender->next_in_room = observer;
    observer->next_in_room = NULL;
    room.people = sender;

    obj.name = "filter_test_object";
    obj.short_descr = "a filter test object";
    obj.in_room = &room;
    obj.next_content = NULL;
    room.contents = &obj;

    saved_descriptor_list = descriptor_list;
    sender_desc->next = observer_desc;
    observer_desc->next = NULL;
    descriptor_list = sender_desc;

    sender_desc->outtop = 0;
    if (sender_desc->outbuf)
        sender_desc->outbuf[0] = '\0';
    observer_desc->outtop = 0;
    if (observer_desc->outbuf)
        observer_desc->outbuf[0] = '\0';

    orig_def = channel_registry_find("intone");
    memset(&def, 0, sizeof(def));
    if (orig_def)
        def = *orig_def;
    else {
        strlcpy(def.id, "intone", sizeof(def.id));
        strlcpy(def.name, "Intone", sizeof(def.name));
        strlcpy(def.command, "intone", sizeof(def.command));
        def.scope = CHANNEL_SCOPE_ROOM_WV;
    }

    def.filter_enabled = true;
    def.filter_mode = CHANNEL_FILTER_BLOCK;
    strlcpy(def.filter_simple, "forbidden", sizeof(def.filter_simple));
    def.filter_regex[0] = '\0';

    if (!channel_registry_upsert(&def)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_registry_upsert failed in intone def filter block test");
        success = false;
    }

    if (!channel_service_send_object_targeted(sender, "intone", &obj, "forbidden by def")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "channel_service_send_object_targeted failed in intone def filter block test");
        success = false;
    }

    if (sender_desc->outtop <= 0 || !strstr(sender_desc->outbuf, "blocked by channel filters")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Sender missing filter block feedback in intone def filter block test");
        success = false;
    }

    if (observer_desc->outtop > 0 && strstr(observer_desc->outbuf, "forbidden by def")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "Observer received channel-def blocked intone text");
        success = false;
    }

    if (orig_def)
        channel_registry_upsert(orig_def);
    else
        channel_registry_remove("intone");

    room.contents = NULL;
    descriptor_list = saved_descriptor_list;

    test_utils_destroy_fake_player(observer, observer_desc);
    test_utils_destroy_fake_player(sender, sender_desc);

    return success ? TEST_SUCCESS : TEST_FAILURE;
}

#endif /* BUILD_TESTS */
