/**
 * Lookup & Flag Table Tests
 * 
 * Tests the flag/stat lookup functions and table integrity including:
 * - position_lookup, sex_lookup, size_lookup
 * - flag_lookup for various flag tables
 * - damage_class_lookup
 * - Flag table structural integrity (no NULL names)
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../tables.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_position_lookup(test_case_t *test);
static test_result_t test_sex_lookup(test_case_t *test);
static test_result_t test_size_lookup(test_case_t *test);
static test_result_t test_flag_table_lookup(test_case_t *test);
static test_result_t test_damage_class_lookup(test_case_t *test);
static test_result_t test_flag_table_integrity(test_case_t *test);
static test_result_t test_sector_runtime_lookup(test_case_t *test);
static test_result_t test_sector_affinity_lookup(test_case_t *test);

/**
 * Resolve a flag table name to a pointer
 */
static const struct flag_type *resolve_table(const char *name)
{
    if (!name) return NULL;

    if (strcmp(name, "act_flags") == 0)     return act_flags;
    if (strcmp(name, "affect_flags") == 0)  return affect_flags;
    if (strcmp(name, "room_flags") == 0)    return room_flags;
    if (strcmp(name, "exit_flags") == 0)    return exit_flags;
    if (strcmp(name, "wear_flags") == 0)    return wear_flags;
    if (strcmp(name, "extra_flags") == 0)   return extra_flags;
    if (strcmp(name, "weapon_class") == 0)  return weapon_class;
    if (strcmp(name, "sector_flags") == 0)  return sector_flags;
    if (strcmp(name, "container_flags") == 0) return container_flags;
    if (strcmp(name, "portal_flags") == 0)  return portal_flags;
    if (strcmp(name, "off_flags") == 0)     return off_flags;
    if (strcmp(name, "imm_flags") == 0)     return imm_flags;
    if (strcmp(name, "res_flags") == 0)     return res_flags;
    if (strcmp(name, "vuln_flags") == 0)    return vuln_flags;
    if (strcmp(name, "form_flags") == 0)    return form_flags;
    if (strcmp(name, "part_flags") == 0)    return part_flags;
    if (strcmp(name, "comm_flags") == 0)    return comm_flags;
    if (strcmp(name, "type_flags") == 0)    return type_flags;
    if (strcmp(name, "apply_flags") == 0)   return apply_flags;

    return NULL;
}

/**
 * Main test dispatcher for lookup/table tests
 */
test_result_t run_lookup_table_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "position_lookup_test") == 0) {
        result = test_position_lookup(test);
    }
    else if (strcmp(test->test_type, "sex_lookup_test") == 0) {
        result = test_sex_lookup(test);
    }
    else if (strcmp(test->test_type, "size_lookup_test") == 0) {
        result = test_size_lookup(test);
    }
    else if (strcmp(test->test_type, "flag_table_lookup_test") == 0) {
        result = test_flag_table_lookup(test);
    }
    else if (strcmp(test->test_type, "damage_class_lookup_test") == 0) {
        result = test_damage_class_lookup(test);
    }
    else if (strcmp(test->test_type, "flag_table_integrity_test") == 0) {
        result = test_flag_table_integrity(test);
    }
    else if (strcmp(test->test_type, "sector_runtime_lookup_test") == 0) {
        result = test_sector_runtime_lookup(test);
    }
    else if (strcmp(test->test_type, "sector_affinity_lookup_test") == 0) {
        result = test_sector_affinity_lookup(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown lookup table test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test position_lookup for known positions
 */
static test_result_t test_position_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        int result = position_lookup(name);

        if (should_exist && result < 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "position_lookup('%s') returned %d, expected valid",
                         name, result);
            return TEST_FAILURE;
        }

        if (!should_exist && result >= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "position_lookup('%s') returned %d, expected invalid",
                         name, result);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test sex_lookup for known sexes
 */
static test_result_t test_sex_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        int result = sex_lookup(name);

        if (should_exist && result < 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "sex_lookup('%s') returned %d, expected valid",
                         name, result);
            return TEST_FAILURE;
        }

        if (!should_exist && result >= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "sex_lookup('%s') returned %d, expected invalid",
                         name, result);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test size_lookup for known sizes
 */
static test_result_t test_size_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        int result = size_lookup(name);

        if (should_exist && result < 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "size_lookup('%s') returned %d, expected valid",
                         name, result);
            return TEST_FAILURE;
        }

        if (!should_exist && result >= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "size_lookup('%s') returned %d, expected invalid",
                         name, result);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test flag_lookup on a specific flag table
 */
static test_result_t test_flag_table_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    const char *table_name = test_json_get_string(input, "table");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!table_name || !test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    const struct flag_type *table = resolve_table(table_name);
    if (!table) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Cannot resolve table: %s", table_name);
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");

        if (!name) continue;

        int result = flag_lookup(name, table);

        if (should_exist && (result == 0 || result == NO_FLAG)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "flag_lookup('%s', %s) returned %d, expected valid",
                         name, table_name, result);
            return TEST_FAILURE;
        }

        if (!should_exist && result != 0 && result != NO_FLAG) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "flag_lookup('%s', %s) returned %d, expected miss sentinel (0/NO_FLAG)",
                         name, table_name, result);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test damage_class_lookup for known damage types
 */
static test_result_t test_damage_class_lookup(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        const char *expected_name = test_json_get_string(tc, "expected_name");

        if (!name) continue;

        int result = damage_class_lookup(name);
        int expected = DAM_NONE;

        if (expected_name && expected_name[0]) {
            if (!str_cmp(expected_name, "none")) {
                expected = DAM_NONE;
            } else {
                expected = damage_class_lookup(expected_name);
            }
        } else {
            expected = flag_lookup(name, damage_classes);
            if (expected == DAM_NONE) {
                expected = DAM_BASH;
            }
        }

            if (result != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "damage_class_lookup('%s') returned %d, expected %d",
                             name, result, expected);
                return TEST_FAILURE;
            }
    }

    return TEST_SUCCESS;
}

/**
 * Test flag table structural integrity - no NULL/empty names before sentinel
 */
static test_result_t test_flag_table_integrity(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *tables = json_object_get(input, "tables_to_check");

    if (!tables || !json_is_array(tables)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tname;
    json_array_foreach(tables, index, tname) {
        const char *table_name = json_string_value(tname);
        if (!table_name) continue;

        const struct flag_type *table = resolve_table(table_name);
        if (!table) {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                         "Cannot resolve table '%s' for integrity check, skipping",
                         table_name);
            continue;
        }

        int entry_count = 0;
        for (int i = 0; table[i].name != NULL; i++) {
            entry_count++;

            if (strlen(table[i].name) == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Table '%s' entry %d has empty name",
                             table_name, i);
                return TEST_FAILURE;
            }
        }

        if (entry_count == 0) {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                         "Table '%s' has no entries", table_name);
        }
    }

    return TEST_SUCCESS;
}

static test_result_t test_sector_runtime_lookup(test_case_t *test)
{
    int sector_id;
    int found;

    if (!test || !test->config)
        return TEST_ERROR;

    load_sector_data();

    sector_id = (int)json_get_int(test->config, "sector_id", SECT_FIELD);
    sector_id = sector_type_sanitize(sector_id);

    if (!sector_set_hide_msg(sector_id, 0, "among the old stones"))
        return TEST_FAILURE;
    if (!sector_set_hide_msg(sector_id, 1, "inside a weathered hollow"))
        return TEST_FAILURE;

    if (sector_hide_msg_count(sector_id) < 2) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector_hide_msg_count(%d) < 2 after setting two messages", sector_id);
        return TEST_FAILURE;
    }

    if (strcmp(sector_hide_msg(sector_id, 0), "among the old stones") != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector_hide_msg(%d,0) mismatch", sector_id);
        return TEST_FAILURE;
    }

    found = sector_lookup(sector_name(sector_id));
    if (found != sector_id) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector_lookup(sector_name(%d)) returned %d", sector_id, found);
        return TEST_FAILURE;
    }

    sector_set_hide_msg(sector_id, 0, "");
    sector_set_hide_msg(sector_id, 1, "");

    return TEST_SUCCESS;
}

static test_result_t test_sector_affinity_lookup(test_case_t *test)
{
    int sector_id;

    if (!test || !test->config)
        return TEST_ERROR;

    load_sector_data();

    sector_id = (int)json_get_int(test->config, "sector_id", SECT_FIELD);
    sector_id = sector_type_sanitize(sector_id);

    if (!sector_set_affinity(sector_id, 0, CATALYST_FIRE, 25))
        return TEST_FAILURE;
    if (!sector_set_affinity(sector_id, 1, CATALYST_ICE, -15))
        return TEST_FAILURE;

    if (sector_affinity_catalyst(sector_id, 0) != CATALYST_FIRE ||
        sector_affinity_value(sector_id, 0) != 25) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector affinity slot 0 mismatch for sector %d", sector_id);
        return TEST_FAILURE;
    }

    if (sector_affinity_catalyst(sector_id, 1) != CATALYST_ICE ||
        sector_affinity_value(sector_id, 1) != -15) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector affinity slot 1 mismatch for sector %d", sector_id);
        return TEST_FAILURE;
    }

    if (!sector_clear_affinity(sector_id, 0))
        return TEST_FAILURE;

    if (sector_affinity_catalyst(sector_id, 0) != CATALYST_NONE ||
        sector_affinity_value(sector_id, 0) != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "sector affinity slot 0 not cleared for sector %d", sector_id);
        return TEST_FAILURE;
    }

    sector_clear_affinity(sector_id, 1);

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
