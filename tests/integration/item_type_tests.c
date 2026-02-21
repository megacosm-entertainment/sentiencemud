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
static test_result_t test_item_type_typed_data_consistency(test_case_t *test);

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
    else if (strcmp(test->test_type, "item_type_typed_data_test") == 0) {
        result = test_item_type_typed_data_consistency(test);
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

static bool obj_index_primary_typed_data_present(OBJ_INDEX_DATA *pObj)
{
    if (!pObj)
        return false;

    switch (pObj->item_type) {
        case ITEM_ARMOUR:            return IS_ARMOR(pObj);
        case ITEM_BOOK:              return IS_BOOK(pObj);
        case ITEM_CART:              return IS_CART(pObj);
        case ITEM_COMPASS:           return IS_COMPASS(pObj);
        case ITEM_CONTAINER:         return IS_CONTAINER(pObj);
        case ITEM_FLUID_CONTAINER:   return IS_FLUID_CON(pObj);
        case ITEM_FOOD:              return IS_FOOD(pObj);
        case ITEM_FURNITURE:         return IS_FURNITURE(pObj);
        case ITEM_INK:               return IS_INK(pObj);
        case ITEM_INSTRUMENT:        return IS_INSTRUMENT(pObj);
        case ITEM_JEWELRY:           return IS_JEWELRY(pObj);
        case ITEM_LIGHT:             return IS_LIGHT(pObj);
        case ITEM_MAP:               return IS_MAP(pObj);
        case ITEM_MIST:              return IS_MIST(pObj);
        case ITEM_MONEY:             return IS_MONEY(pObj);
        case ITEM_PAGE:              return IS_PAGE(pObj);
        case ITEM_PORTAL:            return IS_PORTAL(pObj);
        case ITEM_SCROLL:            return IS_SCROLL(pObj);
        case ITEM_SEXTANT:           return IS_SEXTANT(pObj);
        case ITEM_TATTOO:            return IS_TATTOO(pObj);
        case ITEM_TELESCOPE:         return IS_TELESCOPE(pObj);
        case ITEM_TOOL:              return IS_TOOL(pObj);
        case ITEM_WAND:              return IS_WAND(pObj);
        case ITEM_WEAPON:            return IS_WEAPON(pObj);
        case ITEM_WEAPON_CONTAINER:  return IS_WEAPON_CON(pObj);
        default:                     return true;
    }
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
    bool strict_type_resolution = true;

    if (json_object_get(input, "strict_type_resolution")) {
        strict_type_resolution = test_json_get_bool(input, "strict_type_resolution");
    }

    if (!test_cases || !json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *primary_name = test_json_get_string(tc, "primary_type");
        const char *add_name = test_json_get_string(tc, "add_type");
        bool expected = test_json_get_bool(tc, "expected_compatible");
        bool check_reverse = false;
        const char *desc = test_json_get_string(tc, "description");

        if (json_object_get(tc, "check_reverse")) {
            check_reverse = test_json_get_bool(tc, "check_reverse");
        }

        if (!primary_name || !add_name) continue;

        int primary = resolve_item_type(primary_name);
        int add = resolve_item_type(add_name);

        if (primary < 0 || add < 0) {
            if (strict_type_resolution) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Cannot resolve type names in compatibility case: %s/%s (%s)",
                             primary_name, add_name, desc ? desc : "");
                return TEST_ERROR;
            }

            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                         "Cannot resolve type names in compatibility case: %s/%s (%s)",
                         primary_name, add_name, desc ? desc : "");
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

        if (check_reverse) {
            TYPE_BITSET reverse_current;
            TBIT_ZERO(reverse_current);
            TBIT_SET(reverse_current, add);

            bool reverse_compatible = obj_can_add_item_type(add, reverse_current, primary);
            if (reverse_compatible != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Reverse compatibility mismatch for %s + %s: expected %s, got %s (%s)",
                             add_name,
                             primary_name,
                             expected ? "compatible" : "incompatible",
                             reverse_compatible ? "compatible" : "incompatible",
                             desc ? desc : "");
                return TEST_FAILURE;
            }
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

        if (should_exist && result == 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "type_flags lookup '%s' failed, expected valid", name);
            return TEST_FAILURE;
        }

        if (!should_exist && result != 0) {
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
    int min_objects_expected = 1;
    bool require_primary_type_name = true;
    bool require_primary_in_type_flags = false;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg = test_json_get_int(input, "max_areas_to_check");
            if (cfg > 0) max_areas = cfg;

            if (json_object_get(input, "min_objects_expected")) {
                int min_cfg = test_json_get_int(input, "min_objects_expected");
                if (min_cfg > 0) min_objects_expected = min_cfg;
            }

            if (json_object_get(input, "require_primary_type_name")) {
                require_primary_type_name = test_json_get_bool(input, "require_primary_type_name");
            }

            if (json_object_get(input, "require_primary_in_type_flags")) {
                require_primary_in_type_flags = test_json_get_bool(input, "require_primary_in_type_flags");
            }
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
                    continue;
                }

                if (require_primary_type_name) {
                    const char *type_name = item_type_info[pObj->item_type].name;
                    if (!type_name || type_name[0] == '\0') {
                        invalid_count++;
                        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                                     "Object %ld in area '%s' has unnamed item_type %d",
                                     pObj->vnum, area->name, pObj->item_type);
                    }
                }

                if (require_primary_in_type_flags && !TBIT_TST(pObj->type_flags, pObj->item_type)) {
                    invalid_count++;
                    log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                                 "Object %ld in area '%s' primary item_type %d missing from type_flags",
                                 pObj->vnum, area->name, pObj->item_type);
                }
            }
        }
    }

    if (obj_count < min_objects_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Object sample too small: expected at least %d objects, found %d",
                     min_objects_expected,
                     obj_count);
        return TEST_FAILURE;
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

static test_result_t test_item_type_typed_data_consistency(test_case_t *test)
{
    if (!test || !test->config) {
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *test_cases = input ? json_object_get(input, "test_cases") : NULL;
    int max_areas = 0;

    if (!json_is_array(test_cases)) {
        return TEST_ERROR;
    }

    if (json_object_get(input, "max_areas_to_check")) {
        max_areas = test_json_get_int(input, "max_areas_to_check");
    }

    size_t index;
    json_t *tc;
    json_array_foreach(test_cases, index, tc) {
        const char *primary_name = test_json_get_string(tc, "primary_type");
        bool expected_has_typed_data = test_json_get_bool(tc, "expected_has_typed_data");
        int min_objects = 1;
        int matched = 0;

        if (!primary_name || primary_name[0] == '\0') {
            return TEST_ERROR;
        }

        if (json_object_get(tc, "min_objects")) {
            int cfg_min = test_json_get_int(tc, "min_objects");
            if (cfg_min > 0) {
                min_objects = cfg_min;
            }
        }

        int primary = resolve_item_type(primary_name);
        if (primary <= 0 || primary >= ITEM__MAX) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Typed-data test could not resolve primary type '%s'",
                         primary_name);
            return TEST_ERROR;
        }

        if (item_type_info[primary].has_typed_data != expected_has_typed_data) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Typed-data metadata mismatch for '%s': expected has_typed_data=%s, actual=%s",
                         primary_name,
                         expected_has_typed_data ? "true" : "false",
                         item_type_info[primary].has_typed_data ? "true" : "false");
            return TEST_FAILURE;
        }

        int area_count = 0;
        AREA_DATA *area;
        for (area = area_first; area; area = area->next) {
            if (max_areas > 0 && area_count >= max_areas) {
                break;
            }
            area_count++;

            for (int hash = 0; hash < MAX_KEY_HASH; hash++) {
                OBJ_INDEX_DATA *pObj;
                for (pObj = area->obj_index_hash[hash]; pObj; pObj = pObj->next) {
                    if (pObj->item_type != primary) {
                        continue;
                    }

                    matched++;
                    if (expected_has_typed_data && !obj_index_primary_typed_data_present(pObj)) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                     "Object %ld in area '%s' missing typed data for primary type '%s'",
                                     pObj->vnum,
                                     area->name ? area->name : "(null)",
                                     primary_name);
                        return TEST_FAILURE;
                    }
                }
            }
        }

        if (matched < min_objects) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Typed-data test found too few '%s' objects: expected at least %d, found %d",
                         primary_name,
                         min_objects,
                         matched);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
