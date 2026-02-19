#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#ifdef BUILD_TESTS

#include "test_framework.h"

bool test_utils_create_fake_player(CHAR_DATA **out_ch, DESCRIPTOR_DATA **out_desc);
void test_utils_destroy_fake_player(CHAR_DATA *ch, DESCRIPTOR_DATA *desc);

#endif /* BUILD_TESTS */

#endif /* TEST_UTILS_H */
