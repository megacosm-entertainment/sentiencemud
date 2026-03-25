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
