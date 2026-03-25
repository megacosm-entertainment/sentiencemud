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
static test_result_t run_gmcp_client_ready_capabilities_scenario(json_t *tc);
static test_result_t run_gmcp_client_ready_state_scenario(json_t *tc);
static test_result_t run_gmcp_affects_scenario(json_t *tc);
static test_result_t run_gmcp_enemies_scenario(json_t *tc);
static test_result_t run_gmcp_room_contents_scenario(json_t *tc);
static test_result_t run_gmcp_room_map_scenario(json_t *tc);
static test_result_t run_gmcp_channel_message_scenario(json_t *tc);
static test_result_t run_gmcp_layout_name_validation_scenario(json_t *tc);
static test_result_t run_gmcp_layout_storage_scenario(json_t *tc);
static test_result_t run_gmcp_auth_qrcode_scenario(json_t *tc);
static test_result_t run_gmcp_preferences_scenario(json_t *tc);

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

/* --- Client.Ready.Capabilities scenario --- */

static test_result_t run_gmcp_client_ready_capabilities_scenario(json_t *tc)
{
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    json_t *exp_pkgs, *res_pkgs, *exp_feats, *res_feats;
    size_t i;

    if (!expected) return TEST_ERROR;

    result = sentience_build_client_ready_capabilities_json();
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    exp_pkgs = json_object_get(expected, "packages");
    res_pkgs = json_object_get(result, "packages");
    TEST_ASSERT_NOT_NULL(res_pkgs);
    TEST_ASSERT_INT_EQ(json_array_size(exp_pkgs), json_array_size(res_pkgs));

    for (i = 0; i < json_array_size(exp_pkgs); i++) {
        const char *exp_str = json_string_value(json_array_get(exp_pkgs, i));
        const char *res_str = json_string_value(json_array_get(res_pkgs, i));
        TEST_ASSERT_STR_EQ(exp_str, res_str);
    }

    exp_feats = json_object_get(expected, "features");
    res_feats = json_object_get(result, "features");
    TEST_ASSERT_NOT_NULL(res_feats);
    TEST_ASSERT_INT_EQ(json_array_size(exp_feats), json_array_size(res_feats));

    for (i = 0; i < json_array_size(exp_feats); i++) {
        const char *exp_str = json_string_value(json_array_get(exp_feats, i));
        const char *res_str = json_string_value(json_array_get(res_feats, i));
        TEST_ASSERT_STR_EQ(exp_str, res_str);
    }

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Client.Ready.State scenario --- */

static test_result_t run_gmcp_client_ready_state_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;

    if (!params || !expected) return TEST_ERROR;

    int tick_rate = (int)json_integer_value(json_object_get(params, "tick_rate"));
    int pps = (int)json_integer_value(json_object_get(params, "pulse_per_second"));

    result = sentience_build_client_ready_state_json(tick_rate, pps);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "tick_rate")),
                       json_integer_value(json_object_get(result, "tick_rate")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "pulse_per_second")),
                       json_integer_value(json_object_get(result, "pulse_per_second")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Char.Affects scenario --- */

static test_result_t run_gmcp_affects_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result, *aff_arr, *exp_arr;
    sentience_affect_input_t inputs[32];
    int num_affects = 0;
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    aff_arr = json_object_get(params, "affects");
    if (aff_arr && json_is_array(aff_arr)) {
        json_t *af;
        json_array_foreach(aff_arr, i, af) {
            if (num_affects >= 32) break;
            inputs[num_affects].name = test_json_get_string(af, "name");
            inputs[num_affects].wnum = test_json_get_string(af, "wnum");
            inputs[num_affects].duration = (int)json_integer_value(json_object_get(af, "duration"));
            inputs[num_affects].estimated_seconds = (int)json_integer_value(json_object_get(af, "estimated_seconds"));
            inputs[num_affects].modifier = test_json_get_string(af, "modifier");
            inputs[num_affects].level = (int)json_integer_value(json_object_get(af, "level"));
            num_affects++;
        }
    }

    result = sentience_build_affects_json(inputs, num_affects);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    exp_arr = json_object_get(expected, "affects");
    json_t *res_arr = json_object_get(result, "affects");
    TEST_ASSERT_NOT_NULL(res_arr);
    TEST_ASSERT_INT_EQ(json_array_size(exp_arr), json_array_size(res_arr));

    for (i = 0; i < json_array_size(exp_arr); i++) {
        json_t *exp_af = json_array_get(exp_arr, i);
        json_t *res_af = json_array_get(res_arr, i);

        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_af, "name")),
                           json_string_value(json_object_get(res_af, "name")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "duration")),
                           json_integer_value(json_object_get(res_af, "duration")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "estimated_seconds")),
                           json_integer_value(json_object_get(res_af, "estimated_seconds")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "level")),
                           json_integer_value(json_object_get(res_af, "level")));

        if (json_is_null(json_object_get(exp_af, "wnum"))) {
            TEST_ASSERT_TRUE(json_is_null(json_object_get(res_af, "wnum")));
        } else {
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_af, "wnum")),
                               json_string_value(json_object_get(res_af, "wnum")));
        }

        if (json_is_null(json_object_get(exp_af, "modifier"))) {
            TEST_ASSERT_TRUE(json_is_null(json_object_get(res_af, "modifier")));
        } else {
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_af, "modifier")),
                               json_string_value(json_object_get(res_af, "modifier")));
        }
    }

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Char.Enemies scenario --- */

static test_result_t run_gmcp_enemies_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result, *en_arr, *exp_arr;
    sentience_enemy_input_t inputs[32];
    int num_enemies = 0;
    long self_hp, self_max_hp;
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    self_hp = json_integer_value(json_object_get(params, "self_hp"));
    self_max_hp = json_integer_value(json_object_get(params, "self_max_hp"));

    en_arr = json_object_get(params, "enemies");
    if (en_arr && json_is_array(en_arr)) {
        json_t *en;
        json_array_foreach(en_arr, i, en) {
            json_t *iid;
            if (num_enemies >= 32) break;
            inputs[num_enemies].name = test_json_get_string(en, "name");
            iid = json_object_get(en, "instance_id");
            inputs[num_enemies].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            inputs[num_enemies].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            inputs[num_enemies].hp_pct = (int)json_integer_value(json_object_get(en, "hp_pct"));
            inputs[num_enemies].is_primary = json_is_true(json_object_get(en, "is_primary"));
            inputs[num_enemies].target = test_json_get_string(en, "target");
            num_enemies++;
        }
    }

    result = sentience_build_enemies_json(inputs, num_enemies, self_hp, self_max_hp);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    exp_arr = json_object_get(expected, "enemies");
    json_t *res_arr = json_object_get(result, "enemies");
    if (exp_arr) {
        TEST_ASSERT_NOT_NULL(res_arr);
        TEST_ASSERT_INT_EQ(json_array_size(exp_arr), json_array_size(res_arr));

        for (i = 0; i < json_array_size(exp_arr); i++) {
            json_t *exp_en = json_array_get(exp_arr, i);
            json_t *res_en = json_array_get(res_arr, i);

            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_en, "name")),
                               json_string_value(json_object_get(res_en, "name")));
            TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_en, "hp_pct")),
                               json_integer_value(json_object_get(res_en, "hp_pct")));
            TEST_ASSERT_TRUE(json_is_true(json_object_get(exp_en, "is_primary"))
                             == json_is_true(json_object_get(res_en, "is_primary")));
        }
    }

    json_t *exp_self = json_object_get(expected, "self");
    json_t *res_self = json_object_get(result, "self");
    if (json_is_null(exp_self)) {
        TEST_ASSERT_TRUE(json_is_null(res_self));
    } else if (exp_self) {
        TEST_ASSERT_NOT_NULL(res_self);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "hp")),
                           json_integer_value(json_object_get(res_self, "hp")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "max_hp")),
                           json_integer_value(json_object_get(res_self, "max_hp")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "hp_pct")),
                           json_integer_value(json_object_get(res_self, "hp_pct")));
    }

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Room.Contents scenario --- */

static test_result_t run_gmcp_room_contents_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_room_entity_input_t items[32], npcs[32], players[32];
    sentience_room_door_input_t doors[10];
    sentience_room_contents_input_t data = {0};
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    json_t *j_items = json_object_get(params, "items");
    if (j_items && json_is_array(j_items)) {
        json_t *it;
        json_array_foreach(j_items, i, it) {
            json_t *iid;
            if (data.num_items >= 32) break;
            items[data.num_items].name = test_json_get_string(it, "name");
            iid = json_object_get(it, "instance_id");
            items[data.num_items].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            items[data.num_items].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            items[data.num_items].short_desc = test_json_get_string(it, "short_desc");
            data.num_items++;
        }
    }
    data.items = items;

    json_t *j_npcs = json_object_get(params, "npcs");
    if (j_npcs && json_is_array(j_npcs)) {
        json_t *npc;
        json_array_foreach(j_npcs, i, npc) {
            json_t *iid;
            if (data.num_npcs >= 32) break;
            npcs[data.num_npcs].name = test_json_get_string(npc, "name");
            iid = json_object_get(npc, "instance_id");
            npcs[data.num_npcs].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            npcs[data.num_npcs].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            npcs[data.num_npcs].short_desc = test_json_get_string(npc, "short_desc");
            data.num_npcs++;
        }
    }
    data.npcs = npcs;

    json_t *j_players = json_object_get(params, "players");
    if (j_players && json_is_array(j_players)) {
        json_t *pl;
        json_array_foreach(j_players, i, pl) {
            json_t *iid;
            if (data.num_players >= 32) break;
            players[data.num_players].name = test_json_get_string(pl, "name");
            iid = json_object_get(pl, "instance_id");
            if (iid) {
                players[data.num_players].instance_id[0] = json_integer_value(json_array_get(iid, 0));
                players[data.num_players].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            }
            players[data.num_players].short_desc = NULL;
            data.num_players++;
        }
    }
    data.players = players;

    json_t *j_doors = json_object_get(params, "doors");
    if (j_doors && json_is_array(j_doors)) {
        json_t *dr;
        json_array_foreach(j_doors, i, dr) {
            if (data.num_doors >= 10) break;
            doors[data.num_doors].direction = test_json_get_string(dr, "direction");
            doors[data.num_doors].state = test_json_get_string(dr, "state");
            doors[data.num_doors].is_locked = json_is_true(json_object_get(dr, "is_locked"));
            data.num_doors++;
        }
    }
    data.doors = doors;

    result = sentience_build_room_contents_json(&data);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    const char *categories[] = {"items", "npcs", "players", "doors"};
    int c;
    for (c = 0; c < 4; c++) {
        json_t *exp_cat = json_object_get(expected, categories[c]);
        json_t *res_cat = json_object_get(result, categories[c]);
        if (!exp_cat) continue;

        TEST_ASSERT_NOT_NULL(res_cat);
        TEST_ASSERT_INT_EQ(json_array_size(exp_cat), json_array_size(res_cat));

        for (i = 0; i < json_array_size(exp_cat); i++) {
            json_t *exp_ent = json_array_get(exp_cat, i);
            json_t *res_ent = json_array_get(res_cat, i);

            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_ent, "name")),
                               json_string_value(json_object_get(res_ent, "name")));

            if (strcmp(categories[c], "doors") == 0) {
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "direction")),
                    json_string_value(json_object_get(res_ent, "direction")));
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "state")),
                    json_string_value(json_object_get(res_ent, "state")));
                TEST_ASSERT_TRUE(json_is_true(json_object_get(exp_ent, "is_locked"))
                                 == json_is_true(json_object_get(res_ent, "is_locked")));
            }

            if (strcmp(categories[c], "items") == 0
                || strcmp(categories[c], "npcs") == 0) {
                json_t *eid = json_object_get(exp_ent, "instance_id");
                json_t *rid = json_object_get(res_ent, "instance_id");
                TEST_ASSERT_NOT_NULL(rid);
                TEST_ASSERT_INT_EQ(json_integer_value(json_array_get(eid, 0)),
                                   json_integer_value(json_array_get(rid, 0)));
                TEST_ASSERT_INT_EQ(json_integer_value(json_array_get(eid, 1)),
                                   json_integer_value(json_array_get(rid, 1)));
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "short_desc")),
                    json_string_value(json_object_get(res_ent, "short_desc")));
            }
        }
    }

    json_decref(result);
    return TEST_SUCCESS;
}

static test_result_t run_gmcp_room_map_scenario(json_t *test_case)
{
    json_t *params = json_object_get(test_case, "params");
    json_t *expected = json_object_get(test_case, "expected");

    sentience_room_map_input_t input = {
        .type     = json_string_value(json_object_get(params, "type")),
        .map_text = json_string_value(json_object_get(params, "map_text")),
        .width    = json_integer_value(json_object_get(params, "width")),
        .height   = json_integer_value(json_object_get(params, "height")),
    };

    json_t *result = sentience_build_room_map(&input);
    if (!result) {
        if (!expected || json_is_null(expected))
            return TEST_SUCCESS;
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "sentience_build_room_map returned NULL");
        return TEST_FAILURE;
    }

    test_result_t tr = TEST_SUCCESS;

    if (json_object_get(expected, "_v")) {
        int exp_v = json_integer_value(json_object_get(expected, "_v"));
        int got_v = json_integer_value(json_object_get(result, "_v"));
        if (exp_v != got_v) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "_v: expected %d, got %d", exp_v, got_v);
            tr = TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "type")) {
        const char *exp = json_string_value(json_object_get(expected, "type"));
        const char *got = json_string_value(json_object_get(result, "type"));
        if (!exp || !got || strcmp(exp, got) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "type: expected '%s', got '%s'", exp ? exp : "NULL", got ? got : "NULL");
            tr = TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "width")) {
        int exp_w = json_integer_value(json_object_get(expected, "width"));
        int got_w = json_integer_value(json_object_get(result, "width"));
        if (exp_w != got_w) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "width: expected %d, got %d", exp_w, got_w);
            tr = TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "height")) {
        int exp_h = json_integer_value(json_object_get(expected, "height"));
        int got_h = json_integer_value(json_object_get(result, "height"));
        if (exp_h != got_h) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "height: expected %d, got %d", exp_h, got_h);
            tr = TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "map_text")) {
        const char *exp = json_string_value(json_object_get(expected, "map_text"));
        const char *got = json_string_value(json_object_get(result, "map_text"));
        if (!exp || !got || strcmp(exp, got) != 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "map_text mismatch");
            tr = TEST_FAILURE;
        }
    }

    json_decref(result);
    return tr;
}

/* --- Channel.Message scenario --- */

static test_result_t run_gmcp_channel_message_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *tell_target_val = json_object_get(params, "tell_target");

    sentience_channel_message_input_t input = {
        .channel     = json_string_value(json_object_get(params, "channel")),
        .sender      = json_string_value(json_object_get(params, "sender")),
        .text        = json_string_value(json_object_get(params, "text")),
        .timestamp   = (long)json_integer_value(json_object_get(params, "timestamp")),
        .tell_target = (tell_target_val && !json_is_null(tell_target_val))
                     ? json_string_value(tell_target_val)
                     : NULL,
    };

    json_t *result = sentience_build_channel_message(&input);
    if (!result) {
        if (!expected || json_is_null(expected))
            return TEST_SUCCESS;
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "sentience_build_channel_message returned NULL");
        return TEST_FAILURE;
    }

    bool pass = json_equal(result, expected);
    if (!pass) {
        char *r = json_dumps(result, JSON_COMPACT | JSON_SORT_KEYS);
        char *e = json_dumps(expected, JSON_COMPACT | JSON_SORT_KEYS);
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Channel.Message JSON mismatch:\n  got:    %s\n  expect: %s",
                     r ? r : "NULL", e ? e : "NULL");
        free(r); free(e);
    }

    json_decref(result);
    return pass ? TEST_SUCCESS : TEST_FAILURE;
}

/* --- Layout name validation scenario --- */

static test_result_t run_gmcp_layout_name_validation_scenario(json_t *tc)
{
    const char *name = test_json_get_string(tc, "name");
    bool expected_valid = json_is_true(json_object_get(tc, "valid"));
    bool actual = test_layout_name_is_valid(name);

    if (actual != expected_valid) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
            "layout_name_is_valid('%s') = %s, expected %s",
            name ? name : "(null)",
            actual ? "true" : "false",
            expected_valid ? "true" : "false");
        return TEST_FAILURE;
    }
    return TEST_SUCCESS;
}

/* --- Layout storage helpers scenario --- */

static test_result_t run_gmcp_layout_storage_scenario(json_t *tc)
{
    web_client_layout_t *list = NULL;

    /* Initially empty */
    TEST_ASSERT_INT_EQ(0, layout_count(list));
    TEST_ASSERT_NULL(layout_find(list, "default"));

    /* Add one entry */
    web_client_layout_t *e1 = calloc(1, sizeof(*e1));
    snprintf(e1->name, sizeof(e1->name), "default");
    e1->layout = json_object();
    json_object_set_new(e1->layout, "test", json_true());
    e1->next = list;
    list = e1;

    TEST_ASSERT_INT_EQ(1, layout_count(list));
    TEST_ASSERT_NOT_NULL(layout_find(list, "default"));
    TEST_ASSERT_NULL(layout_find(list, "other"));

    /* Add second entry */
    web_client_layout_t *e2 = calloc(1, sizeof(*e2));
    snprintf(e2->name, sizeof(e2->name), "compact");
    e2->layout = json_object();
    e2->next = list;
    list = e2;

    TEST_ASSERT_INT_EQ(2, layout_count(list));
    TEST_ASSERT_NOT_NULL(layout_find(list, "compact"));
    TEST_ASSERT_NOT_NULL(layout_find(list, "default"));

    /* Free all */
    layout_free_all(&list);
    TEST_ASSERT_NULL(list);
    TEST_ASSERT_INT_EQ(0, layout_count(list));

    return TEST_SUCCESS;
}

/* --- Auth.QRCode builder scenario --- */

static test_result_t run_gmcp_auth_qrcode_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    const char *purpose = test_json_get_string(params, "purpose");
    const char *image = test_json_get_string(params, "image");
    const char *uri = test_json_get_string(params, "uri");
    long expires_at = json_integer_value(json_object_get(params, "expires_at"));

    json_t *result = sentience_build_auth_qrcode_json(purpose, image, uri, expires_at);
    if (!result) return TEST_FAILURE;

    test_result_t status = json_equal(result, expected) ? TEST_SUCCESS : TEST_FAILURE;
    if (status != TEST_SUCCESS) {
        char *exp_str = json_dumps(expected, JSON_COMPACT | JSON_SORT_KEYS);
        char *got_str = json_dumps(result, JSON_COMPACT | JSON_SORT_KEYS);
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "QRCode mismatch. Expected: %s Got: %s", exp_str, got_str);
        free(exp_str);
        free(got_str);
    }

    json_decref(result);
    return status;
}

/* --- Preferences scenario --- */

static test_result_t run_gmcp_preferences_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    if (!params || !expected) return TEST_ERROR;

    json_t *preferences_array = json_object_get(params, "preferences");
    if (!preferences_array || !json_is_array(preferences_array)) return TEST_ERROR;

    /* Populate the input struct from the JSON array */
    sentience_preferences_input_t input = {0};
    input.num_prefs = json_array_size(preferences_array);

    if (input.num_prefs > SENTIENCE_MAX_PREFERENCES) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Too many preferences: %d", input.num_prefs);
        return TEST_ERROR;
    }

    for (int i = 0; i < input.num_prefs; i++) {
        json_t *pref = json_array_get(preferences_array, i);
        sentience_pref_entry_t *entry = &input.prefs[i];

        entry->key = json_string_value(json_object_get(pref, "key"));
        entry->category = json_string_value(json_object_get(pref, "category"));
        entry->type = json_string_value(json_object_get(pref, "type"));
        entry->source = json_string_value(json_object_get(pref, "source"));
        entry->label = json_string_value(json_object_get(pref, "label"));
        entry->value_bool = json_boolean_value(json_object_get(pref, "value_bool"));
        entry->value_int = json_integer_value(json_object_get(pref, "value_int"));
        entry->value_string = json_string_value(json_object_get(pref, "value_string"));
    }

    /* Call the builder */
    json_t *result = sentience_build_preferences_json(&input);
    if (!result) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                   "sentience_build_preferences_json returned NULL");
        return TEST_FAILURE;
    }

    /* Check expected fields */
    if (json_object_get(expected, "_v")) {
        int exp_v = json_integer_value(json_object_get(expected, "_v"));
        int got_v = json_integer_value(json_object_get(result, "_v"));
        if (exp_v != got_v) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "_v: expected %d, got %d", exp_v, got_v);
            json_decref(result);
            return TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "preferences_count")) {
        json_t *prefs_array = json_object_get(result, "preferences");
        if (!prefs_array || !json_is_array(prefs_array)) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                       "Missing or invalid preferences array");
            json_decref(result);
            return TEST_FAILURE;
        }
        int exp_count = json_integer_value(json_object_get(expected, "preferences_count"));
        int got_count = json_array_size(prefs_array);
        if (exp_count != got_count) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "preferences count: expected %d, got %d", exp_count, got_count);
            json_decref(result);
            return TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "has_key_brief")) {
        json_t *prefs_array = json_object_get(result, "preferences");
        bool found_brief = false;
        for (size_t i = 0; i < json_array_size(prefs_array); i++) {
            json_t *pref = json_array_get(prefs_array, i);
            const char *key = json_string_value(json_object_get(pref, "key"));
            if (key && !strcmp(key, "brief")) {
                found_brief = true;
                /* Check specific values for brief if specified */
                if (json_object_get(expected, "brief_source")) {
                    const char *exp_source = json_string_value(json_object_get(expected, "brief_source"));
                    const char *got_source = json_string_value(json_object_get(pref, "source"));
                    if (!got_source || strcmp(exp_source, got_source)) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                     "brief source: expected '%s', got '%s'", 
                                     exp_source, got_source ? got_source : "NULL");
                        json_decref(result);
                        return TEST_FAILURE;
                    }
                }
                if (json_object_get(expected, "brief_value")) {
                    bool exp_value = json_boolean_value(json_object_get(expected, "brief_value"));
                    bool got_value = json_boolean_value(json_object_get(pref, "value"));
                    if (exp_value != got_value) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                     "brief value: expected %s, got %s", 
                                     exp_value ? "true" : "false",
                                     got_value ? "true" : "false");
                        json_decref(result);
                        return TEST_FAILURE;
                    }
                }
                break;
            }
        }
        if (!found_brief) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Expected key 'brief' not found");
            json_decref(result);
            return TEST_FAILURE;
        }
    }

    if (json_object_get(expected, "compact_source")) {
        json_t *prefs_array = json_object_get(result, "preferences");
        bool found_compact = false;
        for (size_t i = 0; i < json_array_size(prefs_array); i++) {
            json_t *pref = json_array_get(prefs_array, i);
            const char *key = json_string_value(json_object_get(pref, "key"));
            if (key && !strcmp(key, "compact")) {
                found_compact = true;
                const char *exp_source = json_string_value(json_object_get(expected, "compact_source"));
                const char *got_source = json_string_value(json_object_get(pref, "source"));
                if (!got_source || strcmp(exp_source, got_source)) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "compact source: expected '%s', got '%s'", 
                                 exp_source, got_source ? got_source : "NULL");
                    json_decref(result);
                    return TEST_FAILURE;
                }
                break;
            }
        }
        if (!found_compact) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Expected key 'compact' not found");
            json_decref(result);
            return TEST_FAILURE;
        }
    }

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
        } else if (strcmp(func_name, "build_client_ready_capabilities") == 0) {
            result = run_gmcp_client_ready_capabilities_scenario(tc);
        } else if (strcmp(func_name, "build_client_ready_state") == 0) {
            result = run_gmcp_client_ready_state_scenario(tc);
        } else if (strcmp(func_name, "build_affects") == 0) {
            result = run_gmcp_affects_scenario(tc);
        } else if (strcmp(func_name, "build_enemies") == 0) {
            result = run_gmcp_enemies_scenario(tc);
        } else if (strcmp(func_name, "build_room_contents") == 0) {
            result = run_gmcp_room_contents_scenario(tc);
        } else if (strcmp(func_name, "build_room_map") == 0) {
            result = run_gmcp_room_map_scenario(tc);
        } else if (strcmp(func_name, "build_channel_message") == 0) {
            result = run_gmcp_channel_message_scenario(tc);
        } else if (strcmp(func_name, "layout_name_validation") == 0) {
            result = run_gmcp_layout_name_validation_scenario(tc);
        } else if (strcmp(func_name, "layout_storage_helpers") == 0) {
            result = run_gmcp_layout_storage_scenario(tc);
        } else if (strcmp(func_name, "build_auth_qrcode") == 0) {
            result = run_gmcp_auth_qrcode_scenario(tc);
        } else if (strcmp(func_name, "build_preferences") == 0) {
            result = run_gmcp_preferences_scenario(tc);
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
