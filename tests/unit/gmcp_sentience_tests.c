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
static test_result_t run_gmcp_identity_extended_scenario(json_t *tc);
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
static test_result_t run_gmcp_inventory_scenario(json_t *tc);
static test_result_t run_gmcp_equipment_scenario(json_t *tc);
static test_result_t run_gmcp_abilities_scenario(json_t *tc);
static test_result_t run_gmcp_reputations_scenario(json_t *tc);

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

/* --- Extended Identity scenario --- */

static test_result_t run_gmcp_identity_extended_scenario(json_t *tc)
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

    /* Extended: Classes with full metadata */
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
            
            /* Extended class fields */
            data.classes[i].max_level     = (int)json_integer_value(json_object_get(cls, "max_level"));
            data.classes[i].type          = json_string_value(json_object_get(cls, "type"));
            data.classes[i].flags         = json_string_value(json_object_get(cls, "flags"));
            data.classes[i].primary_stat  = json_string_value(json_object_get(cls, "primary_stat"));
            data.classes[i].hp_min        = (int)json_integer_value(json_object_get(cls, "hp_min"));
            data.classes[i].hp_max        = (int)json_integer_value(json_object_get(cls, "hp_max"));
            data.classes[i].gains_mana    = json_is_true(json_object_get(cls, "gains_mana"));
            data.classes[i].description   = json_string_value(json_object_get(cls, "description"));
            data.classes[i].xp            = json_integer_value(json_object_get(cls, "xp"));
            data.classes[i].active_title  = json_string_value(json_object_get(cls, "active_title"));
            data.classes[i].action_label  = json_string_value(json_object_get(cls, "action_label"));
            data.classes[i].action_cmd    = json_string_value(json_object_get(cls, "action_cmd"));

            /* Class titles */
            json_t *titles = json_object_get(cls, "titles");
            if (titles && json_is_array(titles)) {
                data.classes[i].num_titles = (int)json_array_size(titles);
                if (data.classes[i].num_titles > SENTIENCE_MAX_TITLES)
                    data.classes[i].num_titles = SENTIENCE_MAX_TITLES;
                for (int j = 0; j < data.classes[i].num_titles; j++) {
                    json_t *title = json_array_get(titles, j);
                    data.classes[i].titles[j].keyword    = json_string_value(json_object_get(title, "keyword"));
                    data.classes[i].titles[j].display    = json_string_value(json_object_get(title, "display"));
                    data.classes[i].titles[j].is_default = json_is_true(json_object_get(title, "is_default"));
                }
            }
        }
    }

    /* Extended: Traits */
    json_t *traits = json_object_get(p, "traits");
    if (traits && json_is_array(traits)) {
        data.num_traits = (int)json_array_size(traits);
        if (data.num_traits > SENTIENCE_MAX_TRAITS)
            data.num_traits = SENTIENCE_MAX_TRAITS;
        for (int i = 0; i < data.num_traits; i++) {
            json_t *trait = json_array_get(traits, i);
            data.traits[i].id          = json_string_value(json_object_get(trait, "id"));
            data.traits[i].name        = json_string_value(json_object_get(trait, "name"));
            data.traits[i].description = json_string_value(json_object_get(trait, "description"));
            data.traits[i].category    = json_string_value(json_object_get(trait, "category"));
            data.traits[i].type        = json_string_value(json_object_get(trait, "type"));
            data.traits[i].source      = json_string_value(json_object_get(trait, "source"));
            data.traits[i].value_bool  = json_is_true(json_object_get(trait, "value_bool"));
            data.traits[i].value_int   = (int)json_integer_value(json_object_get(trait, "value_int"));
            data.traits[i].value_string = json_string_value(json_object_get(trait, "value_string"));
        }
    }

    /* Extended: Race info */
    json_t *race_info = json_object_get(p, "race_info");
    if (race_info) {
        data.race_info.id           = json_string_value(json_object_get(race_info, "id"));
        data.race_info.name         = json_string_value(json_object_get(race_info, "name"));
        data.race_info.description  = json_string_value(json_object_get(race_info, "description"));
        data.race_info.playable     = json_is_true(json_object_get(race_info, "playable"));
        data.race_info.starting     = json_is_true(json_object_get(race_info, "starting"));
        data.race_info.size         = json_string_value(json_object_get(race_info, "size"));
        data.race_info.resistances  = json_string_value(json_object_get(race_info, "resistances"));
        data.race_info.vulnerabilities = json_string_value(json_object_get(race_info, "vulnerabilities"));
        data.race_info.immunities   = json_string_value(json_object_get(race_info, "immunities"));
        data.race_info.affects      = json_string_value(json_object_get(race_info, "affects"));
        data.race_info.remort_into  = json_string_value(json_object_get(race_info, "remort_into"));

        /* Stats arrays */
        json_t *stats = json_object_get(race_info, "stats");
        if (stats && json_is_array(stats)) {
            for (int i = 0; i < 5 && i < (int)json_array_size(stats); i++) {
                data.race_info.stats[i] = (int)json_integer_value(json_array_get(stats, i));
            }
        }
        json_t *max_stats = json_object_get(race_info, "max_stats");
        if (max_stats && json_is_array(max_stats)) {
            for (int i = 0; i < 5 && i < (int)json_array_size(max_stats); i++) {
                data.race_info.max_stats[i] = (int)json_integer_value(json_array_get(max_stats, i));
            }
        }
        json_t *max_vitals = json_object_get(race_info, "max_vitals");
        if (max_vitals && json_is_array(max_vitals)) {
            for (int i = 0; i < 3 && i < (int)json_array_size(max_vitals); i++) {
                data.race_info.max_vitals[i] = (int)json_integer_value(json_array_get(max_vitals, i));
            }
        }

        /* Skills */
        json_t *skills = json_object_get(race_info, "skills");
        if (skills && json_is_array(skills)) {
            data.race_info.num_skills = (int)json_array_size(skills);
            if (data.race_info.num_skills > SENTIENCE_MAX_RACE_SKILLS)
                data.race_info.num_skills = SENTIENCE_MAX_RACE_SKILLS;
            for (int i = 0; i < data.race_info.num_skills; i++) {
                data.race_info.skills[i] = json_string_value(json_array_get(skills, i));
            }
        }

        /* Race traits - Note: for simplicity, keeping empty as per test data */
        data.race_info.num_traits = 0;
    }

    json_t *result = sentience_build_identity_json(&data);
    TEST_ASSERT_NOT_NULL(result);

    /* Test the extended fields */
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "_v")),
                       json_integer_value(json_object_get(result, "_v")));
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "name")),
                       json_string_value(json_object_get(result, "name")));

    json_t *result_classes = json_object_get(result, "classes");
    TEST_ASSERT_NOT_NULL(result_classes);
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "classes_count")),
                       (long)json_array_size(result_classes));

    if (json_array_size(result_classes) > 0) {
        json_t *first_class = json_array_get(result_classes, 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_class_max_level")),
                           json_integer_value(json_object_get(first_class, "max_level")));
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_class_type")),
                           json_string_value(json_object_get(first_class, "type")));
        TEST_ASSERT_INT_EQ(json_boolean_value(json_object_get(expected, "first_class_gains_mana")) ? 1 : 0,
                           json_boolean_value(json_object_get(first_class, "gains_mana")) ? 1 : 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_class_xp")),
                           json_integer_value(json_object_get(first_class, "xp")));

        json_t *titles_arr = json_object_get(first_class, "available_titles");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_class_titles_count")),
                           (long)json_array_size(titles_arr));

        json_t *action = json_object_get(first_class, "action");
        TEST_ASSERT_INT_EQ(json_boolean_value(json_object_get(expected, "first_class_action_null")) ? 1 : 0,
                           json_is_null(action) ? 1 : 0);
    }

    if (json_array_size(result_classes) > 1) {
        json_t *second_class = json_array_get(result_classes, 1);
        json_t *action = json_object_get(second_class, "action");
        if (!json_is_null(action)) {
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "second_class_action_label")),
                               json_string_value(json_object_get(action, "label")));
        }
    }

    json_t *result_traits = json_object_get(result, "traits");
    TEST_ASSERT_NOT_NULL(result_traits);
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "traits_count")),
                       (long)json_array_size(result_traits));

    if (json_array_size(result_traits) > 0) {
        json_t *first_trait = json_array_get(result_traits, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_trait_id")),
                           json_string_value(json_object_get(first_trait, "id")));
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_trait_source")),
                           json_string_value(json_object_get(first_trait, "source")));
    }

    if (json_array_size(result_traits) > 1) {
        json_t *second_trait = json_array_get(result_traits, 1);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "second_trait_value_int")),
                           json_integer_value(json_object_get(second_trait, "value")));
    }

    json_t *result_race_info = json_object_get(result, "race_info");
    TEST_ASSERT_NOT_NULL(result_race_info);
    TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "race_info_id")),
                       json_string_value(json_object_get(result_race_info, "id")));
    TEST_ASSERT_INT_EQ(json_boolean_value(json_object_get(expected, "race_info_playable")) ? 1 : 0,
                       json_boolean_value(json_object_get(result_race_info, "playable")) ? 1 : 0);

    json_t *skills_arr = json_object_get(result_race_info, "skills");
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "race_info_skills_count")),
                       (long)json_array_size(skills_arr));

    json_t *stats_obj = json_object_get(result_race_info, "stats");
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "race_info_stats_str")),
                       json_integer_value(json_object_get(stats_obj, "str")));

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
        } else if (strcmp(func_name, "build_identity_extended") == 0) {
            result = run_gmcp_identity_extended_scenario(tc);
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
        } else if (strcmp(func_name, "build_inventory") == 0) {
            result = run_gmcp_inventory_scenario(tc);
        } else if (strcmp(func_name, "build_equipment") == 0) {
            result = run_gmcp_equipment_scenario(tc);
        } else if (strcmp(func_name, "build_abilities") == 0) {
            result = run_gmcp_abilities_scenario(tc);
        } else if (strcmp(func_name, "build_reputations") == 0) {
            result = run_gmcp_reputations_scenario(tc);
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

/* --- Inventory scenario --- */

static test_result_t run_gmcp_inventory_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_inventory_input_t input = {0};
    size_t i, j;

    if (!params || !expected) return TEST_ERROR;

    /* Parse items array */
    json_t *items_arr = json_object_get(params, "items");
    if (items_arr && json_is_array(items_arr)) {
        input.num_items = (int)json_array_size(items_arr);
        if (input.num_items > SENTIENCE_MAX_INVENTORY)
            input.num_items = SENTIENCE_MAX_INVENTORY;

        for (i = 0; i < (size_t)input.num_items; i++) {
            json_t *item = json_array_get(items_arr, i);
            sentience_inventory_item_t *inv_item = &input.items[i];

            inv_item->name = json_string_value(json_object_get(item, "name"));
            inv_item->keywords = json_string_value(json_object_get(item, "keywords"));
            inv_item->keyword = json_string_value(json_object_get(item, "keyword"));
            inv_item->item_type = json_string_value(json_object_get(item, "item_type"));
            inv_item->condition = (int)json_integer_value(json_object_get(item, "condition"));
            inv_item->condition_label = json_string_value(json_object_get(item, "condition_label"));
            inv_item->level = (int)json_integer_value(json_object_get(item, "level"));
            inv_item->weight = (int)json_integer_value(json_object_get(item, "weight"));
            inv_item->item_count = (int)json_integer_value(json_object_get(item, "item_count"));

            /* Parse ID array */
            json_t *id_arr = json_object_get(item, "id");
            if (id_arr && json_is_array(id_arr)) {
                inv_item->id[0] = (unsigned long)json_integer_value(json_array_get(id_arr, 0));
                inv_item->id[1] = (unsigned long)json_integer_value(json_array_get(id_arr, 1));
            }

            /* Parse flags array */
            json_t *flags_arr = json_object_get(item, "flags");
            if (flags_arr && json_is_array(flags_arr)) {
                inv_item->num_flags = (int)json_array_size(flags_arr);
                if (inv_item->num_flags > SENTIENCE_MAX_ITEM_FLAGS)
                    inv_item->num_flags = SENTIENCE_MAX_ITEM_FLAGS;
                for (j = 0; j < (size_t)inv_item->num_flags; j++) {
                    inv_item->flags[j] = json_string_value(json_array_get(flags_arr, j));
                }
            }

            /* Parse actions array */
            json_t *actions_arr = json_object_get(item, "actions");
            if (actions_arr && json_is_array(actions_arr)) {
                inv_item->num_actions = (int)json_array_size(actions_arr);
                if (inv_item->num_actions > SENTIENCE_MAX_ITEM_ACTIONS)
                    inv_item->num_actions = SENTIENCE_MAX_ITEM_ACTIONS;
                for (j = 0; j < (size_t)inv_item->num_actions; j++) {
                    json_t *action = json_array_get(actions_arr, j);
                    inv_item->actions[j].label = json_string_value(json_object_get(action, "label"));
                    inv_item->actions[j].cmd = json_string_value(json_object_get(action, "cmd"));
                }
            }
        }
    }

    /* Parse capacity object */
    json_t *capacity = json_object_get(params, "capacity");
    if (capacity) {
        input.capacity_current_items = (int)json_integer_value(json_object_get(capacity, "items"));
        input.capacity_max_items = (int)json_integer_value(json_object_get(capacity, "max_items"));
        input.capacity_current_weight = (int)json_integer_value(json_object_get(capacity, "weight"));
        input.capacity_max_weight = (int)json_integer_value(json_object_get(capacity, "max_weight"));
        input.capacity_coin_weight = (int)json_integer_value(json_object_get(capacity, "coin_weight"));
    }

    /* Call the builder function */
    result = sentience_build_inventory_json(&input);
    TEST_ASSERT_NOT_NULL(result);

    /* Check expected values */
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    if (json_object_get(expected, "items_count")) {
        json_t *items_result = json_object_get(result, "items");
        TEST_ASSERT_NOT_NULL(items_result);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "items_count")),
                           (long)json_array_size(items_result));
    }

    if (json_object_get(expected, "first_item_name")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_item_name")),
                           json_string_value(json_object_get(first_item, "name")));
    }

    if (json_object_get(expected, "first_item_type")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_item_type")),
                           json_string_value(json_object_get(first_item, "item_type")));
    }

    if (json_object_get(expected, "first_item_condition")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_condition")),
                           json_integer_value(json_object_get(first_item, "condition")));
    }

    if (json_object_get(expected, "first_item_id_0")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        json_t *id_arr = json_object_get(first_item, "id");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_id_0")),
                           json_integer_value(json_array_get(id_arr, 0)));
    }

    if (json_object_get(expected, "first_item_id_1")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        json_t *id_arr = json_object_get(first_item, "id");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_id_1")),
                           json_integer_value(json_array_get(id_arr, 1)));
    }

    if (json_object_get(expected, "first_item_flags_count")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        json_t *flags_arr = json_object_get(first_item, "flags");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_flags_count")),
                           (long)json_array_size(flags_arr));
    }

    if (json_object_get(expected, "first_item_actions_count")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *first_item = json_array_get(items_result, 0);
        json_t *actions_arr = json_object_get(first_item, "actions");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_actions_count")),
                           (long)json_array_size(actions_arr));
    }

    if (json_object_get(expected, "second_item_keyword")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *second_item = json_array_get(items_result, 1);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "second_item_keyword")),
                           json_string_value(json_object_get(second_item, "keyword")));
    }

    if (json_object_get(expected, "third_item_keyword")) {
        json_t *items_result = json_object_get(result, "items");
        json_t *third_item = json_array_get(items_result, 2);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "third_item_keyword")),
                           json_string_value(json_object_get(third_item, "keyword")));
    }

    /* Check capacity values */
    if (json_object_get(expected, "capacity_items")) {
        json_t *capacity_result = json_object_get(result, "capacity");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "capacity_items")),
                           json_integer_value(json_object_get(capacity_result, "items")));
    }

    if (json_object_get(expected, "capacity_max_items")) {
        json_t *capacity_result = json_object_get(result, "capacity");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "capacity_max_items")),
                           json_integer_value(json_object_get(capacity_result, "max_items")));
    }

    if (json_object_get(expected, "capacity_weight")) {
        json_t *capacity_result = json_object_get(result, "capacity");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "capacity_weight")),
                           json_integer_value(json_object_get(capacity_result, "weight")));
    }

    if (json_object_get(expected, "capacity_max_weight")) {
        json_t *capacity_result = json_object_get(result, "capacity");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "capacity_max_weight")),
                           json_integer_value(json_object_get(capacity_result, "max_weight")));
    }

    json_decref(result);
    return TEST_SUCCESS;
}

/* --- Equipment scenario --- */

static test_result_t run_gmcp_equipment_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_equipment_input_t input = {0};
    size_t i, j;

    if (!params || !expected) return TEST_ERROR;

    /* Parse slots array */
    json_t *slots_arr = json_object_get(params, "slots");
    if (slots_arr && json_is_array(slots_arr)) {
        input.num_slots = (int)json_array_size(slots_arr);
        if (input.num_slots > SENTIENCE_MAX_EQUIPMENT_SLOTS)
            input.num_slots = SENTIENCE_MAX_EQUIPMENT_SLOTS;

        for (i = 0; i < (size_t)input.num_slots; i++) {
            json_t *slot = json_array_get(slots_arr, i);
            sentience_equipment_slot_t *eq_slot = &input.slots[i];

            eq_slot->slot_id = (int)json_integer_value(json_object_get(slot, "slot_id"));
            eq_slot->slot_name = json_string_value(json_object_get(slot, "slot_name"));
            eq_slot->occupied = json_boolean_value(json_object_get(slot, "occupied"));

            if (eq_slot->occupied) {
                eq_slot->item_name = json_string_value(json_object_get(slot, "item_name"));
                eq_slot->keywords = json_string_value(json_object_get(slot, "keywords"));
                eq_slot->keyword = json_string_value(json_object_get(slot, "keyword"));
                eq_slot->item_type = json_string_value(json_object_get(slot, "item_type"));
                eq_slot->condition = (int)json_integer_value(json_object_get(slot, "condition"));
                eq_slot->condition_label = json_string_value(json_object_get(slot, "condition_label"));
                eq_slot->level = (int)json_integer_value(json_object_get(slot, "level"));

                /* Parse ID array */
                json_t *id_arr = json_object_get(slot, "id");
                if (id_arr && json_is_array(id_arr)) {
                    eq_slot->id[0] = (unsigned long)json_integer_value(json_array_get(id_arr, 0));
                    eq_slot->id[1] = (unsigned long)json_integer_value(json_array_get(id_arr, 1));
                }

                /* Parse flags array */
                json_t *flags_arr = json_object_get(slot, "flags");
                if (flags_arr && json_is_array(flags_arr)) {
                    eq_slot->num_flags = (int)json_array_size(flags_arr);
                    if (eq_slot->num_flags > SENTIENCE_MAX_ITEM_FLAGS)
                        eq_slot->num_flags = SENTIENCE_MAX_ITEM_FLAGS;
                    for (j = 0; j < (size_t)eq_slot->num_flags; j++) {
                        eq_slot->flags[j] = json_string_value(json_array_get(flags_arr, j));
                    }
                }

                /* Parse actions array */
                json_t *actions_arr = json_object_get(slot, "actions");
                if (actions_arr && json_is_array(actions_arr)) {
                    eq_slot->num_actions = (int)json_array_size(actions_arr);
                    if (eq_slot->num_actions > SENTIENCE_MAX_ITEM_ACTIONS)
                        eq_slot->num_actions = SENTIENCE_MAX_ITEM_ACTIONS;
                    for (j = 0; j < (size_t)eq_slot->num_actions; j++) {
                        json_t *action = json_array_get(actions_arr, j);
                        eq_slot->actions[j].label = json_string_value(json_object_get(action, "label"));
                        eq_slot->actions[j].cmd = json_string_value(json_object_get(action, "cmd"));
                    }
                }
            }
        }
    }

    /* Call the builder function */
    result = sentience_build_equipment_json(&input);
    TEST_ASSERT_NOT_NULL(result);

    /* Check expected values */
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    if (json_object_get(expected, "slots_count")) {
        json_t *slots_result = json_object_get(result, "slots");
        TEST_ASSERT_NOT_NULL(slots_result);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "slots_count")),
                           (long)json_array_size(slots_result));
    }

    if (json_object_get(expected, "first_slot_name")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_slot_name")),
                           json_string_value(json_object_get(first_slot, "slot_name")));
    }

    if (json_object_get(expected, "first_slot_occupied")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        bool expected_occupied = json_boolean_value(json_object_get(expected, "first_slot_occupied"));
        if (expected_occupied) {
            TEST_ASSERT_NOT_NULL(first_item);
        } else {
            TEST_ASSERT_TRUE(json_is_null(first_item));
        }
    }

    if (json_object_get(expected, "first_item_name")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_item_name")),
                           json_string_value(json_object_get(first_item, "name")));
    }

    if (json_object_get(expected, "first_item_type")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_item_type")),
                           json_string_value(json_object_get(first_item, "item_type")));
    }

    if (json_object_get(expected, "first_item_id_0")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        json_t *id_arr = json_object_get(first_item, "id");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_id_0")),
                           json_integer_value(json_array_get(id_arr, 0)));
    }

    if (json_object_get(expected, "first_item_id_1")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        json_t *id_arr = json_object_get(first_item, "id");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_id_1")),
                           json_integer_value(json_array_get(id_arr, 1)));
    }

    if (json_object_get(expected, "first_item_flags_count")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        json_t *flags_arr = json_object_get(first_item, "flags");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_flags_count")),
                           (long)json_array_size(flags_arr));
    }

    if (json_object_get(expected, "first_item_actions_count")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *first_slot = json_array_get(slots_result, 0);
        json_t *first_item = json_object_get(first_slot, "item");
        json_t *actions_arr = json_object_get(first_item, "actions");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_item_actions_count")),
                           (long)json_array_size(actions_arr));
    }

    if (json_object_get(expected, "second_slot_name")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *second_slot = json_array_get(slots_result, 1);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "second_slot_name")),
                           json_string_value(json_object_get(second_slot, "slot_name")));
    }

    if (json_object_get(expected, "second_slot_occupied")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *second_slot = json_array_get(slots_result, 1);
        json_t *second_item = json_object_get(second_slot, "item");
        bool expected_occupied = json_boolean_value(json_object_get(expected, "second_slot_occupied"));
        if (expected_occupied) {
            TEST_ASSERT_NOT_NULL(second_item);
        } else {
            TEST_ASSERT_TRUE(json_is_null(second_item));
        }
    }

    if (json_object_get(expected, "third_item_name")) {
        json_t *slots_result = json_object_get(result, "slots");
        json_t *third_slot = json_array_get(slots_result, 2);
        json_t *third_item = json_object_get(third_slot, "item");
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "third_item_name")),
                           json_string_value(json_object_get(third_item, "name")));
    }

    json_decref(result);
    return TEST_SUCCESS;
}

static test_result_t run_gmcp_abilities_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_abilities_input_t input = {0};
    size_t i, j;

    if (!params || !expected) return TEST_ERROR;

    /* Parse abilities array */
    json_t *abilities_arr = json_object_get(params, "abilities");
    if (abilities_arr && json_is_array(abilities_arr)) {
        input.num_abilities = (int)json_array_size(abilities_arr);
        if (input.num_abilities > SENTIENCE_MAX_ABILITIES)
            input.num_abilities = SENTIENCE_MAX_ABILITIES;

        for (i = 0; i < (size_t)input.num_abilities; i++) {
            json_t *ability = json_array_get(abilities_arr, i);
            sentience_ability_t *ab = &input.abilities[i];

            ab->name = json_string_value(json_object_get(ability, "name"));
            ab->type = json_string_value(json_object_get(ability, "type"));
            ab->available = json_boolean_value(json_object_get(ability, "available"));
            ab->rating = (int)json_integer_value(json_object_get(ability, "rating"));
            ab->modifier = (int)json_integer_value(json_object_get(ability, "modifier"));
            ab->mana = (int)json_integer_value(json_object_get(ability, "mana"));
            ab->level = (int)json_integer_value(json_object_get(ability, "level"));
            ab->target = json_string_value(json_object_get(ability, "target"));
            ab->can_practice = json_boolean_value(json_object_get(ability, "can_practice"));
            ab->learn_rate = (int)json_integer_value(json_object_get(ability, "learn_rate"));

            /* Parse actions array */
            json_t *actions_arr = json_object_get(ability, "actions");
            if (actions_arr && json_is_array(actions_arr)) {
                ab->num_actions = (int)json_array_size(actions_arr);
                if (ab->num_actions > 2)
                    ab->num_actions = 2;
                for (j = 0; j < (size_t)ab->num_actions; j++) {
                    json_t *action = json_array_get(actions_arr, j);
                    ab->actions[j].label = json_string_value(json_object_get(action, "label"));
                    ab->actions[j].cmd = json_string_value(json_object_get(action, "cmd"));
                }
            }
        }
    }

    /* Call the builder function */
    result = sentience_build_abilities_json(&input);
    TEST_ASSERT_NOT_NULL(result);

    /* Check expected values */
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    if (json_object_get(expected, "abilities_count")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        TEST_ASSERT_NOT_NULL(abilities_result);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "abilities_count")),
                           (long)json_array_size(abilities_result));
    }

    if (json_object_get(expected, "first_ability_name")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_ability_name")),
                           json_string_value(json_object_get(first_ability, "name")));
    }

    if (json_object_get(expected, "first_ability_type")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_ability_type")),
                           json_string_value(json_object_get(first_ability, "type")));
    }

    if (json_object_get(expected, "first_ability_available")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        bool expected_available = json_boolean_value(json_object_get(expected, "first_ability_available"));
        bool actual_available = json_boolean_value(json_object_get(first_ability, "available"));
        TEST_ASSERT_TRUE(expected_available == actual_available);
    }

    if (json_object_get(expected, "first_ability_rating")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_ability_rating")),
                           json_integer_value(json_object_get(first_ability, "rating")));
    }

    if (json_object_get(expected, "first_ability_mana")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_ability_mana")),
                           json_integer_value(json_object_get(first_ability, "mana")));
    }

    if (json_object_get(expected, "first_ability_actions_count")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        json_t *actions_arr = json_object_get(first_ability, "actions");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_ability_actions_count")),
                           (long)json_array_size(actions_arr));
    }

    if (json_object_get(expected, "first_ability_action_label")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *first_ability = json_array_get(abilities_result, 0);
        json_t *actions_arr = json_object_get(first_ability, "actions");
        json_t *first_action = json_array_get(actions_arr, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_ability_action_label")),
                           json_string_value(json_object_get(first_action, "label")));
    }

    if (json_object_get(expected, "second_ability_target")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *second_ability = json_array_get(abilities_result, 1);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "second_ability_target")),
                           json_string_value(json_object_get(second_ability, "target")));
    }

    if (json_object_get(expected, "second_ability_actions_count")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *second_ability = json_array_get(abilities_result, 1);
        json_t *actions_arr = json_object_get(second_ability, "actions");
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "second_ability_actions_count")),
                           (long)json_array_size(actions_arr));
    }

    if (json_object_get(expected, "third_ability_type")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *third_ability = json_array_get(abilities_result, 2);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "third_ability_type")),
                           json_string_value(json_object_get(third_ability, "type")));
    }

    if (json_object_get(expected, "fourth_ability_available")) {
        json_t *abilities_result = json_object_get(result, "abilities");
        json_t *fourth_ability = json_array_get(abilities_result, 3);
        bool expected_available = json_boolean_value(json_object_get(expected, "fourth_ability_available"));
        bool actual_available = json_boolean_value(json_object_get(fourth_ability, "available"));
        TEST_ASSERT_TRUE(expected_available == actual_available);
    }

    json_decref(result);
    return TEST_SUCCESS;
}

static test_result_t run_gmcp_reputations_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_reputations_input_t input;
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    /* Initialize input struct */
    memset(&input, 0, sizeof(input));

    /* Parse reputations array */
    json_t *reputations_arr = json_object_get(params, "reputations");
    if (reputations_arr && json_is_array(reputations_arr)) {
        input.num_reputations = (int)json_array_size(reputations_arr);
        if (input.num_reputations > SENTIENCE_MAX_REPUTATIONS)
            input.num_reputations = SENTIENCE_MAX_REPUTATIONS;

        for (i = 0; i < (size_t)input.num_reputations; i++) {
            json_t *reputation = json_array_get(reputations_arr, i);
            sentience_reputation_t *rep = &input.reputations[i];

            rep->name = json_string_value(json_object_get(reputation, "name"));
            rep->rank = json_string_value(json_object_get(reputation, "rank"));
            rep->rank_color = json_string_value(json_object_get(reputation, "rank_color"));
            rep->points = (int)json_integer_value(json_object_get(reputation, "points"));
            rep->paragon_level = (int)json_integer_value(json_object_get(reputation, "paragon_level"));
            rep->max_rank = json_string_value(json_object_get(reputation, "max_rank"));
        }
    }

    /* Call the builder function */
    result = sentience_build_reputations_json(&input);
    TEST_ASSERT_NOT_NULL(result);

    /* Check expected values */
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    if (json_object_get(expected, "reputations_count")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        TEST_ASSERT_NOT_NULL(reputations_result);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "reputations_count")),
                           (long)json_array_size(reputations_result));
    }

    if (json_object_get(expected, "first_rep_name")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        json_t *first_rep = json_array_get(reputations_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_rep_name")),
                           json_string_value(json_object_get(first_rep, "name")));
    }

    if (json_object_get(expected, "first_rep_rank")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        json_t *first_rep = json_array_get(reputations_result, 0);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "first_rep_rank")),
                           json_string_value(json_object_get(first_rep, "rank")));
    }

    if (json_object_get(expected, "first_rep_points")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        json_t *first_rep = json_array_get(reputations_result, 0);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "first_rep_points")),
                           json_integer_value(json_object_get(first_rep, "points")));
    }

    if (json_object_get(expected, "second_rep_name")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        json_t *second_rep = json_array_get(reputations_result, 1);
        TEST_ASSERT_STR_EQ(json_string_value(json_object_get(expected, "second_rep_name")),
                           json_string_value(json_object_get(second_rep, "name")));
    }

    if (json_object_get(expected, "second_rep_points")) {
        json_t *reputations_result = json_object_get(result, "reputations");
        json_t *second_rep = json_array_get(reputations_result, 1);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "second_rep_points")),
                           json_integer_value(json_object_get(second_rep, "points")));
    }

    json_decref(result);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
