/**
 * Combat Telemetry Synthetic Tests
 *
 * Focused branch-validation tests for combat damage reduction behavior used by
 * synthetic telemetry baselines while live population is low.
 */

#ifdef BUILD_TESTS

#include <string.h>
#include "../../merc.h"
#include "../framework/test_framework.h"
#include "../framework/test_utils.h"

static test_result_t test_combat_damage_npc_branch(test_case_t *test);
static test_result_t test_combat_damage_pc_branch(test_case_t *test);
static test_result_t test_combat_damage_mixed_equivalence(test_case_t *test);
static MOB_INDEX_DATA *find_test_npc_index(void);
static ROOM_INDEX_DATA *resolve_combat_test_room(json_t *input);
static MOB_INDEX_DATA *resolve_combat_test_npc_index(json_t *input);

static int expected_npc_branch_damage(int raw_damage)
{
    int dam = raw_damage;

    if (dam > 35)
        dam = (dam - 35) / 3 + 35;
    if (dam > 80)
        dam = (dam - 80) / 2 + 80;

    return dam;
}

static int expected_pc_branch_damage(int raw_damage,
                                     int attacker_level,
                                     int victim_level,
                                     bool same_class,
                                     int dt)
{
    int dam = raw_damage;

    if (dam > 35)
        dam = (dam - 35) * 3 / 4 + 35;
    if (dam > 80)
        dam = (dam - 80) * 3 / 4 + 80;

    if (attacker_level > victim_level)
        dam += (victim_level - attacker_level);

    if (same_class && abs(victim_level - attacker_level) < 20)
        dam = dam * 6 / 5;

    if (dt == skill_resolve_gsn("backstab"))
        dam = dam * 3 / 2;

    return dam;
}

static ROOM_INDEX_DATA *find_non_newbie_non_safe_room(void)
{
    AREA_DATA *area;
    int hash;

    for (area = area_first; area; area = area->next) {
        if (IS_SET(area->area_flags, AREA_NEWBIE))
            continue;

        for (hash = 0; hash < MAX_KEY_HASH; hash++) {
            ROOM_INDEX_DATA *room;

            for (room = area->room_index_hash[hash]; room; room = room->next) {
                if (!IS_SET(room->room_flag[0], ROOM_SAFE))
                    return room;
            }
        }
    }

    return NULL;
}

static MOB_INDEX_DATA *find_test_npc_index(void)
{
    AREA_DATA *area;
    int hash;

    for (area = area_first; area; area = area->next) {
        for (hash = 0; hash < MAX_KEY_HASH; hash++) {
            MOB_INDEX_DATA *mob_index;

            for (mob_index = area->mob_index_hash[hash]; mob_index; mob_index = mob_index->next) {
                if (IS_SET(mob_index->act[0], ACT_PROTECTED))
                    continue;
                if (mob_index->pShop != NULL)
                    continue;
                return mob_index;
            }
        }
    }

    return get_reserved_mob_index("mob_objcaster");
}

static ROOM_INDEX_DATA *resolve_combat_test_room(json_t *input)
{
    const char *area_name = "Bootstrap Tests";
    long room_vnum = 2;
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;

    if (input) {
        const char *cfg_area_name = test_json_get_string(input, "fixture_area_name");
        long cfg_room_vnum = test_json_get_int(input, "fixture_room_vnum");

        if (cfg_area_name && cfg_area_name[0])
            area_name = cfg_area_name;
        if (cfg_room_vnum > 0)
            room_vnum = cfg_room_vnum;
    }

    area = find_area((char *)area_name);
    if (area) {
        room = get_room_index(area, room_vnum);
        if (room && !IS_SET(room->room_flag[0], ROOM_SAFE) &&
            !IS_SET(room->area->area_flags, AREA_NEWBIE)) {
            return room;
        }
    }

    return find_non_newbie_non_safe_room();
}

static MOB_INDEX_DATA *resolve_combat_test_npc_index(json_t *input)
{
    const char *area_name = "Bootstrap Tests";
    long npc_vnum = 103;
    AREA_DATA *area;
    MOB_INDEX_DATA *mob_index;

    if (input) {
        const char *cfg_area_name = test_json_get_string(input, "fixture_area_name");
        long cfg_npc_vnum = test_json_get_int(input, "fixture_npc_vnum");

        if (cfg_area_name && cfg_area_name[0])
            area_name = cfg_area_name;
        if (cfg_npc_vnum > 0)
            npc_vnum = cfg_npc_vnum;
    }

    area = find_area((char *)area_name);
    if (area) {
        mob_index = get_mob_index(area, npc_vnum);
        if (mob_index && !IS_SET(mob_index->act[0], ACT_PROTECTED) &&
            mob_index->pShop == NULL) {
            return mob_index;
        }
    }

    return find_test_npc_index();
}

static void prepare_actor_common(CHAR_DATA *ch, int level, int hit)
{
    if (!ch)
        return;

    ch->tot_level = level;
    ch->level = level;
    ch->hit = hit;
    ch->max_hit = hit;
    ch->position = POS_STANDING;
    ch->wimpy = 0;
    ch->fighting = NULL;
    ch->in_damage_function = false;
    ch->set_death_type = DEATHTYPE_ALIVE;
}

test_result_t run_combat_telemetry_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (!test || !test->test_type)
        return TEST_ERROR;

    if (strcmp(test->test_type, "combat_damage_npc_branch_test") == 0) {
        result = test_combat_damage_npc_branch(test);
    } else if (strcmp(test->test_type, "combat_damage_pc_branch_test") == 0) {
        result = test_combat_damage_pc_branch(test);
    } else if (strcmp(test->test_type, "combat_damage_mixed_equivalence_test") == 0) {
        result = test_combat_damage_mixed_equivalence(test);
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown combat telemetry test type: %s", test->test_type);
    }

    return result;
}

static test_result_t test_combat_damage_npc_branch(test_case_t *test)
{
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *npc_index;
    CHAR_DATA *attacker = NULL;
    CHAR_DATA *victim = NULL;
    DESCRIPTOR_DATA *victim_desc = NULL;
    json_t *input = NULL;
    int raw_damage = 200;
    int expected;
    int before_hit;
    int actual;
    bool ok;

    if (!test_environment_ready())
        return TEST_SKIP;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (input) {
            int cfg_damage = test_json_get_int(input, "raw_damage");
            if (cfg_damage > 0)
                raw_damage = cfg_damage;
        }
    }

    room = resolve_combat_test_room(input);
    if (!room) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat telemetry: no non-newbie, non-safe room found");
        return TEST_ERROR;
    }

    npc_index = resolve_combat_test_npc_index(input);
    if (!npc_index) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat telemetry: no suitable NPC index found");
        return TEST_ERROR;
    }

    attacker = create_mobile(npc_index, false);
    if (!attacker)
        return TEST_ERROR;

    if (!test_utils_create_fake_player(&victim, &victim_desc)) {
        extract_char(attacker, true);
        return TEST_ERROR;
    }

    prepare_actor_common(attacker, 80, 5000);
    prepare_actor_common(victim, 80, 5000);

    char_to_room(attacker, room);
    char_to_room(victim, room);

    before_hit = victim->hit;
    ok = damage_new(attacker, victim, NULL, raw_damage, TYPE_UNDEFINED, DAM_BASH, false);
    actual = before_hit - victim->hit;

    expected = expected_npc_branch_damage(raw_damage);

    if (victim->in_room)
        char_from_room(victim);
    test_utils_destroy_fake_player(victim, victim_desc);

    if (IS_VALID(attacker))
        extract_char(attacker, true);

    if (!ok) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat_damage_npc_branch_test: damage_new returned false");
        return TEST_FAILURE;
    }

    if (actual != expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "NPC branch damage mismatch: raw=%d expected=%d actual=%d",
                      raw_damage, expected, actual);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_combat_damage_pc_branch(test_case_t *test)
{
    ROOM_INDEX_DATA *room;
    CHAR_DATA *attacker = NULL;
    CHAR_DATA *victim = NULL;
    DESCRIPTOR_DATA *attacker_desc = NULL;
    DESCRIPTOR_DATA *victim_desc = NULL;
    json_t *input = NULL;
    int raw_damage = 200;
    int attacker_level = 80;
    int victim_level = 55;
    int expected;
    int before_hit;
    int actual;
    bool same_class;
    bool ok;
    long saved_room_flags;

    if (!test_environment_ready())
        return TEST_SKIP;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (input) {
            int cfg_damage = test_json_get_int(input, "raw_damage");
            int cfg_attacker_level = test_json_get_int(input, "attacker_level");
            int cfg_victim_level = test_json_get_int(input, "victim_level");

            if (cfg_damage > 0)
                raw_damage = cfg_damage;
            if (cfg_attacker_level > 0)
                attacker_level = cfg_attacker_level;
            if (cfg_victim_level > 0)
                victim_level = cfg_victim_level;
        }
    }

    room = resolve_combat_test_room(input);
    if (!room) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat telemetry: no non-newbie, non-safe room found");
        return TEST_ERROR;
    }

    if (!test_utils_create_fake_player(&attacker, &attacker_desc) ||
        !test_utils_create_fake_player(&victim, &victim_desc)) {
        if (attacker || attacker_desc)
            test_utils_destroy_fake_player(attacker, attacker_desc);
        if (victim || victim_desc)
            test_utils_destroy_fake_player(victim, victim_desc);
        return TEST_ERROR;
    }

    prepare_actor_common(attacker, attacker_level, 5000);
    prepare_actor_common(victim, victim_level, 5000);

    REMOVE_BIT(attacker->act[0], ACT_IS_NPC);
    REMOVE_BIT(victim->act[0], ACT_IS_NPC);

    char_to_room(attacker, room);
    char_to_room(victim, room);

    saved_room_flags = room->room_flag[0];
    SET_BIT(room->room_flag[0], ROOM_PK);

    before_hit = victim->hit;
    ok = damage_new(attacker, victim, NULL, raw_damage, TYPE_UNDEFINED, DAM_BASH, false);
    actual = before_hit - victim->hit;

    same_class = (get_player_classnth(attacker) == get_player_classnth(victim));
    expected = expected_pc_branch_damage(raw_damage,
                                         attacker_level,
                                         victim_level,
                                         same_class,
                                         TYPE_UNDEFINED);

    room->room_flag[0] = saved_room_flags;

    if (victim->in_room)
        char_from_room(victim);
    if (attacker->in_room)
        char_from_room(attacker);
    test_utils_destroy_fake_player(victim, victim_desc);
    test_utils_destroy_fake_player(attacker, attacker_desc);

    if (!ok) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat_damage_pc_branch_test: damage_new returned false");
        return TEST_FAILURE;
    }

    if (actual != expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "PC branch damage mismatch: raw=%d expected=%d actual=%d (same_class=%s)",
                      raw_damage, expected, actual, same_class ? "true" : "false");
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_combat_damage_mixed_equivalence(test_case_t *test)
{
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *npc_index;
    CHAR_DATA *npc_attacker = NULL;
    CHAR_DATA *npc_victim = NULL;
    CHAR_DATA *pc_attacker = NULL;
    CHAR_DATA *pc_victim = NULL;
    DESCRIPTOR_DATA *pc_attacker_desc = NULL;
    DESCRIPTOR_DATA *pc_victim_desc = NULL;
    json_t *input = NULL;
    int raw_damage = 200;
    int expected;
    int actual_npc_attacker;
    int actual_npc_victim;
    int before_hit;
    bool ok_a;
    bool ok_b;

    if (!test_environment_ready())
        return TEST_SKIP;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (input) {
            int cfg_damage = test_json_get_int(input, "raw_damage");
            if (cfg_damage > 0)
                raw_damage = cfg_damage;
        }
    }

    room = resolve_combat_test_room(input);
    if (!room) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat telemetry: no non-newbie, non-safe room found");
        return TEST_ERROR;
    }

    npc_index = resolve_combat_test_npc_index(input);
    if (!npc_index) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat telemetry: no suitable NPC index found");
        return TEST_ERROR;
    }

    npc_attacker = create_mobile(npc_index, false);
    npc_victim = create_mobile(npc_index, false);
    if (!npc_attacker || !npc_victim) {
        if (npc_attacker && IS_VALID(npc_attacker))
            extract_char(npc_attacker, true);
        if (npc_victim && IS_VALID(npc_victim))
            extract_char(npc_victim, true);
        return TEST_ERROR;
    }

    if (!test_utils_create_fake_player(&pc_attacker, &pc_attacker_desc) ||
        !test_utils_create_fake_player(&pc_victim, &pc_victim_desc)) {
        if (pc_attacker || pc_attacker_desc)
            test_utils_destroy_fake_player(pc_attacker, pc_attacker_desc);
        if (pc_victim || pc_victim_desc)
            test_utils_destroy_fake_player(pc_victim, pc_victim_desc);
        if (IS_VALID(npc_attacker))
            extract_char(npc_attacker, true);
        if (IS_VALID(npc_victim))
            extract_char(npc_victim, true);
        return TEST_ERROR;
    }

    prepare_actor_common(npc_attacker, 80, 5000);
    prepare_actor_common(npc_victim, 80, 5000);
    prepare_actor_common(pc_attacker, 80, 5000);
    prepare_actor_common(pc_victim, 80, 5000);

    char_to_room(npc_attacker, room);
    char_to_room(npc_victim, room);
    char_to_room(pc_attacker, room);
    char_to_room(pc_victim, room);

    before_hit = pc_victim->hit;
    ok_a = damage_new(npc_attacker, pc_victim, NULL, raw_damage, TYPE_UNDEFINED, DAM_BASH, false);
    actual_npc_attacker = before_hit - pc_victim->hit;

    before_hit = npc_victim->hit;
    ok_b = damage_new(pc_attacker, npc_victim, NULL, raw_damage, TYPE_UNDEFINED, DAM_BASH, false);
    actual_npc_victim = before_hit - npc_victim->hit;

    if (pc_victim->in_room)
        char_from_room(pc_victim);
    if (pc_attacker->in_room)
        char_from_room(pc_attacker);
    test_utils_destroy_fake_player(pc_victim, pc_victim_desc);
    test_utils_destroy_fake_player(pc_attacker, pc_attacker_desc);

    if (IS_VALID(npc_victim))
        extract_char(npc_victim, true);
    if (IS_VALID(npc_attacker))
        extract_char(npc_attacker, true);

    if (!ok_a || !ok_b) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "combat_damage_mixed_equivalence_test: damage_new returned false");
        return TEST_FAILURE;
    }

    expected = expected_npc_branch_damage(raw_damage);

    if (actual_npc_attacker != expected || actual_npc_victim != expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Mixed branch mismatch: raw=%d expected=%d npc_attacker=%d npc_victim=%d",
                      raw_damage, expected, actual_npc_attacker, actual_npc_victim);
        return TEST_FAILURE;
    }

    if (actual_npc_attacker != actual_npc_victim) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Mixed equivalence mismatch: npc_attacker=%d npc_victim=%d",
                      actual_npc_attacker, actual_npc_victim);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
