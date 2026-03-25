#ifdef BUILD_TESTS

#include <string.h>
#include <jansson.h>

#include "../framework/test_framework.h"
#include "../../log.h"
#include "../../sentience_link.h"
#include "../../mxp_links.h"
#include "../../utils/buffer.h"

/* Forward declarations for scenario runners */
static test_result_t run_url_encode_scenario(json_t *tc);
static test_result_t run_queue_add_scenario(json_t *tc);
static test_result_t run_queue_json_scenario(json_t *tc);
static test_result_t run_osc8_gmcp_scenario(json_t *tc);
static test_result_t run_osc8_telnet_scenario(json_t *tc);
static test_result_t run_filter_staff_scenario(json_t *tc);
static test_result_t run_queue_sequential_scenario(json_t *tc);
static test_result_t run_queue_overflow_scenario(json_t *tc);

/* ── URL encode ──────────────────────────────────────── */

static test_result_t run_url_encode_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *input = test_json_get_string(params, "input");
    const char *expected_output = test_json_get_string(expected, "output");
    int expected_len = test_json_get_int(expected, "length");

    char buf[512];
    int len = link_url_encode(buf, sizeof(buf), input);

    TEST_ASSERT_STR_EQ(expected_output, buf);
    TEST_ASSERT_INT_EQ(expected_len, len);

    return TEST_SUCCESS;
}

/* ── Queue add ───────────────────────────────────────── */

static test_result_t run_queue_add_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    const char *text = test_json_get_string(params, "text");
    const char *hint = test_json_get_string(params, "hint");
    const char *category = test_json_get_string(params, "category");

    json_t *actions_arr = json_object_get(params, "actions");
    sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS] = {0};
    int num_actions = 0;

    if (actions_arr && json_is_array(actions_arr)) {
        num_actions = (int)json_array_size(actions_arr);
        if (num_actions > SENTIENCE_LINK_MAX_ACTIONS)
            num_actions = SENTIENCE_LINK_MAX_ACTIONS;
        for (int i = 0; i < num_actions; i++) {
            json_t *a = json_array_get(actions_arr, i);
            actions[i].label = (char *)test_json_get_string(a, "label");
            actions[i].cmd   = (char *)test_json_get_string(a, "cmd");
            actions[i].hint  = (char *)test_json_get_string(a, "hint");
        }
    }

    const char *id = sentience_link_queue_add(&queue, text, hint, category,
                                               actions, num_actions);

    const char *expected_id = test_json_get_string(expected, "id");
    int expected_count = test_json_get_int(expected, "count");

    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_STR_EQ(expected_id, id);
    TEST_ASSERT_INT_EQ(expected_count, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Queue to JSON ───────────────────────────────────── */

static test_result_t run_queue_json_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    /* Add entries from params.entries[] */
    json_t *entries = json_object_get(params, "entries");
    if (entries && json_is_array(entries)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(entries, idx, entry) {
            const char *text = test_json_get_string(entry, "text");
            const char *hint = test_json_get_string(entry, "hint");
            const char *cat  = test_json_get_string(entry, "category");
            json_t *acts = json_object_get(entry, "actions");
            sentience_link_action_t actions[SENTIENCE_LINK_MAX_ACTIONS] = {0};
            int nact = 0;
            if (acts && json_is_array(acts)) {
                nact = (int)json_array_size(acts);
                if (nact > SENTIENCE_LINK_MAX_ACTIONS) nact = SENTIENCE_LINK_MAX_ACTIONS;
                for (int i = 0; i < nact; i++) {
                    json_t *a = json_array_get(acts, i);
                    actions[i].label = (char *)test_json_get_string(a, "label");
                    actions[i].cmd   = (char *)test_json_get_string(a, "cmd");
                    actions[i].hint  = (char *)test_json_get_string(a, "hint");
                }
            }
            sentience_link_queue_add(&queue, text, hint, cat, actions, nact);
        }
    }

    json_t *result = sentience_link_queue_to_json(&queue);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(json_is_array(result));

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, (int)json_array_size(result));

    /* Verify first entry structure if present */
    if (expected_count > 0) {
        json_t *first = json_array_get(result, 0);
        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(json_object_get(first, "id"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "text"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "category"));
        TEST_ASSERT_NOT_NULL(json_object_get(first, "actions"));
        TEST_ASSERT_TRUE(json_is_array(json_object_get(first, "actions")));

        const char *exp_id = test_json_get_string(expected, "first_id");
        if (exp_id) {
            TEST_ASSERT_STR_EQ(exp_id, json_string_value(json_object_get(first, "id")));
        }
        const char *exp_cat = test_json_get_string(expected, "first_category");
        if (exp_cat) {
            TEST_ASSERT_STR_EQ(exp_cat, json_string_value(json_object_get(first, "category")));
        }
    }

    json_decref(result);
    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── OSC 8 GMCP output ──────────────────────────────── */

static test_result_t run_osc8_gmcp_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *link_id = test_json_get_string(params, "link_id");
    const char *text = test_json_get_string(params, "text");
    const char *expected_output = test_json_get_string(expected, "output");

    BUFFER *buf = new_buf();
    link_osc8_gmcp(buf, link_id, text);
    const char *result = buf_string(buf);

    TEST_ASSERT_STR_EQ(expected_output, result);

    free_buf(buf);
    return TEST_SUCCESS;
}

/* ── OSC 8 telnet output ─────────────────────────────── */

static test_result_t run_osc8_telnet_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    const char *cmd = test_json_get_string(params, "cmd");
    const char *text = test_json_get_string(params, "text");
    const char *expected_output = test_json_get_string(expected, "output");

    BUFFER *buf = new_buf();
    link_osc8_telnet(buf, cmd, text);
    const char *result = buf_string(buf);

    TEST_ASSERT_STR_EQ(expected_output, result);

    free_buf(buf);
    return TEST_SUCCESS;
}

/* ── Trust filtering ─────────────────────────────────── */

static test_result_t run_filter_staff_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    bool is_staff = test_json_get_bool(params, "is_staff");
    json_t *items_arr = json_object_get(params, "items");
    if (!items_arr || !json_is_array(items_arr)) return TEST_ERROR;

    int nitems = (int)json_array_size(items_arr);
    mxp_cmd_hint_t items[SENTIENCE_LINK_MAX_ACTIONS] = {0};
    mxp_cmd_hint_t out[SENTIENCE_LINK_MAX_ACTIONS] = {0};

    for (int i = 0; i < nitems && i < SENTIENCE_LINK_MAX_ACTIONS; i++) {
        json_t *item = json_array_get(items_arr, i);
        items[i].cmd = test_json_get_string(item, "cmd");
        items[i].hint = test_json_get_string(item, "hint");
        items[i].staff_only = test_json_get_bool(item, "staff_only");
    }

    int nf = sentience_link_filter_staff(items, nitems, out,
                                          SENTIENCE_LINK_MAX_ACTIONS, is_staff);

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, nf);

    if (nf > 0) {
        const char *exp_first = test_json_get_string(expected, "first_cmd");
        if (exp_first)
            TEST_ASSERT_STR_EQ(exp_first, out[0].cmd);

        const char *exp_last = test_json_get_string(expected, "last_cmd");
        if (exp_last)
            TEST_ASSERT_STR_EQ(exp_last, out[nf - 1].cmd);
    }

    return TEST_SUCCESS;
}

/* ── Queue sequential IDs ────────────────────────────── */

static test_result_t run_queue_sequential_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    json_t *texts = json_object_get(params, "texts");
    if (!texts || !json_is_array(texts)) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    json_t *exp_ids = json_object_get(expected, "ids");
    size_t idx;
    json_t *val;

    json_array_foreach(texts, idx, val) {
        const char *text = json_string_value(val);
        sentience_link_action_t act = { .label = "Look", .cmd = "look", .hint = NULL };
        const char *id = sentience_link_queue_add(&queue, text, NULL, "obj", &act, 1);
        TEST_ASSERT_NOT_NULL(id);

        if (exp_ids && idx < json_array_size(exp_ids)) {
            const char *exp_id = json_string_value(json_array_get(exp_ids, idx));
            TEST_ASSERT_STR_EQ(exp_id, id);
        }
    }

    int expected_count = test_json_get_int(expected, "count");
    TEST_ASSERT_INT_EQ(expected_count, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Queue capacity overflow ─────────────────────────── */

static test_result_t run_queue_overflow_scenario(json_t *tc)
{
    json_t *expected = json_object_get(tc, "expected");
    if (!expected) return TEST_ERROR;

    sentience_link_queue_t queue;
    sentience_link_queue_init(&queue);

    sentience_link_action_t act = { .label = "Look", .cmd = "look x", .hint = NULL };

    /* Fill to capacity */
    for (int i = 0; i < SENTIENCE_LINK_QUEUE_MAX; i++) {
        const char *id = sentience_link_queue_add(&queue, "item", NULL, "obj", &act, 1);
        if (!id) {
            sentience_link_queue_free(&queue);
            return TEST_FAILURE;
        }
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_LINK_QUEUE_MAX, queue.count);

    /* One more should fail */
    const char *overflow_id = sentience_link_queue_add(&queue, "overflow", NULL, "obj",
                                                        &act, 1);

    bool expect_null = test_json_get_bool(expected, "overflow_returns_null");
    if (expect_null) {
        TEST_ASSERT_NULL(overflow_id);
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_LINK_QUEUE_MAX, queue.count);

    sentience_link_queue_free(&queue);
    return TEST_SUCCESS;
}

/* ── Main dispatcher ─────────────────────────────────── */

test_result_t run_sentience_link_test_case(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *func_name;
    size_t index;
    json_t *tc;

    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test missing configuration");
        return TEST_ERROR;
    }

    input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test missing input");
        return TEST_ERROR;
    }

    func_name = test_json_get_string(input, "function");
    test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "Sentience link test requires test_cases array");
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, tc) {
        test_result_t result;

        if (strcmp(func_name, "url_encode") == 0) {
            result = run_url_encode_scenario(tc);
        } else if (strcmp(func_name, "queue_add") == 0) {
            result = run_queue_add_scenario(tc);
        } else if (strcmp(func_name, "queue_json") == 0) {
            result = run_queue_json_scenario(tc);
        } else if (strcmp(func_name, "osc8_gmcp") == 0) {
            result = run_osc8_gmcp_scenario(tc);
        } else if (strcmp(func_name, "osc8_telnet") == 0) {
            result = run_osc8_telnet_scenario(tc);
        } else if (strcmp(func_name, "filter_staff") == 0) {
            result = run_filter_staff_scenario(tc);
        } else if (strcmp(func_name, "queue_sequential") == 0) {
            result = run_queue_sequential_scenario(tc);
        } else if (strcmp(func_name, "queue_overflow") == 0) {
            result = run_queue_overflow_scenario(tc);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Unknown link function: %s", func_name);
            return TEST_ERROR;
        }

        if (result != TEST_SUCCESS)
            return result;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */