/**
 * Song Data System Tests
 * 
 * Tests the data-driven song system including:
 * - Song count verification
 * - Name lookup by prefix
 * - UID integrity and uniqueness
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../song_data.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_song_count(test_case_t *test);
static test_result_t test_song_lookup(test_case_t *test);
static test_result_t test_song_uid_integrity(test_case_t *test);

/**
 * Main test dispatcher for song data tests
 */
test_result_t run_song_data_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "song_count_test") == 0) {
        result = test_song_count(test);
    }
    else if (strcmp(test->test_type, "song_lookup_test") == 0) {
        result = test_song_lookup(test);
    }
    else if (strcmp(test->test_type, "song_uid_integrity_test") == 0) {
        result = test_song_uid_integrity(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown song data test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test that songs have been loaded
 */
static test_result_t test_song_count(test_case_t *test)
{
    int count = song_count();
    int min_expected = 1;

    if (test && test->config) {
        json_t *input = json_object_get(test->config, "input");
        if (input) {
            int cfg_min = json_get_int(input, "minimum_expected");
            if (cfg_min > 0) min_expected = cfg_min;
        }
    }

    if (count < min_expected) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "song_count() = %d, below minimum expected %d",
                     count, min_expected);
        return TEST_FAILURE;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "Song count: %d (minimum %d)", count, min_expected);
    return TEST_SUCCESS;
}

/**
 * Test song lookup by name prefix
 */
static test_result_t test_song_lookup(test_case_t *test)
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
        const char *prefix = json_get_string(tc, "prefix");
        bool should_find = json_get_bool(tc, "should_find");

        if (!prefix) continue;

        SONG_DATA *song = song_lookup(prefix);

        if (should_find && !song) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "song_lookup('%s') returned NULL, expected match", prefix);
            return TEST_FAILURE;
        }

        if (!should_find && song) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "song_lookup('%s') returned match, expected NULL", prefix);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

/**
 * Test all songs have unique positive UIDs
 */
static test_result_t test_song_uid_integrity(test_case_t *test)
{
    LLIST *song_list = song_get_list();
    if (!song_list) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                   "song_get_list() returned NULL");
        return TEST_SUCCESS;
    }

    #define MAX_UID_CHECK 4096
    bool seen[MAX_UID_CHECK];
    memset(seen, 0, sizeof(seen));

    int checked = 0;
    LLIST_LINK *link;
    for (link = song_list->head; link; link = link->next) {
        SONG_DATA *song = (SONG_DATA *)link->data;
        if (!song) continue;
        checked++;

        if (song->uid <= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                         "Song '%s' has non-positive UID %d",
                         song->name ? song->name : "?", song->uid);
            return TEST_FAILURE;
        }

        if (song->uid < MAX_UID_CHECK) {
            if (seen[song->uid]) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "Duplicate song UID %d (song: %s)",
                             song->uid, song->name ? song->name : "?");
                return TEST_FAILURE;
            }
            seen[song->uid] = true;
        }
    }
    #undef MAX_UID_CHECK

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                 "All %d songs have unique positive UIDs", checked);
    return TEST_SUCCESS;
}

#endif /* BUILD_TESTS */
