#ifndef TEST_MODULES_H
#define TEST_MODULES_H

#ifdef BUILD_TESTS

#include "test_framework.h"

/* Match modes for the handler dispatch table. */
typedef enum {
    MATCH_EXACT,    /* strcmp: test_type must match pattern exactly */
    MATCH_SUBSTR    /* strstr: test_type must contain pattern as a substring */
} test_match_mode_t;

/* Entry in the handler dispatch table. */
typedef struct {
    const char *pattern;
    test_result_t (*handler)(test_case_t *);
    test_match_mode_t match_mode;
} test_handler_entry_t;

test_result_t run_wnum_test_case(test_case_t *test);
test_result_t run_pure_function_test_case(test_case_t *test);
test_result_t run_buffer_function_test_case(test_case_t *test);
test_result_t run_memory_util_test_case(test_case_t *test);
test_result_t run_utf8_test_case(test_case_t *test);
test_result_t run_string_editor_test_case(test_case_t *test);

test_result_t run_reset_test_case(test_case_t *test);
test_result_t run_shop_stock_test_case(test_case_t *test);
test_result_t run_church_test_case(test_case_t *test);
// test_result_t run_instance_test_case(test_case_t *test);  // Temporarily disabled
test_result_t run_chat_room_test_case(test_case_t *test);
test_result_t run_skill_data_test_case(test_case_t *test);
test_result_t run_spell_data_test_case(test_case_t *test);
test_result_t run_class_data_test_case(test_case_t *test);
test_result_t run_item_type_test_case(test_case_t *test);
test_result_t run_lookup_table_test_case(test_case_t *test);
test_result_t run_song_data_test_case(test_case_t *test);
test_result_t run_skill_group_test_case(test_case_t *test);
test_result_t run_trait_system_test_case(test_case_t *test);
test_result_t run_script_engine_test_case(test_case_t *test);
test_result_t run_channel_pubsub_test_case(test_case_t *test);
test_result_t run_combat_telemetry_test_case(test_case_t *test);
test_result_t run_constants_tables_test_case(test_case_t *test);
test_result_t run_command_table_test_case(test_case_t *test);
test_result_t run_handler_function_test_case(test_case_t *test);
test_result_t run_combat_math_test_case(test_case_t *test);
test_result_t run_quest_system_test_case(test_case_t *test);
test_result_t run_reputation_system_test_case(test_case_t *test);
test_result_t run_olc_framework_test_case(test_case_t *test);
test_result_t run_wilderness_system_test_case(test_case_t *test);
test_result_t run_update_cycle_test_case(test_case_t *test);
test_result_t run_array_test_case(test_case_t *test);
test_result_t run_strdict_test_case(test_case_t *test);

#endif /* BUILD_TESTS */

#endif /* TEST_MODULES_H */
