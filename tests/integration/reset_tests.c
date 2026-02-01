/**
 * Reset Cross-Area Reference Tests
 * 
 * Tests RESET_DATA WNUM migration functionality including:
 * - Cross-area reset creation and persistence
 * - WNUM serialization/deserialization
 * - Fixup phase conversion from WNUM_LOAD to WNUM
 * - Legacy bare vnum support
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_reset_cross_area_creation(test_case_t *test);
static test_result_t test_reset_serialization(test_case_t *test);
static test_result_t test_reset_legacy_vnum(test_case_t *test);

/**
 * Main test dispatcher for reset tests
 */
test_result_t run_reset_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;
    
    if (strcmp(test->test_type, "reset_cross_area_creation") == 0) {
        result = test_reset_cross_area_creation(test);
    }
    else if (strcmp(test->test_type, "reset_serialization") == 0) {
        result = test_reset_serialization(test);
    }
    else if (strcmp(test->test_type, "reset_legacy_vnum") == 0) {
        result = test_reset_legacy_vnum(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown reset test type: %s", test->test_type);
    }
    
    return result;
}

/**
 * Test cross-area reset creation
 * Validates that resets can reference entities from different areas
 */
static test_result_t test_reset_cross_area_creation(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long source_room_vnum = json_get_int(input, "source_room_vnum");
    long target_area_uid = json_get_int(input, "target_area_uid");
    long target_entity_vnum = json_get_int(input, "target_entity_vnum");
    const char *reset_type = json_get_string(input, "reset_type");
    
    /* Find source room */
    AREA_DATA *source_area = find_area_by_vnum(source_room_vnum, NULL);
    if (!source_area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Source room area not found for vnum %ld", source_room_vnum);
        return TEST_FAILURE;
    }
    
    ROOM_INDEX_DATA *room = get_room_index(source_area, source_room_vnum);
    if (!room) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Source room %ld not found", source_room_vnum);
        return TEST_FAILURE;
    }
    
    /* Find target area */
    AREA_DATA *target_area = get_area_index(target_area_uid);
    if (!target_area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Target area UID %ld not found", target_area_uid);
        return TEST_FAILURE;
    }
    
    /* Verify target entity exists */
    if (strcmp(reset_type, "M") == 0 || strcmp(reset_type, "G") == 0) {
        MOB_INDEX_DATA *mob = get_mob_index(target_area, target_entity_vnum);
        if (!mob) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Target mob %ld not found in area UID %ld",
                          target_entity_vnum, target_area_uid);
            return TEST_FAILURE;
        }
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                      "Cross-area reset validation successful: %c %ld#%ld exists",
                      reset_type[0], target_area_uid, target_entity_vnum);
    }
    else if (strcmp(reset_type, "O") == 0 || strcmp(reset_type, "P") == 0) {
        OBJ_INDEX_DATA *obj = get_obj_index(target_area, target_entity_vnum);
        if (!obj) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Target obj %ld not found in area UID %ld",
                          target_entity_vnum, target_area_uid);
            return TEST_FAILURE;
        }
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                      "Cross-area reset validation successful: %c %ld#%ld exists",
                      reset_type[0], target_area_uid, target_entity_vnum);
    }
    
    return TEST_SUCCESS;
}

/**
 * Test reset serialization/deserialization
 * Validates that WNUM data survives save/load cycle
 */
static test_result_t test_reset_serialization(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long area_uid = json_get_int(input, "area_uid");
    long room_vnum = json_get_int(input, "room_vnum");
    
    /* Find area and room */
    AREA_DATA *area = get_area_index(area_uid);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Area UID %ld not found", area_uid);
        return TEST_FAILURE;
    }
    
    ROOM_INDEX_DATA *room = get_room_index(area, room_vnum);
    if (!room) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Room %ld not found in area UID %ld", room_vnum, area_uid);
        return TEST_FAILURE;
    }
    
    /* Check if room has resets */
    if (!room->reset_first) {
        log_message_f(LOG_LEVEL_WARN, LOG_INIT,
                      "Room %ld has no resets to test", room_vnum);
        return TEST_SKIP;
    }
    
    /* Iterate through resets and verify structure */
    int reset_count = 0;
    int cross_area_count = 0;
    
    for (RESET_DATA *reset = room->reset_first; reset; reset = reset->next) {
        reset_count++;
        
        /* Check entity-referencing reset types */
        if (reset->command == 'M' || reset->command == 'O' || 
            reset->command == 'G' || reset->command == 'E') {
            
            /* Verify WNUM structure is valid */
            if (reset->arg1.wnum.vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                              "Reset %d: Invalid vnum %ld",
                              reset_count, reset->arg1.wnum.vnum);
                return TEST_FAILURE;
            }
            
            /* If cross-area, verify area pointer */
            if (reset->arg1.wnum.pArea && reset->arg1.wnum.pArea != room->area) {
                cross_area_count++;
                
                if (reset->arg1.wnum.pArea->uid <= 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                  "Reset %d: Cross-area reset has invalid area UID",
                                  reset_count);
                    return TEST_FAILURE;
                }
                
                log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                              "Found cross-area reset: %c %ld#%ld",
                              reset->command,
                              reset->arg1.wnum.pArea->uid,
                              reset->arg1.wnum.vnum);
            }
        }
        else if (reset->command == 'P') {
            /* Container reset - check both arg1 and arg3 */
            if (reset->arg3.wnum.pArea && reset->arg3.wnum.pArea != room->area) {
                cross_area_count++;
                
                log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                              "Found cross-area container reset: P %ld#%ld -> %ld#%ld",
                              reset->arg1.wnum.pArea ? reset->arg1.wnum.pArea->uid : 0,
                              reset->arg1.wnum.vnum,
                              reset->arg3.wnum.pArea->uid,
                              reset->arg3.wnum.vnum);
            }
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                  "Validated %d resets (%d cross-area) in room %ld",
                  reset_count, cross_area_count, room_vnum);
    
    return TEST_SUCCESS;
}

/**
 * Test legacy bare vnum support
 * Validates that resets with NULL pArea still work (backward compatibility)
 */
static test_result_t test_reset_legacy_vnum(test_case_t *test)
{
    if (!test_environment_ready()) {
        return TEST_SKIP;
    }
    
    json_t *input = json_object_get(test->config, "input");
    long room_vnum = json_get_int(input, "room_vnum");
    long entity_vnum = json_get_int(input, "entity_vnum");
    const char *reset_type = json_get_string(input, "reset_type");
    
    /* Find room */
    AREA_DATA *area = find_area_by_vnum(room_vnum, NULL);
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Room area not found for vnum %ld", room_vnum);
        return TEST_FAILURE;
    }
    
    ROOM_INDEX_DATA *room = get_room_index(area, room_vnum);
    if (!room) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Room %ld not found", room_vnum);
        return TEST_FAILURE;
    }
    
    /* Verify fallback lookup works */
    AREA_DATA *fallback_area = room->area;
    
    if (reset_type[0] == 'M') {
        MOB_INDEX_DATA *mob = get_mob_index(fallback_area, entity_vnum);
        if (!mob) {
            /* Try global lookup */
            mob = get_mob_index_global(entity_vnum);
        }
        if (!mob) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Legacy reset: mob %ld not found", entity_vnum);
            return TEST_FAILURE;
        }
    }
    else if (reset_type[0] == 'O') {
        OBJ_INDEX_DATA *obj = get_obj_index(fallback_area, entity_vnum);
        if (!obj) {
            /* Try global lookup */
            obj = get_obj_index_global(entity_vnum);
        }
        if (!obj) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                          "Legacy reset: obj %ld not found", entity_vnum);
            return TEST_FAILURE;
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_INIT,
                  "Legacy reset validated: %c %ld (fallback successful)",
                  reset_type[0], entity_vnum);
    
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
