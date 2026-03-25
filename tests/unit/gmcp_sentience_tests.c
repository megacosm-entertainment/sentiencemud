#ifdef BUILD_TESTS

#include <string.h>
#include <jansson.h>

#include "../framework/test_framework.h"
#include "../../log.h"
#include "../../gmcp_sentience.h"

/* Forward declarations for scenario runners */
static test_result_t run_gmcp_vitals_scenario(json_t *tc);
static test_result_t run_gmcp_stats_scenario(json_t *tc);
static test_result_t run_gmcp_combat_scenario(json_t *tc);
static test_result_t run_gmcp_worth_scenario(json_t *tc);
static test_result_t run_gmcp_identity_scenario(json_t *tc);
static test_result_t run_gmcp_room_scenario(json_t *tc);

/* --- Vitals scenario --- */

static test_result_t run_gmcp_vitals_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    long hp       = json_integer_value(json_object_get(params, "hp"));
    long max_hp   = json_integer_value(json_object_get(params, "max_hp"));
    long mana     = json_integer_value(json_object_get(params, "mana"));
    long max_mana = json_integer_value(json_object_get(params, "max_mana"));
    long move     = json_integer_value(json_object_get(params, "move"));
    long max_move = json_integer_value(json_object_get(params, "max_move"));

    json_t *result = sentience_build_vitals_json(hp, max_hp, mana, max_mana, move, max_move);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hp")),
                       json_integer_value(json_object_get(result, "hp")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hp_max")),
                       json_integer_value(json_object_get(result, "hp_max")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "mana")),
                       json_integer_value(json_object_get(result, "mana")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "mana_max")),
                       json_integer_value(json_object_get(result, "mana_max")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "move")),
                       json_integer_value(json_object_get(result, "move")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "move_max")),
                       json_integer_value(json_object_get(result, "move_max")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Stats scenario --- */

static test_result_t run_gmcp_stats_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_stats_json(
        (int)json_integer_value(json_object_get(p, "str")),
        (int)json_integer_value(json_object_get(p, "int")),
        (int)json_integer_value(json_object_get(p, "wis")),
        (int)json_integer_value(json_object_get(p, "dex")),
        (int)json_integer_value(json_object_get(p, "con")),
        (int)json_integer_value(json_object_get(p, "str_perm")),
        (int)json_integer_value(json_object_get(p, "int_perm")),
        (int)json_integer_value(json_object_get(p, "wis_perm")),
        (int)json_integer_value(json_object_get(p, "dex_perm")),
        (int)json_integer_value(json_object_get(p, "con_perm")),
        (int)json_integer_value(json_object_get(p, "hitroll")),
        (int)json_integer_value(json_object_get(p, "damroll")),
        (int)json_integer_value(json_object_get(p, "wimpy"))
    );
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "str")),
                       json_integer_value(json_object_get(result, "str")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "str_base")),
                       json_integer_value(json_object_get(result, "str_base")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "hitroll")),
                       json_integer_value(json_object_get(result, "hitroll")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Combat scenario --- */

static test_result_t run_gmcp_combat_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_combat_json(
        (int)json_integer_value(json_object_get(p, "ac_pierce")),
        (int)json_integer_value(json_object_get(p, "ac_bash")),
        (int)json_integer_value(json_object_get(p, "ac_slash")),
        (int)json_integer_value(json_object_get(p, "ac_exotic"))
    );
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "ac_pierce")),
                       json_integer_value(json_object_get(result, "ac_pierce")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Worth scenario --- */

static test_result_t run_gmcp_worth_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    json_t *result = sentience_build_worth_json(
        (int)json_integer_value(json_object_get(p, "alignment")),
        json_integer_value(json_object_get(p, "xp")),
        json_integer_value(json_object_get(p, "xp_tnl")),
        (int)json_integer_value(json_object_get(p, "practices")),
        json_integer_value(json_object_get(p, "gold"))
    );
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "alignment")),
                       json_integer_value(json_object_get(result, "alignment")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "xp")),
                       json_integer_value(json_object_get(result, "xp")));
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Identity scenario --- */

static test_result_t run_gmcp_identity_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    sentience_identity_input_t data = {0};
    data.name      = json_string_value(json_object_get(p, "name"));
    data.race_wnum = json_string_value(json_object_get(p, "race_wnum"));
    data.race_name = json_string_value(json_object_get(p, "race_name"));
    data.body_type = json_string_value(json_object_get(p, "body_type"));
    data.level     = (int)json_integer_value(json_object_get(p, "level"));
    data.tot_level = (int)json_integer_value(json_object_get(p, "tot_level"));
    data.title     = json_string_value(json_object_get(p, "title"));

    json_t *classes = json_object_get(p, "classes");
    if (classes && json_is_array(classes)) {
        data.num_classes = (int)json_array_size(classes);
        if (data.num_classes > SENTIENCE_MAX_CLASSES)
            data.num_classes = SENTIENCE_MAX_CLASSES;
        for (int i = 0; i < data.num_classes; i++) {
            json_t *cls = json_array_get(classes, i);
            data.classes[i].id         = json_string_value(json_object_get(cls, "id"));
            data.classes[i].name       = json_string_value(json_object_get(cls, "name"));
            data.classes[i].level      = (int)json_integer_value(json_object_get(cls, "level"));
            data.classes[i].is_primary = json_is_true(json_object_get(cls, "is_primary"));
        }
    }

    json_t *result = sentience_build_identity_json(&data);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "name")),
                       json_string_value(json_object_get(result, "name")));
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "race")),
                       json_string_value(json_object_get(result, "race")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "level")),
                       json_integer_value(json_object_get(result, "level")));

    json_t *result_classes = json_object_get(result, "classes");
    TEST_ASSERT_NOT_NULL(result_classes);
    TEST_ASSERT_TRUE(json_is_array(result_classes));

    json_t *expected_classes = json_object_get(expected, "classes");
    if (expected_classes) {
        TEST_ASSERT_INT_EQ((long)json_array_size(expected_classes),
                           (long)json_array_size(result_classes));
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Room scenario --- */

static test_result_t run_gmcp_room_scenario(json_t *tc)
{
    json_t *p = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!p || !expected) return TEST_ERROR;

    sentience_room_input_t data = {0};
    data.wnum      = json_string_value(json_object_get(p, "wnum"));
    data.name      = json_string_value(json_object_get(p, "name"));
    data.area_name = json_string_value(json_object_get(p, "area_name"));
    data.area_wnum = json_string_value(json_object_get(p, "area_wnum"));
    data.sector    = json_string_value(json_object_get(p, "sector"));
    data.is_wilds  = json_is_true(json_object_get(p, "is_wilds"));
    data.wilds_uid = (int)json_integer_value(json_object_get(p, "wilds_uid"));
    data.wilds_x   = (int)json_integer_value(json_object_get(p, "wilds_x"));
    data.wilds_y   = (int)json_integer_value(json_object_get(p, "wilds_y"));

    json_t *exits = json_object_get(p, "exits");
    if (exits && json_is_array(exits)) {
        data.num_exits = (int)json_array_size(exits);
        if (data.num_exits > 10) data.num_exits = 10;
        for (int i = 0; i < data.num_exits; i++) {
            json_t *ex = json_array_get(exits, i);
            data.exits[i].dir       = json_string_value(json_object_get(ex, "dir"));
            data.exits[i].wnum      = json_string_value(json_object_get(ex, "wnum"));
            data.exits[i].name      = json_string_value(json_object_get(ex, "name"));
            data.exits[i].is_door   = json_is_true(json_object_get(ex, "is_door"));
            data.exits[i].is_closed = json_is_true(json_object_get(ex, "is_closed"));
            data.exits[i].is_locked = json_is_true(json_object_get(ex, "is_locked"));
        }
    }

    json_t *result = sentience_build_room_json(&data);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "wnum")),
                       json_string_value(json_object_get(result, "wnum")));
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "name")),
                       json_string_value(json_object_get(result, "name")));

    json_t *result_exits = json_object_get(result, "exits");
    TEST_ASSERT_NOT_NULL(result_exits);
    TEST_ASSERT_TRUE(json_is_object(result_exits));

    if (data.is_wilds) {
        TEST_ASSERT_TRUE(json_is_true(json_object_get(result, "is_wilds")));
        TEST_ASSERT_INT_EQ(data.wilds_x,
                           (int)json_integer_value(json_object_get(result, "wilds_x")));
    }

    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    json_decref(result);
    return TEST_SUCCESS;
}

/*
 * Main test dispatcher — routes by function name from JSON config.
 */
test_result_t run_gmcp_sentience_test_case(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *func_name;
    size_t index;
    json_t *tc;

    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test missing configuration");
        return TEST_ERROR;
    }

    input = json_object_get(test->config, "input");
    if (!input) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test missing input");
        return TEST_ERROR;
    }

    func_name = test_json_get_string(input, "function");
    test_cases = json_object_get(input, "test_cases");
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "GMCP sentience test requires test_cases array");
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, tc) {
        const char *scenario = test_json_get_string(tc, "scenario");
        test_result_t result;

        if (!scenario) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "GMCP sentience test case missing scenario");
            return TEST_ERROR;
        }

        if (strcmp(func_name, "build_vitals") == 0) {
            result = run_gmcp_vitals_scenario(tc);
        } else if (strcmp(func_name, "build_stats") == 0) {
            result = run_gmcp_stats_scenario(tc);
        } else if (strcmp(func_name, "build_combat") == 0) {
            result = run_gmcp_combat_scenario(tc);
        } else if (strcmp(func_name, "build_worth") == 0) {
            result = run_gmcp_worth_scenario(tc);
        } else if (strcmp(func_name, "build_identity") == 0) {
            result = run_gmcp_identity_scenario(tc);
        } else if (strcmp(func_name, "build_room") == 0) {
            result = run_gmcp_room_scenario(tc);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Unknown GMCP function: %s", func_name);
            return TEST_ERROR;
        }

        if (result != TEST_SUCCESS)
            return result;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
