#ifndef TEST_MODULES_H
#define TEST_MODULES_H

#ifdef BUILD_TESTS

#include "test_framework.h"

test_result_t run_wnum_test_case(test_case_t *test);
test_result_t run_pure_function_test_case(test_case_t *test);
test_result_t run_string_editor_test_case(test_case_t *test);

test_result_t run_reset_test_case(test_case_t *test);
test_result_t run_shop_stock_test_case(test_case_t *test);
test_result_t run_church_test_case(test_case_t *test);
test_result_t run_instance_test_case(test_case_t *test);
test_result_t run_chat_room_test_case(test_case_t *test);
test_result_t run_skill_data_test_case(test_case_t *test);
test_result_t run_class_data_test_case(test_case_t *test);
test_result_t run_item_type_test_case(test_case_t *test);
test_result_t run_lookup_table_test_case(test_case_t *test);
test_result_t run_song_data_test_case(test_case_t *test);
test_result_t run_skill_group_test_case(test_case_t *test);
test_result_t run_trait_system_test_case(test_case_t *test);
test_result_t run_script_engine_test_case(test_case_t *test);
test_result_t run_channel_pubsub_test_case(test_case_t *test);

#endif /* BUILD_TESTS */

#endif /* TEST_MODULES_H */
