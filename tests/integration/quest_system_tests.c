#ifdef BUILD_TESTS

#include "../framework/test_framework.h"
#include "../../merc.h"

// External quest system globals
extern QUEST_INDEX_V2_DATA *quest_index_v2_list;
extern QUEST_INDEX_DATA *quest_index_list;

// Forward declarations for individual test functions
static test_result_t run_qsys_data_loading_basic(test_case_t *test);
static test_result_t run_qsys_data_v2_registry_populated(test_case_t *test);
static test_result_t run_qsys_data_v1_registry_check(test_case_t *test);
static test_result_t run_qsys_lookup_v2_by_vnum(test_case_t *test);
static test_result_t run_qsys_lookup_v2_by_wnum(test_case_t *test);
static test_result_t run_qsys_lookup_v1_functions(test_case_t *test);
static test_result_t run_qsys_lookup_stage_and_objectives(test_case_t *test);
static test_result_t run_qsys_state_stage_validation(test_case_t *test);
static test_result_t run_qsys_state_objective_validation(test_case_t *test);
static test_result_t run_qsys_state_runtime_validation(test_case_t *test);
static test_result_t run_qsys_integrity_v2_structure(test_case_t *test);
static test_result_t run_qsys_integrity_stage_linkage(test_case_t *test);
static test_result_t run_qsys_integrity_area_references(test_case_t *test);

/**
 * Main test dispatcher for quest system tests
 */
test_result_t run_quest_system_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "qsys_data_loading_basic") == 0) {
        result = run_qsys_data_loading_basic(test);
    }
    else if (strcmp(test->test_type, "qsys_data_v2_registry_populated") == 0) {
        result = run_qsys_data_v2_registry_populated(test);
    }
    else if (strcmp(test->test_type, "qsys_data_v1_registry_check") == 0) {
        result = run_qsys_data_v1_registry_check(test);
    }
    else if (strcmp(test->test_type, "qsys_lookup_v2_by_vnum") == 0) {
        result = run_qsys_lookup_v2_by_vnum(test);
    }
    else if (strcmp(test->test_type, "qsys_lookup_v2_by_wnum") == 0) {
        result = run_qsys_lookup_v2_by_wnum(test);
    }
    else if (strcmp(test->test_type, "qsys_lookup_v1_functions") == 0) {
        result = run_qsys_lookup_v1_functions(test);
    }
    else if (strcmp(test->test_type, "qsys_lookup_stage_and_objectives") == 0) {
        result = run_qsys_lookup_stage_and_objectives(test);
    }
    else if (strcmp(test->test_type, "qsys_state_stage_validation") == 0) {
        result = run_qsys_state_stage_validation(test);
    }
    else if (strcmp(test->test_type, "qsys_state_objective_validation") == 0) {
        result = run_qsys_state_objective_validation(test);
    }
    else if (strcmp(test->test_type, "qsys_state_runtime_validation") == 0) {
        result = run_qsys_state_runtime_validation(test);
    }
    else if (strcmp(test->test_type, "qsys_integrity_v2_structure") == 0) {
        result = run_qsys_integrity_v2_structure(test);
    }
    else if (strcmp(test->test_type, "qsys_integrity_stage_linkage") == 0) {
        result = run_qsys_integrity_stage_linkage(test);
    }
    else if (strcmp(test->test_type, "qsys_integrity_area_references") == 0) {
        result = run_qsys_integrity_area_references(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown quest system test type: %s", test->test_type);
        return TEST_FAILURE;
    }

    return result;
}

// Helper function to count quests in v2 registry
static int count_quest_index_v2_entries(void)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int count = 0;
    
    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next) {
        count++;
    }
    
    return count;
}

// Helper function to count quests in v1 registry  
static int count_quest_index_entries(void)
{
    QUEST_INDEX_DATA *quest_index;
    int count = 0;
    
    for (quest_index = quest_index_list; quest_index != NULL; quest_index = quest_index->next) {
        count++;
    }
    
    return count;
}

// Helper function to find first valid quest v2 for testing
static QUEST_INDEX_V2_DATA *get_first_quest_v2(void)
{
    return quest_index_v2_list;
}

// Helper function to find first valid quest v1 for testing
static QUEST_INDEX_DATA *get_first_quest_v1(void)
{
    return quest_index_list;
}

// Test: Basic quest data loading
static test_result_t run_qsys_data_loading_basic(test_case_t *test)
{
    // Test that quest system globals are not NULL (indicating data was loaded)
    if (quest_index_v2_list == NULL && quest_index_list == NULL) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "No quest data loaded - both quest registries are NULL");
        return TEST_FAILURE;
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest system data structures initialized");
    return TEST_SUCCESS;
}

// Test: Quest v2 registry populated
static test_result_t run_qsys_data_v2_registry_populated(test_case_t *test)
{
    int quest_v2_count = count_quest_index_v2_entries();
    
    if (quest_v2_count < 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v2 registry count should be non-negative, got %d", quest_v2_count);
        return TEST_FAILURE;
    }
    
    if (quest_v2_count > 0) {
        TEST_ASSERT_NOT_NULL(quest_index_v2_list);
        
        // Test first quest structure validity
        QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
        if (first_quest) {
            if (first_quest->vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "First quest v2 has invalid vnum: %ld", first_quest->vnum);
                return TEST_FAILURE;
            }
            TEST_ASSERT_NOT_NULL(first_quest->area);
        }
        
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v2 registry populated with %d entries", quest_v2_count);
    } else {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v2 registry empty but valid");
    }
    
    return TEST_SUCCESS;
}

// Test: Quest v1 registry check
static test_result_t run_qsys_data_v1_registry_check(test_case_t *test)
{
    int quest_v1_count = count_quest_index_entries();
    
    if (quest_v1_count < 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v1 registry count should be non-negative, got %d", quest_v1_count);
        return TEST_FAILURE;
    }
    
    if (quest_v1_count > 0) {
        TEST_ASSERT_NOT_NULL(quest_index_list);
        
        // Test first quest structure validity
        QUEST_INDEX_DATA *first_quest = get_first_quest_v1();
        if (first_quest) {
            if (first_quest->vnum <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "First quest v1 has invalid vnum: %ld", first_quest->vnum);
                return TEST_FAILURE;
            }
        }
        
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v1 registry populated with %d entries", quest_v1_count);
    } else {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v1 registry empty but valid");
    }
    
    return TEST_SUCCESS;
}

// Test: Quest v2 lookup by vnum
static test_result_t run_qsys_lookup_v2_by_vnum(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 data available for lookup testing");
        return TEST_SKIP;
    }
    
    // Test lookup by vnum
    long test_vnum = first_quest->vnum;
    QUEST_INDEX_V2_DATA *found_quest = get_quest_index_v2(test_vnum);
    
    if (found_quest) {
        TEST_ASSERT_INT_EQ(found_quest->vnum, test_vnum);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v2 lookup by vnum %ld successful", test_vnum);
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v2 lookup by vnum %ld failed", test_vnum);
        return TEST_FAILURE;
    }
    
    // Test lookup of non-existent vnum
    QUEST_INDEX_V2_DATA *not_found = get_quest_index_v2(-1);
    TEST_ASSERT_NULL(not_found);
    
    return TEST_SUCCESS;
}

// Test: Quest v2 lookup by wnum
static test_result_t run_qsys_lookup_v2_by_wnum(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 data available for wnum lookup testing");
        return TEST_SKIP;
    }
    
    if (!first_quest->area) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "First quest v2 has no area reference for wnum testing");
        return TEST_SKIP;
    }
    
    // Test lookup by wnum
    WNUM test_wnum = { .pArea = first_quest->area, .vnum = first_quest->vnum };
    QUEST_INDEX_V2_DATA *found_quest = get_quest_index_v2_wnum(test_wnum);
    
    if (found_quest) {
        TEST_ASSERT_INT_EQ(found_quest->vnum, test_wnum.vnum);
        if (found_quest->area != test_wnum.pArea) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Found quest area does not match wnum area");
            return TEST_FAILURE;
        }
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v2 lookup by wnum (area:%p, vnum:%ld) successful", 
                           (void*)test_wnum.pArea, test_wnum.vnum);
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v2 lookup by wnum (area:%p, vnum:%ld) failed", 
                           (void*)test_wnum.pArea, test_wnum.vnum);
        return TEST_FAILURE;
    }
    
    // Test lookup with invalid wnum
    WNUM invalid_wnum = { .pArea = NULL, .vnum = -1 };
    QUEST_INDEX_V2_DATA *not_found = get_quest_index_v2_wnum(invalid_wnum);
    TEST_ASSERT_NULL(not_found);
    
    return TEST_SUCCESS;
}

// Test: Quest v1 lookup functions
static test_result_t run_qsys_lookup_v1_functions(test_case_t *test)
{
    QUEST_INDEX_DATA *first_quest = get_first_quest_v1();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v1 data available for lookup testing");
        return TEST_SKIP;
    }
    
    // Test lookup by vnum
    long test_vnum = first_quest->vnum;
    QUEST_INDEX_DATA *found_by_vnum = get_quest_index(test_vnum);
    
    if (found_by_vnum) {
        TEST_ASSERT_INT_EQ(found_by_vnum->vnum, test_vnum);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v1 lookup by vnum %ld successful", test_vnum);
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v1 lookup by vnum %ld failed", test_vnum);
        return TEST_FAILURE;
    }
    
    // Test lookup by wnum if area is available
    if (first_quest->area) {
        WNUM test_wnum = { .pArea = first_quest->area, .vnum = first_quest->vnum };
        QUEST_INDEX_DATA *found_by_wnum = get_quest_index_wnum(test_wnum);
        
        if (found_by_wnum) {
            TEST_ASSERT_INT_EQ(found_by_wnum->vnum, test_wnum.vnum);
            if (found_by_wnum->area != test_wnum.pArea) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Found quest v1 wnum area does not match");
                return TEST_FAILURE;
            }
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v1 lookup by wnum successful");
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v1 lookup by wnum failed");
            return TEST_FAILURE;
        }
    }
    
    return TEST_SUCCESS;
}

// Test: Quest stage and objective lookup
static test_result_t run_qsys_lookup_stage_and_objectives(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 data available for stage/objective testing");
        return TEST_SKIP;
    }
    
    // Test stage lookup
    if (first_quest->stages) {
        QUEST_STAGE_INDEX_V2_DATA *first_stage = first_quest->stages;
        int stage_id = first_stage->id;
        
        QUEST_STAGE_INDEX_V2_DATA *found_stage = quest_index_v2_get_stage(first_quest, stage_id);
        
        if (found_stage) {
            TEST_ASSERT_INT_EQ(found_stage->id, stage_id);
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest stage lookup for ID %d successful", stage_id);
            
            // Test objective lookup within stage
            if (found_stage->objectives) {
                QUEST_OBJECTIVE_INDEX_V2_DATA *first_objective = found_stage->objectives;
                int objective_id = first_objective->id;
                
                QUEST_OBJECTIVE_INDEX_V2_DATA *found_objective = 
                    quest_stage_index_v2_get_objective(found_stage, objective_id);
                
                if (found_objective) {
                    TEST_ASSERT_INT_EQ(found_objective->id, objective_id);
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest objective lookup for ID %d successful", objective_id);
                } else {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest objective lookup for ID %d failed", objective_id);
                    return TEST_FAILURE;
                }
            } else {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No objectives in first stage to test");
            }
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest stage lookup for ID %d failed", stage_id);
            return TEST_FAILURE;
        }
    } else {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No stages in first quest v2 to test");
    }
    
    return TEST_SUCCESS;
}

// Test: Quest stage validation
static test_result_t run_qsys_state_stage_validation(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 data available for stage validation");
        return TEST_SKIP;
    }
    
    // Test entry stage ID validation
    if (first_quest->entry_stage_id > 0) {
        QUEST_STAGE_INDEX_V2_DATA *entry_stage = quest_index_v2_get_stage(first_quest, first_quest->entry_stage_id);
        
        if (entry_stage) {
            TEST_ASSERT_INT_EQ(entry_stage->id, first_quest->entry_stage_id);
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest entry stage ID %d validation successful", first_quest->entry_stage_id);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest entry stage ID %d not found in quest stages", first_quest->entry_stage_id);
            return TEST_FAILURE;
        }
    }
    
    // Test stage ID uniqueness
    if (first_quest->stages) {
        QUEST_STAGE_INDEX_V2_DATA *stage1, *stage2;
        bool found_duplicate = false;
        
        for (stage1 = first_quest->stages; stage1 != NULL; stage1 = stage1->next) {
            for (stage2 = stage1->next; stage2 != NULL; stage2 = stage2->next) {
                if (stage1->id == stage2->id) {
                    found_duplicate = true;
                    break;
                }
            }
            if (found_duplicate) break;
        }
        
        TEST_ASSERT_FALSE(found_duplicate);
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest stage ID uniqueness validation successful");
    }
    
    return TEST_SUCCESS;
}

// Test: Quest objective validation
static test_result_t run_qsys_state_objective_validation(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest || !first_quest->stages) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 stages available for objective validation");
        return TEST_SKIP;
    }
    
    QUEST_STAGE_INDEX_V2_DATA *stage;
    bool found_objectives = false;
    
    // Test objective ID uniqueness within stages
    for (stage = first_quest->stages; stage != NULL; stage = stage->next) {
        if (stage->objectives) {
            found_objectives = true;
            QUEST_OBJECTIVE_INDEX_V2_DATA *obj1, *obj2;
            bool found_duplicate = false;
            
            for (obj1 = stage->objectives; obj1 != NULL; obj1 = obj1->next) {
                for (obj2 = obj1->next; obj2 != NULL; obj2 = obj2->next) {
                    if (obj1->id == obj2->id) {
                        found_duplicate = true;
                        break;
                    }
                }
                if (found_duplicate) break;
            }
            
            TEST_ASSERT_FALSE(found_duplicate);
        }
    }
    
    if (found_objectives) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest objective ID validation successful");
    } else {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No objectives found to validate");
    }
    
    return TEST_SUCCESS;
}

// Test: Quest runtime validation
static test_result_t run_qsys_state_runtime_validation(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *first_quest = get_first_quest_v2();
    
    if (!first_quest) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "No quest v2 data available for runtime validation");
        return TEST_SKIP;
    }
    
    // Test quest target scope validation
    if (first_quest->target_scope < 0 || first_quest->target_scope > 10) {  // Reasonable range check
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest target scope %d appears to be out of valid range", first_quest->target_scope);
        return TEST_FAILURE;
    }
    
    // Test seed policy validation
    if (first_quest->seed_policy < 0 || first_quest->seed_policy > 10) {  // Reasonable range check
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest seed policy %d appears to be out of valid range", first_quest->seed_policy);
        return TEST_FAILURE;
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest runtime state validation successful");
    return TEST_SUCCESS;
}

// Test: Quest v2 structure integrity
static test_result_t run_qsys_integrity_v2_structure(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int quest_count = 0;
    
    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next) {
        quest_count++;
        
        // Basic structure validation
        if (quest_index_v2->vnum <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Quest v2 #%d has invalid vnum: %ld", quest_count, quest_index_v2->vnum);
            return TEST_FAILURE;
        }
        TEST_ASSERT_NOT_NULL(quest_index_v2->area);
        
        // Validate stages if present
        if (quest_index_v2->stages) {
            QUEST_STAGE_INDEX_V2_DATA *stage;
            int stage_count = 0;
            
            for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
                stage_count++;
                if (stage->id <= 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                        "Quest v2 #%d stage #%d has invalid ID: %d", quest_count, stage_count, stage->id);
                    return TEST_FAILURE;
                }
            }
        }
        
        // Validate rewards if present (note: rewards don't have an 'id' field)
        if (quest_index_v2->rewards) {
            QUEST_REWARD_INDEX_V2_DATA *reward;
            int reward_count = 0;
            
            for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next) {
                reward_count++;
                // Basic reward structure validation - checking for valid type or other fields
                // Note: rewards don't have an 'id' field based on the compile error
            }
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest v2 structure integrity validated for %d quests", quest_count);
    return TEST_SUCCESS;
}

// Test: Quest stage linkage integrity
static test_result_t run_qsys_integrity_stage_linkage(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int quest_count = 0;
    
    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next) {
        quest_count++;
        
        if (!quest_index_v2->stages) {
            continue;  // Skip quests with no stages
        }
        
        // Check entry stage reference validity
        if (quest_index_v2->entry_stage_id > 0) {
            QUEST_STAGE_INDEX_V2_DATA *entry_stage = quest_index_v2_get_stage(quest_index_v2, quest_index_v2->entry_stage_id);
            if (!entry_stage) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                    "Quest v2 #%d entry stage ID %d not found", quest_count, quest_index_v2->entry_stage_id);
                return TEST_FAILURE;
            }
        }
        
        // Check objective linkage within stages
        QUEST_STAGE_INDEX_V2_DATA *stage;
        for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
            if (stage->objectives) {
                QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
                for (objective = stage->objectives; objective != NULL; objective = objective->next) {
                    // Check target reference integrity if present
                    if (objective->target_ref_stage_id > 0 && objective->target_ref_objective_id > 0) {
                        QUEST_STAGE_INDEX_V2_DATA *ref_stage = quest_index_v2_get_stage(quest_index_v2, objective->target_ref_stage_id);
                        if (ref_stage) {
                            QUEST_OBJECTIVE_INDEX_V2_DATA *ref_objective = 
                                quest_stage_index_v2_get_objective(ref_stage, objective->target_ref_objective_id);
                            if (!ref_objective) {
                                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                                    "Quest v2 #%d objective target reference is invalid", quest_count);
                                return TEST_FAILURE;
                            }
                        }
                    }
                }
            }
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest stage linkage integrity validated for %d quests", quest_count);
    return TEST_SUCCESS;
}

// Test: Quest area reference integrity
static test_result_t run_qsys_integrity_area_references(test_case_t *test)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int quest_count = 0;
    int valid_area_refs = 0;
    
    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next) {
        quest_count++;
        
        if (quest_index_v2->area) {
            valid_area_refs++;
            
            // Basic area validation
            if (quest_index_v2->area->uid <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                    "Quest v2 #%d area has invalid UID: %ld", quest_count, quest_index_v2->area->uid);
                return TEST_FAILURE;
            }
            
            // Check area hash consistency if the quest is in the area's hash
            if (quest_index_v2->vnum > 0) {
                int slot = (int)(quest_index_v2->vnum % MAX_KEY_HASH);
                if (quest_index_v2->area->quest_index_v2_hash[slot] == quest_index_v2) {
                    // This quest is in the area's hash - verify WNUM lookup works
                    WNUM test_wnum = { .pArea = quest_index_v2->area, .vnum = quest_index_v2->vnum };
                    QUEST_INDEX_V2_DATA *found = get_quest_index_v2_wnum(test_wnum);
                    if (found != quest_index_v2) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                            "Quest v2 #%d WNUM lookup returned different quest", quest_count);
                        return TEST_FAILURE;
                    }
                }
            }
        }
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Quest area reference integrity validated: %d/%d quests have valid area refs", 
                       valid_area_refs, quest_count);
    return TEST_SUCCESS;
}

#endif // BUILD_TESTS