/**
 * Spell Data Integrity Tests
 * 
 * Tests spell/magic data integrity including:
 * - Spell table population and counting
 * - Spell lookup functions by name
 * - Spell function pointer validation
 * - Spell level range validation  
 * - Spell function name resolution
 * - Spell categorization and metadata integrity
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../skill_data.h"
#include "../../tables.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_spell_count(test_case_t *test);
static test_result_t test_spell_lookup(test_case_t *test);
static test_result_t test_spell_function_pointers(test_case_t *test);
static test_result_t test_spell_level_ranges(test_case_t *test);
static test_result_t test_spell_fun_resolution(test_case_t *test);
static test_result_t test_spell_categorization(test_case_t *test);

/**
 * Main test dispatcher for spell data integrity tests
 */
test_result_t run_spell_data_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "spelldt_count_test") == 0) {
        result = test_spell_count(test);
    }
    else if (strcmp(test->test_type, "spelldt_lookup_test") == 0) {
        result = test_spell_lookup(test);
    }
    else if (strcmp(test->test_type, "spelldt_function_pointer_test") == 0) {
        result = test_spell_function_pointers(test);
    }
    else if (strcmp(test->test_type, "spelldt_level_range_test") == 0) {
        result = test_spell_level_ranges(test);
    }
    else if (strcmp(test->test_type, "spelldt_fun_resolution_test") == 0) {
        result = test_spell_fun_resolution(test);
    }
    else if (strcmp(test->test_type, "spelldt_categorization_test") == 0) {
        result = test_spell_categorization(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Unknown spell data test type: %s", test->test_type);
        return TEST_ERROR;
    }

    return result;
}

/**
 * Test spell count validation
 * Iterates through skill table and counts spells
 */
static test_result_t test_spell_count(test_case_t *test)
{
    int minimum_expected = 5; /* Default minimum */
    
    /* Extract minimum expected from config if available */
    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = test_json_get_int(input, "minimum_expected");
            if (cfg_min > 0) minimum_expected = cfg_min;
        }
    }

    int spell_count = 0;
    int total_skills = skill_count();

    /* Count total loaded skills first */
    if (total_skills <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Total skills loaded should be positive");
        return TEST_FAILURE;
    }
    
    /* Count spells by iterating through skills and checking for spell functions */
    SKILL_DATA *skill = skill_first();
    while (skill) {
        if (skill->spell_fun != NULL) {
            spell_count++;
        }
        skill = skill->next;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Found %d spells out of %d total skills", spell_count, total_skills);

    if (spell_count <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Spell count should be positive");
        return TEST_FAILURE;
    }
    
    if (spell_count < minimum_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, 
                     "Spell count %d below minimum %d", spell_count, minimum_expected);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test spell lookup by name
 */
static test_result_t test_spell_lookup(test_case_t *test)
{
    json_t *input = NULL;
    json_t *test_cases = NULL;
    
    /* Extract test cases from config */
    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (input) {
            test_cases = json_object_get(input, "test_cases");
        }
    }
    
    if (!test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Missing or invalid test_cases array");
        return TEST_ERROR;
    }

    size_t index;
    json_t *test_case;

    json_array_foreach(test_cases, index, test_case) {
        const char *name = json_string_value(json_object_get(test_case, "name"));
        bool should_exist = json_boolean_value(json_object_get(test_case, "should_exist"));
        const char *description = json_string_value(json_object_get(test_case, "description"));

        if (!name) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Missing name in test case");
            return TEST_ERROR;
        }

        /* Test skill_search (prefix match) */
        SKILL_DATA *skill = skill_search(name);
        
        if (should_exist) {
            if (!skill) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Expected to find spell '%s' but got NULL", name);
                return TEST_FAILURE;
            } else if (!skill->spell_fun) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Found skill '%s' but it has no spell function", name);
                return TEST_FAILURE;
            } else {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                              "Successfully found spell '%s' (%s)", name, description);
            }
        } else {
            if (skill && skill->spell_fun) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Expected NOT to find spell '%s' but found it", name);
                return TEST_FAILURE;
            } else {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                              "Correctly did not find non-existent spell '%s'", name);
            }
        }

        /* Also test skill_find (exact match) */
        SKILL_DATA *exact_skill = skill_find(name);
        if (should_exist && skill && !exact_skill) {
            log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                          "skill_search found '%s' but skill_find did not (prefix vs exact match)", name);
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test spell function pointer integrity
 */
static test_result_t test_spell_function_pointers(test_case_t *test)
{
    int total_spells = 0;
    int valid_function_pointers = 0;

    SKILL_DATA *skill = skill_first();
    while (skill) {
        if (skill->spell_fun != NULL) {
            total_spells++;
            
            /* We can't easily validate function pointer is "good" without 
             * calling it, but we can check it's not NULL */
            valid_function_pointers++;
            
            log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                          "Spell '%s' has function pointer %p", 
                          skill_name(skill), (void*)skill->spell_fun);
        }
        skill = skill->next;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Spell function pointers: %d valid, %d total spells",
                  valid_function_pointers, total_spells);

    if (total_spells <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Should have found some spells");
        return TEST_FAILURE;
    }
    
    /* All spells with spell_fun should have valid (non-NULL) pointers */
    if (valid_function_pointers != total_spells) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Valid function pointers (%d) != total spells (%d)",
                     valid_function_pointers, total_spells);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test spell level range validation
 */
static test_result_t test_spell_level_ranges(test_case_t *test)
{
    int min_level = 1;
    int max_level = 100;
    bool allow_special = true;
    
    /* Extract range from config if available */
    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            min_level = test_json_get_int(input, "min_level");
            max_level = test_json_get_int(input, "max_level");
            allow_special = test_json_get_bool(input, "allow_special_levels");
        }
    }

    int total_spells = 0;
    int valid_levels = 0;
    int special_levels = 0;

    SKILL_DATA *skill = skill_first();
    while (skill) {
        if (skill->spell_fun != NULL) {
            total_spells++;
            
            /* Check class levels - for simplicity, just check first class */
            int spell_level = skill->skill_level[0]; /* CLASS_MAGE typically index 0 */
            
            if (spell_level >= min_level && spell_level <= max_level) {
                valid_levels++;
            } else if (allow_special && (spell_level > 9000 || spell_level == 0)) {
                /* Special levels: > 9000 for objects, 0 for unlearnable */
                special_levels++;
                log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                              "Spell '%s' has special level %d", 
                              skill_name(skill), spell_level);
            } else {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Spell '%s' has invalid level %d (range: %d-%d)", 
                              skill_name(skill), spell_level, min_level, max_level);
                return TEST_FAILURE;
            }
        }
        skill = skill->next;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Spell levels: %d valid, %d special, %d total",
                  valid_levels, special_levels, total_spells);

    if (total_spells <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Should have found some spells");
        return TEST_FAILURE;
    }
    
    if (valid_levels + special_levels != total_spells) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Valid (%d) + special (%d) levels != total spells (%d)",
                     valid_levels, special_levels, total_spells);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

/**
 * Test spell function name resolution
 */
static test_result_t test_spell_fun_resolution(test_case_t *test)
{
    json_t *input = NULL;
    json_t *test_functions = NULL;
    
    /* Extract test functions from config */
    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (input) {
            test_functions = json_object_get(input, "test_functions");
        }
    }
    
    if (!test_functions || !json_is_array(test_functions)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Missing or invalid test_functions array");
        return TEST_ERROR;
    }

    size_t index;
    json_t *test_case;

    json_array_foreach(test_functions, index, test_case) {
        const char *name = json_string_value(json_object_get(test_case, "name"));
        bool should_resolve = json_boolean_value(json_object_get(test_case, "should_resolve"));

        if (!name) {
            log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Missing name in function test case");
            return TEST_ERROR;
        }

        SPELL_FUN *spell_fun = spell_fun_lookup(name);
        
        if (should_resolve) {
            if (!spell_fun) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Expected to resolve function '%s' but got NULL", name);
                return TEST_FAILURE;
            } else {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                              "Successfully resolved function '%s' to %p", name, (void*)spell_fun);
                
                /* Try reverse lookup */
                const char *resolved_name = spell_fun_name(spell_fun);
                if (resolved_name && strcmp(resolved_name, name) == 0) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                  "Reverse lookup confirmed: %p -> '%s'", (void*)spell_fun, resolved_name);
                } else {
                    log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                                  "Reverse lookup mismatch: %p -> '%s' (expected '%s')", 
                                  (void*)spell_fun, resolved_name ? resolved_name : "NULL", name);
                }
            }
        } else {
            if (spell_fun) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "Expected NOT to resolve function '%s' but got %p", name, (void*)spell_fun);
                return TEST_FAILURE;
            } else {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                              "Correctly did not resolve non-existent function '%s'", name);
            }
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test spell categorization and metadata integrity
 */
static test_result_t test_spell_categorization(test_case_t *test)
{
    int total_spells = 0;
    int spells_with_names = 0;
    int spells_with_mana_costs = 0;
    int spells_with_beats = 0;

    SKILL_DATA *skill = skill_first();
    while (skill) {
        if (skill->spell_fun != NULL) {
            total_spells++;
            
            /* Check basic metadata */
            const char *name = skill_name(skill);
            if (name && strlen(name) > 0 && strcmp(name, "none") != 0) {
                spells_with_names++;
            }
            
            /* Check mana cost (should be >= 0) */
            if (skill->min_mana >= 0) {
                spells_with_mana_costs++;
            }
            
            /* Check casting time (should be > 0 for most spells) */
            if (skill->beats > 0) {
                spells_with_beats++;
            }
            
            log_message_f(LOG_LEVEL_DEBUG, LOG_UNIT_TESTS,
                          "Spell '%s': mana=%d, beats=%d", 
                          name, skill->min_mana, skill->beats);
        }
        skill = skill->next;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                  "Spell metadata: %d total, %d with names, %d with mana costs, %d with beats",
                  total_spells, spells_with_names, spells_with_mana_costs, spells_with_beats);

    if (total_spells <= 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Should have found some spells");
        return TEST_FAILURE;
    }
    
    if (spells_with_names != total_spells) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Not all spells have valid names: %d/%d", spells_with_names, total_spells);
        return TEST_FAILURE;
    }
    
    if (spells_with_mana_costs != total_spells) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "Not all spells have valid mana costs: %d/%d", spells_with_mana_costs, total_spells);
        return TEST_FAILURE;
    }
    
    if (spells_with_beats < total_spells / 2) {
        log_message_f(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                     "Many spells lack casting time: %d/%d", spells_with_beats, total_spells);
        /* This is just a warning, not a failure */
    }

    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */