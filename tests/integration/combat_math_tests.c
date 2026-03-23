// Minimal test
#ifdef BUILD_TESTS
#include "../../merc.h"
#include "../framework/test_framework.h"
extern void pick_dam_verb_test(int, int, const char **, const char **, char *);
extern int catalyst_from_damage_type_test(int dam_type);

test_result_t run_combat_math_test_case(test_case_t *test) 
{ 
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }

    // Simple damage verb test
    if (strstr(test->test_type, "damage_verb")) {
        const char *vs = NULL;
        const char *vp = NULL; 
        char punct = '.';
        
        pick_dam_verb_test(10, 1000, &vs, &vp, &punct);
        
        if (vs && vp && strcmp(vs, "hit") == 0 && strcmp(vp, "hits") == 0) {
            log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Damage verb test PASSED: 10 on 1000 HP -> hit/hits");
            return TEST_SUCCESS;
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Damage verb test FAILED: got %s/%s", 
                         vs ? vs : "NULL", vp ? vp : "NULL");
            return TEST_FAILURE;
        }
    }
    
    // Simple catalyst test
    if (strstr(test->test_type, "catalyst")) {
        int result = catalyst_from_damage_type_test(DAM_FIRE);
        if (result == CATALYST_FIRE) {
            log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Catalyst test PASSED: DAM_FIRE -> CATALYST_FIRE");
            return TEST_SUCCESS;
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Catalyst test FAILED: expected %d, got %d", 
                         CATALYST_FIRE, result);
            return TEST_FAILURE;
        }
    }
    
    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unknown test type: %s", test->test_type);
    return TEST_ERROR;
}
#endif
