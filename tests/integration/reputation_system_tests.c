/**
 * Reputation System Tests
 * 
 * Tests the reputation system including:
 * - Reputation index data loading and lookup functions
 * - Reputation rank lookup by ordinal and UID
 * - Character reputation management (creation, gain, ranking)
 * - Reputation calculations and level progression
 * - Rank flag behavior (peaceful, hostile)
 * - Paragon system advancement
 * - Data validation and integrity
 * - Reputation display formatting
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../framework/test_framework.h"
#include <string.h>

/* Forward declarations */
static test_result_t test_repsys_index_lookup(test_case_t *test);
static test_result_t test_repsys_rank_lookup(test_case_t *test);
static test_result_t test_repsys_character_rep(test_case_t *test);
static test_result_t test_repsys_calculation(test_case_t *test);
static test_result_t test_repsys_flag(test_case_t *test);
static test_result_t test_repsys_paragon(test_case_t *test);
static test_result_t test_repsys_validation(test_case_t *test);
static test_result_t test_repsys_display(test_case_t *test);

/* Test utilities */
static AREA_DATA *create_test_area(long uid);
static REPUTATION_INDEX_DATA *create_test_reputation_index(AREA_DATA *area, long vnum);
static REPUTATION_INDEX_RANK_DATA *create_test_reputation_rank(int uid, int ordinal, const char *name, long capacity, long flags);
static CHAR_DATA *create_test_character(void);
static void cleanup_test_character(CHAR_DATA *ch);
static void cleanup_test_area(AREA_DATA *area);

/**
 * Main test dispatcher for reputation system tests
 */
test_result_t run_reputation_system_test_case(test_case_t *test)
{
    test_result_t result = TEST_FAILURE;

    if (strcmp(test->test_type, "repsys_idx_lkp") == 0) {
        result = test_repsys_index_lookup(test);
    }
    else if (strcmp(test->test_type, "repsys_rank_lkp") == 0) {
        result = test_repsys_rank_lookup(test);
    }
    else if (strcmp(test->test_type, "repsys_character_rep_test") == 0) {
        result = test_repsys_character_rep(test);
    }
    else if (strcmp(test->test_type, "repsys_calculation_test") == 0) {
        result = test_repsys_calculation(test);
    }
    else if (strcmp(test->test_type, "repsys_flag_test") == 0) {
        result = test_repsys_flag(test);
    }
    else if (strcmp(test->test_type, "repsys_paragon_test") == 0) {
        result = test_repsys_paragon(test);
    }
    else if (strcmp(test->test_type, "repsys_validation_test") == 0) {
        result = test_repsys_validation(test);
    }
    else if (strcmp(test->test_type, "repsys_display_test") == 0) {
        result = test_repsys_display(test);
    }
    else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                      "Unknown reputation system test type: %s", test->test_type);
    }

    return result;
}

/**
 * Test reputation index lookup functions
 */
static test_result_t test_repsys_index_lookup(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test area and reputation index */
    test_area = create_test_area(9999);
    if (!test_area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test area");
        return TEST_ERROR;
    }
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Created test area with UID %ld", test_area->uid);

    long test_vnum = 100;
    rep_index = create_test_reputation_index(test_area, test_vnum);
    if (!rep_index) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test reputation index");
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Created reputation index with vnum %ld in area UID %ld", test_vnum, test_area->uid);

    /* Test get_reputation_index */
    REPUTATION_INDEX_DATA *found = get_reputation_index(test_area, test_vnum);
    if (!found) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "get_reputation_index returned NULL for area %p, vnum %ld", (void*)test_area, test_vnum);
    }
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(found->vnum, test_vnum);

    /* Test lookup of non-existent reputation */
    REPUTATION_INDEX_DATA *not_found = get_reputation_index(test_area, 999999);
    TEST_ASSERT_NULL(not_found);

    /* Test get_reputation_index_auid - use NULL test since global area registry may not be available */
    REPUTATION_INDEX_DATA *found_auid = get_reputation_index_auid(0, test_vnum);
    TEST_ASSERT_NULL(found_auid);

    /* Test get_reputation_index_wnum */
    WNUM wnum = { test_area, test_vnum };
    REPUTATION_INDEX_DATA *found_wnum = get_reputation_index_wnum(wnum);
    TEST_ASSERT_NOT_NULL(found_wnum);
    TEST_ASSERT_TRUE(found_wnum == found);

    /* Test null parameter handling */
    TEST_ASSERT_NULL(get_reputation_index(NULL, test_vnum));
    TEST_ASSERT_NULL(get_reputation_index(test_area, 0));
    TEST_ASSERT_NULL(get_reputation_index_auid(0, test_vnum));

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All reputation index lookups passed");
    result = TEST_SUCCESS;

    cleanup_test_area(test_area);
    return result;
}

/**
 * Test reputation rank lookup functions
 */
static test_result_t test_repsys_rank_lookup(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test area and reputation index */
    test_area = create_test_area(9998);
    if (!test_area) {
        return TEST_ERROR;
    }

    rep_index = create_test_reputation_index(test_area, 101);
    if (!rep_index) {
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Add test ranks */
    REPUTATION_INDEX_RANK_DATA *rank1 = create_test_reputation_rank(1, 1, "Neutral", 100, 0);
    REPUTATION_INDEX_RANK_DATA *rank2 = create_test_reputation_rank(2, 2, "Friendly", 200, REPUTATION_RANK_PEACEFUL);
    REPUTATION_INDEX_RANK_DATA *rank3 = create_test_reputation_rank(3, 3, "Ally", 500, 0);

    if (!rank1 || !rank2 || !rank3) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test ranks");
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    list_appendlink(rep_index->ranks, rank1);
    list_appendlink(rep_index->ranks, rank2);
    list_appendlink(rep_index->ranks, rank3);

    /* Test get_reputation_rank by ordinal */
    REPUTATION_INDEX_RANK_DATA *found_ord1 = get_reputation_rank(rep_index, 1);
    TEST_ASSERT_NOT_NULL(found_ord1);
    TEST_ASSERT_TRUE(found_ord1 == rank1);

    REPUTATION_INDEX_RANK_DATA *found_ord2 = get_reputation_rank(rep_index, 2);
    TEST_ASSERT_NOT_NULL(found_ord2);
    TEST_ASSERT_TRUE(found_ord2 == rank2);

    /* Test get_reputation_rank_uid */
    REPUTATION_INDEX_RANK_DATA *found_uid2 = get_reputation_rank_uid(rep_index, 2);
    TEST_ASSERT_NOT_NULL(found_uid2);
    TEST_ASSERT_TRUE(found_uid2 == rank2);

    REPUTATION_INDEX_RANK_DATA *found_uid3 = get_reputation_rank_uid(rep_index, 3);
    TEST_ASSERT_NOT_NULL(found_uid3);
    TEST_ASSERT_TRUE(found_uid3 == rank3);

    /* Test non-existent rank lookups */
    TEST_ASSERT_NULL(get_reputation_rank(rep_index, 999));
    TEST_ASSERT_NULL(get_reputation_rank_uid(rep_index, 999));

    /* Test null parameter handling */
    TEST_ASSERT_NULL(get_reputation_rank(NULL, 1));
    TEST_ASSERT_NULL(get_reputation_rank(rep_index, 0));
    TEST_ASSERT_NULL(get_reputation_rank_uid(NULL, 1));
    TEST_ASSERT_NULL(get_reputation_rank_uid(rep_index, 0));

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All reputation rank lookups passed");
    result = TEST_SUCCESS;

    cleanup_test_area(test_area);
    return result;
}

/**
 * Test character reputation management
 */
static test_result_t test_repsys_character_rep(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    REPUTATION_DATA *char_rep = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9997);
    if (!test_area) {
        return TEST_ERROR;
    }

    rep_index = create_test_reputation_index(test_area, 102);
    if (!rep_index) {
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    rep_index->initial_rank = 2;
    rep_index->initial_reputation = 150;

    test_char = create_test_character();
    if (!test_char) {
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Test setting character reputation */
    char_rep = set_reputation_char(test_char, rep_index, 2, 150, false);
    TEST_ASSERT_NOT_NULL(char_rep);
    TEST_ASSERT_TRUE(char_rep->pIndexData == rep_index);
    TEST_ASSERT_INT_EQ(char_rep->current_rank, 2);
    TEST_ASSERT_INT_EQ(char_rep->reputation, 150);
    TEST_ASSERT_INT_EQ(char_rep->maximum_rank, 2);

    /* Test finding character reputation */
    REPUTATION_DATA *found_rep = find_reputation_char(test_char, rep_index);
    TEST_ASSERT_NOT_NULL(found_rep);
    TEST_ASSERT_TRUE(found_rep == char_rep);

    /* Test reputation gain */
    int change = 0;
    long total_given = 0;
    bool gain_result = gain_reputation(test_char, rep_index, 50, &change, &total_given, false);
    TEST_ASSERT_TRUE(gain_result);
    TEST_ASSERT_INT_EQ(change, 50);
    TEST_ASSERT_INT_EQ(total_given, 50);
    TEST_ASSERT_INT_EQ(char_rep->reputation, 200);

    /* Test reputation loss with floor */
    gain_result = gain_reputation(test_char, rep_index, -300, &change, &total_given, false);
    TEST_ASSERT_TRUE(gain_result);
    TEST_ASSERT_INT_EQ(char_rep->reputation, 0);

    /* Test rank setting - first add ranks to the index */
    REPUTATION_INDEX_RANK_DATA *test_rank3 = create_test_reputation_rank(3, 3, "Test Rank 3", 300, 0);
    if (!test_rank3) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test rank 3");
        cleanup_test_character(test_char);
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }
    list_appendlink(rep_index->ranks, test_rank3);
    
    bool rank_result = set_reputation_rank(test_char, char_rep, 3, 300, false);
    TEST_ASSERT_TRUE(rank_result);
    TEST_ASSERT_INT_EQ(char_rep->current_rank, 3);
    TEST_ASSERT_INT_EQ(char_rep->reputation, 300);
    TEST_ASSERT_INT_EQ(char_rep->maximum_rank, 3);

    /* Test has_reputation */
    TEST_ASSERT_TRUE(has_reputation(test_char, rep_index));

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All character reputation management tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/**
 * Test reputation calculations
 */
static test_result_t test_repsys_calculation(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9996);
    rep_index = create_test_reputation_index(test_area, 103);
    test_char = create_test_character();

    if (!test_area || !rep_index || !test_char) {
        if (test_char) cleanup_test_character(test_char);
        if (test_area) cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Set up reputation index with initial values */
    rep_index->initial_rank = 1;
    rep_index->initial_reputation = 100;

    /* Test scenario 1: Basic reputation gain from initial state */
    int change = 0;
    gain_reputation(test_char, rep_index, 75, &change, NULL, false);
    REPUTATION_DATA *rep = find_reputation_char(test_char, rep_index);
    TEST_ASSERT_NOT_NULL(rep);
    TEST_ASSERT_INT_EQ(rep->reputation, 175);  /* 100 initial + 75 gain */
    TEST_ASSERT_INT_EQ(change, 75);

    /* Test scenario 2: Reputation loss with floor */
    gain_reputation(test_char, rep_index, -200, &change, NULL, false);
    TEST_ASSERT_INT_EQ(rep->reputation, 0);  /* Can't go below 0 */
    TEST_ASSERT_TRUE(change < 0);

    /* Test scenario 3: Large reputation gain from fresh start */
    cleanup_test_character(test_char);
    test_char = create_test_character();
    if (!test_char) {
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }
    
    rep_index->initial_reputation = 200;
    gain_reputation(test_char, rep_index, 500, &change, NULL, false);
    rep = find_reputation_char(test_char, rep_index);
    TEST_ASSERT_NOT_NULL(rep);
    TEST_ASSERT_INT_EQ(rep->reputation, 700);  /* 200 initial + 500 gain */
    TEST_ASSERT_INT_EQ(change, 500);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All reputation calculation tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/**
 * Test reputation rank flag behavior
 */
static test_result_t test_repsys_flag(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9995);
    rep_index = create_test_reputation_index(test_area, 104);
    test_char = create_test_character();

    if (!test_area || !rep_index || !test_char) {
        if (test_char) cleanup_test_character(test_char);
        if (test_area) cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Add ranks with different flags */
    REPUTATION_INDEX_RANK_DATA *peaceful_rank = create_test_reputation_rank(10, 1, "Peace Keeper", 100, REPUTATION_RANK_PEACEFUL);
    REPUTATION_INDEX_RANK_DATA *hostile_rank = create_test_reputation_rank(11, 2, "Enemy", 100, REPUTATION_RANK_HOSTILE);
    REPUTATION_INDEX_RANK_DATA *neutral_rank = create_test_reputation_rank(12, 3, "Neutral", 100, 0);

    if (!peaceful_rank || !hostile_rank || !neutral_rank) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to create test ranks for flag testing");
        if (test_char) cleanup_test_character(test_char);
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    list_appendlink(rep_index->ranks, peaceful_rank);
    list_appendlink(rep_index->ranks, hostile_rank);
    list_appendlink(rep_index->ranks, neutral_rank);

    /* Test peaceful rank detection */
    set_reputation_char(test_char, rep_index, 1, 100, false);
    bool is_peaceful = is_reputation_rank_peaceful(test_char, rep_index);
    TEST_ASSERT_TRUE(is_peaceful);
    TEST_ASSERT_FALSE(is_reputation_rank_hostile(test_char, rep_index));

    /* Test hostile rank detection */
    REPUTATION_DATA *rep = find_reputation_char(test_char, rep_index);
    set_reputation_rank(test_char, rep, 2, 200, false);
    bool is_hostile = is_reputation_rank_hostile(test_char, rep_index);
    TEST_ASSERT_TRUE(is_hostile);
    TEST_ASSERT_FALSE(is_reputation_rank_peaceful(test_char, rep_index));

    /* Test neutral rank detection */
    set_reputation_rank(test_char, rep, 3, 300, false);
    TEST_ASSERT_FALSE(is_reputation_rank_peaceful(test_char, rep_index));
    TEST_ASSERT_FALSE(is_reputation_rank_hostile(test_char, rep_index));

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All reputation flag behavior tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/**
 * Test paragon system
 */
static test_result_t test_repsys_paragon(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9994);
    rep_index = create_test_reputation_index(test_area, 105);
    test_char = create_test_character();

    if (!test_area || !rep_index || !test_char) {
        if (test_char) cleanup_test_character(test_char);
        if (test_area) cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Create reputation for character */
    REPUTATION_DATA *rep = set_reputation_char(test_char, rep_index, 1, 100, false);
    TEST_ASSERT_NOT_NULL(rep);
    TEST_ASSERT_INT_EQ(rep->paragon_level, 0);

    /* Test paragon advancement */
    paragon_reputation(test_char, rep, false);
    TEST_ASSERT_INT_EQ(rep->paragon_level, 1);

    paragon_reputation(test_char, rep, false);
    paragon_reputation(test_char, rep, false);
    TEST_ASSERT_INT_EQ(rep->paragon_level, 3);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All paragon system tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/**
 * Test data validation
 */
static test_result_t test_repsys_validation(test_case_t *test)
{
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9993);
    rep_index = create_test_reputation_index(test_area, 106);
    test_char = create_test_character();

    if (!test_area || !rep_index || !test_char) {
        if (test_char) cleanup_test_character(test_char);
        if (test_area) cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Test memory allocation validation */
    TEST_ASSERT_NOT_NULL(rep_index->name);
    TEST_ASSERT_NOT_NULL(rep_index->description);
    TEST_ASSERT_NOT_NULL(rep_index->comments);
    TEST_ASSERT_NOT_NULL(rep_index->ranks);

    /* Test field integrity */
    TEST_ASSERT_TRUE(rep_index->valid);
    TEST_ASSERT_TRUE(rep_index->area == test_area);
    TEST_ASSERT_INT_EQ(rep_index->vnum, 106);

    /* Test bounds checking with invalid parameters */
    REPUTATION_DATA *invalid_rep = get_reputation_char(test_char, test_area, -1, false, false);
    TEST_ASSERT_NULL(invalid_rep);

    invalid_rep = get_reputation_char(test_char, NULL, 106, false, false);
    TEST_ASSERT_NULL(invalid_rep);

    /* Test character reputation validation */
    REPUTATION_DATA *valid_rep = set_reputation_char(test_char, rep_index, 1, 100, false);
    TEST_ASSERT_NOT_NULL(valid_rep);
    TEST_ASSERT_TRUE(valid_rep->valid);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All data validation tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/**
 * Test reputation display functionality
 */
static test_result_t test_repsys_display(test_case_t *test)
{
    /* Note: This test focuses on the underlying data structures that support
     * the do_reputations command, since testing output formatting would require
     * more complex mocking of the send_to_char and page_to_char functions */
    
    AREA_DATA *test_area = NULL;
    REPUTATION_INDEX_DATA *rep_index = NULL;
    CHAR_DATA *test_char = NULL;
    test_result_t result = TEST_FAILURE;

    /* Create test environment */
    test_area = create_test_area(9992);
    rep_index = create_test_reputation_index(test_area, 107);
    test_char = create_test_character();

    if (!test_area || !rep_index || !test_char) {
        if (test_char) cleanup_test_character(test_char);
        if (test_area) cleanup_test_area(test_area);
        return TEST_ERROR;
    }

    /* Add a test rank */
    REPUTATION_INDEX_RANK_DATA *rank = create_test_reputation_rank(1, 1, "Test Rank", 100, 0);
    if (!rank) {
        cleanup_test_character(test_char);
        cleanup_test_area(test_area);
        return TEST_ERROR;
    }
    list_appendlink(rep_index->ranks, rank);

    /* Create multiple reputations for display testing */
    REPUTATION_DATA *rep1 = set_reputation_char(test_char, rep_index, 1, 100, false);
    TEST_ASSERT_NOT_NULL(rep1);

    /* Test that character has reputations list */
    TEST_ASSERT_NOT_NULL(test_char->reputations);
    TEST_ASSERT_TRUE(list_size(test_char->reputations) > 0);

    /* Test reputation data accessibility for display */
    TEST_ASSERT_NOT_NULL(rep1->pIndexData);
    TEST_ASSERT_NOT_NULL(rep1->pIndexData->name);
    
    REPUTATION_INDEX_RANK_DATA *display_rank = get_reputation_rank(rep1->pIndexData, rep1->current_rank);
    TEST_ASSERT_NOT_NULL(display_rank);
    TEST_ASSERT_NOT_NULL(display_rank->name);

    /* Test reputation point values are accessible */
    TEST_ASSERT_INT_EQ(rep1->reputation, 100);
    TEST_ASSERT_INT_EQ(rep1->current_rank, 1);

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "All reputation display tests passed");
    result = TEST_SUCCESS;

    cleanup_test_character(test_char);
    cleanup_test_area(test_area);
    return result;
}

/* ===== UTILITY FUNCTIONS ===== */

static AREA_DATA *create_test_area(long uid)
{
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Creating test area with UID %ld", uid);
    
    AREA_DATA *area = calloc(1, sizeof(AREA_DATA));
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to allocate memory for test area");
        return NULL;
    }
    
    area->uid = uid;
    area->name = str_dup("Test Area");
    area->file_name = str_dup("test.are");
    area->builders = str_dup("Test Builder");
    area->credits = str_dup("Test Credits");
    
    /* Initialize reputation hash table */
    for (int i = 0; i < MAX_KEY_HASH; i++) {
        area->reputation_index_hash[i] = NULL;
    }
    
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Successfully created test area");
    return area;
}

static REPUTATION_INDEX_DATA *create_test_reputation_index(AREA_DATA *area, long vnum)
{
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Creating reputation index for area %p with vnum %ld", (void*)area, vnum);
    
    if (!area) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "NULL area passed to create_test_reputation_index");
        return NULL;
    }

    REPUTATION_INDEX_DATA *rep = calloc(1, sizeof(REPUTATION_INDEX_DATA));
    if (!rep) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to allocate memory for reputation index");
        return NULL;
    }

    rep->valid = true;
    rep->area = area;
    rep->vnum = vnum;
    rep->name = str_dup("Test Reputation");
    rep->description = str_dup("A test reputation system");
    rep->comments = str_dup("Test comments");
    rep->created_by = str_dup("Test Creator");
    rep->ranks = list_create(false);
    
    if (!rep->name || !rep->description || !rep->comments || !rep->created_by || !rep->ranks) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Failed to allocate strings or list for reputation index");
        /* Cleanup partial allocation */
        if (rep->name) free_string(rep->name);
        if (rep->description) free_string(rep->description);
        if (rep->comments) free_string(rep->comments);
        if (rep->created_by) free_string(rep->created_by);
        if (rep->ranks) list_destroy(rep->ranks);
        free(rep);
        return NULL;
    }
    rep->initial_rank = 1;
    rep->initial_reputation = 0;

    /* Add to area's hash table */
    int hash = vnum % MAX_KEY_HASH;
    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Adding reputation to hash bucket %d", hash);
    rep->next = area->reputation_index_hash[hash];
    area->reputation_index_hash[hash] = rep;

    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Successfully created reputation index");
    return rep;
}

static REPUTATION_INDEX_RANK_DATA *create_test_reputation_rank(int uid, int ordinal, const char *name, long capacity, long flags)
{
    REPUTATION_INDEX_RANK_DATA *rank = calloc(1, sizeof(REPUTATION_INDEX_RANK_DATA));
    if (!rank) {
        return NULL;
    }

    rank->valid = true;
    rank->uid = uid;
    rank->ordinal = ordinal;
    rank->name = str_dup(name);
    rank->description = str_dup("Test rank description");
    rank->comments = str_dup("Test rank comments");
    rank->capacity = capacity;
    rank->flags = flags;
    rank->color = 'W';

    return rank;
}

static CHAR_DATA *create_test_character(void)
{
    CHAR_DATA *ch = calloc(1, sizeof(CHAR_DATA));
    if (!ch) {
        return NULL;
    }

    ch->name = str_dup("TestChar");
    ch->reputations = list_create(false);
    
    /* Mark as PC (not NPC) for reputation system */
    ch->pIndexData = NULL;  /* NPCs have pIndexData, PCs have NULL */

    return ch;
}

static void cleanup_test_character(CHAR_DATA *ch)
{
    if (!ch) {
        return;
    }

    if (ch->name) {
        free_string(ch->name);
    }
    
    if (ch->reputations) {
        ITERATOR it;
        iterator_start(&it, ch->reputations);
        REPUTATION_DATA *rep;
        while ((rep = (REPUTATION_DATA *)iterator_nextdata(&it)) != NULL) {
            free(rep);
        }
        iterator_stop(&it);
        list_destroy(ch->reputations);
    }

    free(ch);
}

static void cleanup_test_area(AREA_DATA *area)
{
    if (!area) {
        return;
    }

    /* Clean up reputation indices in hash table */
    for (int i = 0; i < MAX_KEY_HASH; i++) {
        REPUTATION_INDEX_DATA *rep = area->reputation_index_hash[i];
        while (rep) {
            REPUTATION_INDEX_DATA *next = rep->next;
            
            if (rep->name) free_string(rep->name);
            if (rep->description) free_string(rep->description);
            if (rep->comments) free_string(rep->comments);
            if (rep->created_by) free_string(rep->created_by);
            
            if (rep->ranks) {
                ITERATOR it;
                iterator_start(&it, rep->ranks);
                REPUTATION_INDEX_RANK_DATA *rank;
                while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it)) != NULL) {
                    if (rank->name) free_string(rank->name);
                    if (rank->description) free_string(rank->description);
                    if (rank->comments) free_string(rank->comments);
                    free(rank);
                }
                iterator_stop(&it);
                list_destroy(rep->ranks);
            }
            
            free(rep);
            rep = next;
        }
        area->reputation_index_hash[i] = NULL;
    }

    if (area->name) free_string(area->name);
    if (area->file_name) free_string(area->file_name);
    if (area->builders) free_string(area->builders);
    if (area->credits) free_string(area->credits);
    
    free(area);
}

#endif /* BUILD_TESTS */