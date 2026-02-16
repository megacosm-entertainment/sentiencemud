/**
 * Item Type System Tests
 * 
 * Tests the multi-type item system including:
 * - Item type info table population
 * - Type compatibility matrix validation
 * - Type flags lookup
 * - Loaded object type validation
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../item_types.h"
#include "../../tables.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_item_type_info_populated(test_case_t *test);
static test_result_t test_item_type_compat(test_case_t *test);
static test_result_t test_item_type_flags(test_case_t *test);
static test_result_t test_item_type_loaded_objs(test_case_t *test);

/**
 * Main test dispatcher for item type tests
 */
test_result_t run_item_type_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "item_type_info_test") == 0) {
        result = test_item_type_info_populated(test);
    }
    else if (strcmp(test->test_type, "item_type_compat_test") == 0) {
        result = test_item_type_compat(test);
    }
    else if (strcmp(test->test_type, "item_type_flags_test") == 0) {
        result = test_item_type_flags(test);
    }
    else if (strcmp(test->test_type, "item_type_loaded_objs_test") == 0) {
        result = test_item_type_loaded_objs(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown item type test type: %s", test->test_type);
    }

    return result;
}

/**
 * Resolve item type constant from name (uses type_flags table)
 */
static int resolve_item_type(const char *name)
{
    if (!name) return -1;
    return flag_lookup(name, type_flags);
}

/**
 * Test item_type_info table has names for known types
 */
static test_result_t test_item_type_info_populated(test_case_t *test)
{
    int populated = 0;

    /* Check that common types have names */
    int types_to_check[] = {
        ITEM_LIGHT, ITEM_SCROLL, ITEM_WAND, ITEM_STAFF,
        ITEM_WEAPON, ITEM_ARMOUR, ITEM_CONTAINER, ITEM_FOOD,
        ITEM_PORTAL, ITEM_FURNITURE
    };
    int num_types = sizeof(types_to_check) / sizeof(types_to_check[0]);

    for (int i = 0; i < num_types; i++) {
        int type = types_to_check[i];
        if (type < 0 || type >= ITEM__MAX) continue;

        if (item_type_info[type].name && strlen(item_type_info[type].name) > 0) {
            populated++;
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "item_type_info[%d] has NULL or empty name", type);
            return TEST_FAILURE;
        }
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Item type info: %d/%d common types populated", populated, num_types);
    return TEST_SUCCESS;
}

/**
 * Test item type compatibility rules
 */
static test_result_t test_item_type_compat(test_case_t *test)
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
        const char *primary_name = test_json_get_string(tc, "primary_type");
        const char *add_name = test_json_get_string(tc, "add_type");
        bool expected = test_json_get_bool(tc, "expected_compatible");
        const char *desc = test_json_get_string(tc, "description");

        if (!primary_name || !add_name) continue;

        int primary = resolve_item_type(primary_name);
        int add = resolve_item_type(add_name);

        if (primary < 0 || add < 0) {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                         "Cannot resolve type names: %s/%s, skipping",
                         primary_name, add_name);
            continue;
        }

        /* Build a type_flags bitset with just the primary type set */
        TYPE_BITSET current;
        TBIT_ZERO(current);
        TBIT_SET(current, primary);

        bool compatible = obj_can_add_item_type(primary, current, add);

        if (compatible != expected) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Compatibility mismatch for %s + %s: expected %s, got %s (%s)",
                         primary_name, add_name,
                         expected ? "compatible" : "incompatible",
                         compatible ? "compatible" : "incompatible",
                         desc ? desc : "");
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test type_flags table lookups for known item types
 */
static test_result_t test_item_type_flags(test_case_t *test)
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

        int result = flag_lookup(name, type_flags);

        if (should_exist && result <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "type_flags lookup '%s' failed, expected valid", name);
            return TEST_FAILURE;
        }

        if (!should_exist && result > 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "type_flags lookup '%s' succeeded, expected failure", name);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test that loaded objects in areas have valid primary item types
 */
static test_result_t test_item_type_loaded_objs(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }

    int max_areas = 5;
    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg = test_json_get_int(input, "max_areas_to_check");
            if (cfg > 0) max_areas = cfg;
        }
    }

    int area_count = 0;
    int obj_count = 0;
    int invalid_count = 0;
    AREA_DATA *area;

    for (area = area_first; area && area_count < max_areas; area = area->next) {
        area_count++;

        for (int hash = 0; hash < MAX_KEY_HASH; hash++) {
            OBJ_INDEX_DATA *pObj;
            for (pObj = area->obj_index_hash[hash]; pObj; pObj = pObj->next) {
                obj_count++;

                if (pObj->item_type <= 0 || pObj->item_type >= ITEM__MAX) {
                    invalid_count++;
                    log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                                 "Object %ld in area '%s' has invalid item_type %d",
                                 pObj->vnum, area->name, pObj->item_type);
                }
            }
        }
    }

    if (obj_count == 0) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "No objects found in checked areas");
        return TEST_SUCCESS;
    }

    if (invalid_count > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "%d/%d objects have invalid item types",
                     invalid_count, obj_count);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d objects in %d areas have valid item types",
                 obj_count, area_count);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
