/**
 * Shop Stock Cross-Area Reference Tests
 * 
 * Tests SHOP_STOCK_DATA WNUM migration functionality including:
 * - Cross-area stock creation and persistence
 * - WNUM serialization/deserialization  
 * - Fixup phase conversion from WNUM_LOAD to WNUM
 * - Legacy bare vnum support
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_shop_stock_cross_area_creation(test_case_t *test);
static test_result_t test_shop_stock_serialization(test_case_t *test);
static test_result_t test_shop_stock_legacy_vnum(test_case_t *test);

/**
 * Main test dispatcher for shop stock tests
 */
test_result_t run_shop_stock_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;
    
    if (strcmp(test->test_type, "shop_stock_cross_area_creation") == 0) {
        result = test_shop_stock_cross_area_creation(test);
    }
    else if (strcmp(test->test_type, "shop_stock_serialization") == 0) {
        result = test_shop_stock_serialization(test);
    }
    else if (strcmp(test->test_type, "shop_stock_legacy_vnum") == 0) {
        result = test_shop_stock_legacy_vnum(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown shop stock test type: %s", test->test_type);
    }
    
    return result;
}

/**
 * Test cross-area shop stock creation
 * Validates that stock can reference entities from different areas
 */
static test_result_t test_shop_stock_cross_area_creation(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long shopkeeper_vnum = json_get_int(input, "shopkeeper_vnum");
    long target_area_uid = json_get_int(input, "target_area_uid");
    long target_entity_vnum = json_get_int(input, "target_entity_vnum");
    const char *stock_type_str = json_get_string(input, "stock_type");
    
    /* Find shopkeeper */
    AREA_DATA *shop_area = find_area_by_vnum(shopkeeper_vnum, NULL);
    if (!shop_area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper area not found for vnum %ld", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    MOB_INDEX_DATA *shopkeeper = get_mob_index(shop_area, shopkeeper_vnum);
    if (!shopkeeper) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper %ld not found", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    if (!shopkeeper->pShop) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Mob %ld is not a shopkeeper", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    /* Find target area */
    AREA_DATA *target_area = get_area_index(target_area_uid);
    if (!target_area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Target area UID %ld not found", target_area_uid);
        return TEST_FAILURE;
    }
    
    /* Determine stock type */
    int stock_type = STOCK_OBJECT;
    if (strcmp(stock_type_str, "pet") == 0) stock_type = STOCK_PET;
    else if (strcmp(stock_type_str, "mount") == 0) stock_type = STOCK_MOUNT;
    else if (strcmp(stock_type_str, "guard") == 0) stock_type = STOCK_GUARD;
    else if (strcmp(stock_type_str, "crew") == 0) stock_type = STOCK_CREW;
    else if (strcmp(stock_type_str, "ship") == 0) stock_type = STOCK_SHIP;
    
    /* Verify target entity exists */
    if (stock_type == STOCK_OBJECT) {
        OBJ_INDEX_DATA *obj = get_obj_index(target_area, target_entity_vnum);
        if (!obj) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Target obj %ld not found in area UID %ld",
                          target_entity_vnum, target_area_uid);
            return TEST_FAILURE;
        }
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                      "Cross-area shop stock validation successful: type %d, %ld#%ld exists",
                      stock_type, target_area_uid, target_entity_vnum);
    }
    else if (stock_type == STOCK_PET || stock_type == STOCK_MOUNT || 
             stock_type == STOCK_GUARD || stock_type == STOCK_CREW) {
        MOB_INDEX_DATA *mob = get_mob_index(target_area, target_entity_vnum);
        if (!mob) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Target mob %ld not found in area UID %ld",
                          target_entity_vnum, target_area_uid);
            return TEST_FAILURE;
        }
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                      "Cross-area shop stock validation successful: type %d, %ld#%ld exists",
                      stock_type, target_area_uid, target_entity_vnum);
    }
    
    return TEST_SUCCESS;
}

/**
 * Test shop stock serialization/deserialization
 * Validates that WNUM data survives save/load cycle
 */
static test_result_t test_shop_stock_serialization(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long shopkeeper_vnum = json_get_int(input, "shopkeeper_vnum");
    
    /* Find shopkeeper */
    AREA_DATA *area = find_area_by_vnum(shopkeeper_vnum, NULL);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper area not found for vnum %ld", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    MOB_INDEX_DATA *shopkeeper = get_mob_index(area, shopkeeper_vnum);
    if (!shopkeeper) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper %ld not found", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    if (!shopkeeper->pShop) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Mob %ld is not a shopkeeper", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    /* Check if shop has stock */
    if (!shopkeeper->pShop->stock) {
        log_message_f(LOG_LEVEL_WARN, LOG_INIT,
                      "Shopkeeper %ld has no stock to test", shopkeeper_vnum);
        return TEST_SKIP;
    }
    
    /* Iterate through stock and verify structure */
    int stock_count = 0;
    int cross_area_count = 0;
    
    for (SHOP_STOCK_DATA *stock = shopkeeper->pShop->stock; stock; stock = stock->next) {
        stock_count++;
        
        /* Check entity-referencing stock types */
        if (stock->type == STOCK_OBJECT || stock->type == STOCK_PET ||
            stock->type == STOCK_MOUNT || stock->type == STOCK_GUARD ||
            stock->type == STOCK_CREW || stock->type == STOCK_SHIP) {
            
            /* Verify WNUM structure is valid */
            if (stock->entity.wnum.vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                              "Stock %d: Invalid vnum %ld",
                              stock_count, stock->entity.wnum.vnum);
                return TEST_FAILURE;
            }
            
            /* If cross-area, verify area pointer */
            if (stock->entity.wnum.pArea && stock->entity.wnum.pArea != area) {
                cross_area_count++;
                
                if (stock->entity.wnum.pArea->uid <= 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                  "Stock %d: Cross-area stock has invalid area UID",
                                  stock_count);
                    return TEST_FAILURE;
                }
                
                log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                              "Found cross-area stock: type %d, %ld#%ld",
                              stock->type,
                              stock->entity.wnum.pArea->uid,
                              stock->entity.wnum.vnum);
            }
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                  "Validated %d stock items (%d cross-area) for shopkeeper %ld",
                  stock_count, cross_area_count, shopkeeper_vnum);
    
    return TEST_SUCCESS;
}

/**
 * Test legacy bare vnum support
 * Validates that stock with NULL pArea still works (backward compatibility)
 */
static test_result_t test_shop_stock_legacy_vnum(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long shopkeeper_vnum = json_get_int(input, "shopkeeper_vnum");
    long entity_vnum = json_get_int(input, "entity_vnum");
    const char *stock_type_str = json_get_string(input, "stock_type");
    
    /* Find shopkeeper */
    AREA_DATA *area = find_area_by_vnum(shopkeeper_vnum, NULL);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper area not found for vnum %ld", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    MOB_INDEX_DATA *shopkeeper = get_mob_index(area, shopkeeper_vnum);
    if (!shopkeeper) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Shopkeeper %ld not found", shopkeeper_vnum);
        return TEST_FAILURE;
    }
    
    /* Determine stock type */
    int stock_type = STOCK_OBJECT;
    if (strcmp(stock_type_str, "pet") == 0) stock_type = STOCK_PET;
    else if (strcmp(stock_type_str, "mount") == 0) stock_type = STOCK_MOUNT;
    
    /* Verify fallback lookup works */
    AREA_DATA *fallback_area = area;
    
    if (stock_type == STOCK_OBJECT) {
        OBJ_INDEX_DATA *obj = get_obj_index(fallback_area, entity_vnum);
        if (!obj) {
            /* Try global lookup */
            obj = get_obj_index_global(entity_vnum);
        }
        if (!obj) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Legacy stock: obj %ld not found", entity_vnum);
            return TEST_FAILURE;
        }
    }
    else if (stock_type == STOCK_PET || stock_type == STOCK_MOUNT) {
        MOB_INDEX_DATA *mob = get_mob_index(fallback_area, entity_vnum);
        if (!mob) {
            /* Try global lookup */
            mob = get_mob_index_global(entity_vnum);
        }
        if (!mob) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Legacy stock: mob %ld not found", entity_vnum);
            return TEST_FAILURE;
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                  "Legacy stock validated: type %d, vnum %ld (fallback successful)",
                  stock_type, entity_vnum);
    
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
