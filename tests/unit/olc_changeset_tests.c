#ifdef BUILD_TESTS

#include <string.h>

#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../editors/common/olc_changeset.h"

static test_result_t test_olccs_create_destroy(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "TestBuilder");

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_INT_EQ(ED_ROOM, cs->editor_type);
    TEST_ASSERT_INT_EQ(1, cs->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(100, cs->entity_wnum.vnum);
    TEST_ASSERT_STR_EQ("Test Room", cs->entity_label);
    TEST_ASSERT_STR_EQ("TestBuilder", cs->author);
    TEST_ASSERT_NOT_NULL(cs->changes);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));
    TEST_ASSERT_FALSE(cs->is_dirty);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_add_find_remove(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 200 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    json_t *old_val = json_string("old name");
    json_t *new_val = json_string("new name");

    olc_pending_change_t *change = olc_changeset_add_change(cs, "name",
                                                             OLC_FIELD_STRING,
                                                             old_val, new_val);
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    TEST_ASSERT_TRUE(cs->is_dirty);

    /* Find it */
    olc_pending_change_t *found = olc_changeset_find_change(cs, "name");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("name", found->field_path);
    TEST_ASSERT_INT_EQ(OLC_FIELD_STRING, found->field_type);

    /* Miss */
    TEST_ASSERT_NULL(olc_changeset_find_change(cs, "nonexistent"));

    /* Remove it */
    TEST_ASSERT_TRUE(olc_changeset_remove_change(cs, "name"));
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));
    TEST_ASSERT_NULL(olc_changeset_find_change(cs, "name"));

    /* Remove nonexistent returns false */
    TEST_ASSERT_FALSE(olc_changeset_remove_change(cs, "name"));

    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_update_in_place(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 2, .vnum = 300 };
    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Builder");

    json_t *original = json_string("original");
    json_t *first_edit = json_string("first edit");
    json_t *second_edit = json_string("second edit");

    /* Initial change */
    olc_changeset_add_change(cs, "short_descr", OLC_FIELD_STRING, original, first_edit);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Update same field — old_value should be preserved as original */
    olc_pending_change_t *updated = olc_changeset_add_change(cs, "short_descr",
                                                              OLC_FIELD_STRING,
                                                              first_edit, second_edit);
    TEST_ASSERT_NOT_NULL(updated);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Verify original old_value is preserved */
    const char *old_str = json_string_value(updated->old_value);
    const char *new_str = json_string_value(updated->new_value);
    TEST_ASSERT_STR_EQ("original", old_str);
    TEST_ASSERT_STR_EQ("second edit", new_str);

    json_decref(original);
    json_decref(first_edit);
    json_decref(second_edit);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_noop_detection(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 3, .vnum = 400 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    json_t *val_a = json_string("value A");
    json_t *val_b = json_string("value B");
    json_t *val_a2 = json_string("value A");

    /* Same old/new = immediate no-op */
    olc_pending_change_t *result = olc_changeset_add_change(cs, "field1",
                                                             OLC_FIELD_STRING,
                                                             val_a, val_a2);
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    /* Add a real change */
    result = olc_changeset_add_change(cs, "field2", OLC_FIELD_STRING, val_a, val_b);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Update back to original = collapse to no-op */
    result = olc_changeset_add_change(cs, "field2", OLC_FIELD_STRING, val_b, val_a2);
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(val_a);
    json_decref(val_b);
    json_decref(val_a2);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_clear(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 4, .vnum = 500 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    json_t *old_val = json_string("old");
    json_t *new_val = json_string("new");

    olc_changeset_add_change(cs, "field1", OLC_FIELD_STRING, old_val, new_val);
    olc_changeset_add_change(cs, "field2", OLC_FIELD_INT, old_val, new_val);
    olc_changeset_add_change(cs, "field3", OLC_FIELD_BOOL, old_val, new_val);
    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    olc_changeset_clear(cs);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));
    TEST_ASSERT_FALSE(cs->is_dirty);

    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_safety_limit(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 600 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    json_t *old_val = json_string("old");
    json_t *new_val = json_string("new");

    /* Fill to the limit */
    char field_buf[64];
    for (int i = 0; i < OLC_MAX_PENDING_PER_ENTITY; i++) {
        snprintf(field_buf, sizeof(field_buf), "field_%d", i);
        olc_pending_change_t *change = olc_changeset_add_change(cs, field_buf,
                                                                 OLC_FIELD_STRING,
                                                                 old_val, new_val);
        TEST_ASSERT_NOT_NULL(change);
    }

    TEST_ASSERT_INT_EQ(OLC_MAX_PENDING_PER_ENTITY, olc_changeset_count(cs));

    /* Next add should fail */
    olc_pending_change_t *overflow = olc_changeset_add_change(cs, "overflow_field",
                                                               OLC_FIELD_STRING,
                                                               old_val, new_val);
    TEST_ASSERT_NULL(overflow);
    TEST_ASSERT_INT_EQ(OLC_MAX_PENDING_PER_ENTITY, olc_changeset_count(cs));

    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_edit_state_lifecycle(test_case_t *test)
{
    olc_edit_state_t *state = olc_edit_state_create();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->active_changesets);
    TEST_ASSERT_INT_EQ(0, olc_edit_state_total_pending(state));

    /* Create and add a changeset */
    WNUM_LOAD wnum1 = { .auid = 10, .vnum = 1000 };
    olc_changeset_t *cs1 = olc_changeset_create(ED_ROOM, wnum1, "Room 1000", "Builder");
    list_addlink(state->active_changesets, cs1);

    /* Add some changes to it */
    json_t *old_val = json_string("old");
    json_t *new_val = json_string("new");
    olc_changeset_add_change(cs1, "name", OLC_FIELD_STRING, old_val, new_val);
    olc_changeset_add_change(cs1, "desc", OLC_FIELD_STRING, old_val, new_val);

    TEST_ASSERT_INT_EQ(2, olc_edit_state_total_pending(state));

    /* Find it */
    olc_changeset_t *found = olc_edit_state_find_changeset(state, ED_ROOM, wnum1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(ED_ROOM, found->editor_type);

    /* Miss on different editor type */
    TEST_ASSERT_NULL(olc_edit_state_find_changeset(state, ED_MOBILE, wnum1));

    /* Miss on different wnum */
    WNUM_LOAD wnum2 = { .auid = 10, .vnum = 9999 };
    TEST_ASSERT_NULL(olc_edit_state_find_changeset(state, ED_ROOM, wnum2));

    /* Add a second changeset */
    olc_changeset_t *cs2 = olc_changeset_create(ED_MOBILE, wnum2, "Mob 9999", "Builder");
    list_addlink(state->active_changesets, cs2);
    olc_changeset_add_change(cs2, "level", OLC_FIELD_INT, old_val, new_val);

    TEST_ASSERT_INT_EQ(3, olc_edit_state_total_pending(state));

    json_decref(old_val);
    json_decref(new_val);
    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_numeric_types(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 6, .vnum = 700 };
    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Builder");

    /* Integer field */
    json_t *old_int = json_integer(5);
    json_t *new_int = json_integer(10);
    olc_pending_change_t *change = olc_changeset_add_change(cs, "level",
                                                             OLC_FIELD_INT,
                                                             old_int, new_int);
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_INT_EQ(OLC_FIELD_INT, change->field_type);
    TEST_ASSERT_INT_EQ(5, json_integer_value(change->old_value));
    TEST_ASSERT_INT_EQ(10, json_integer_value(change->new_value));

    /* Boolean field */
    json_t *old_bool = json_false();
    json_t *new_bool = json_true();
    change = olc_changeset_add_change(cs, "is_aggressive", OLC_FIELD_BOOL,
                                       old_bool, new_bool);
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_INT_EQ(OLC_FIELD_BOOL, change->field_type);
    TEST_ASSERT_TRUE(json_is_false(change->old_value));
    TEST_ASSERT_TRUE(json_is_true(change->new_value));

    /* Flags field */
    json_t *old_flags = json_integer(0x01);
    json_t *new_flags = json_integer(0x05);
    change = olc_changeset_add_change(cs, "act_flags", OLC_FIELD_FLAGS,
                                       old_flags, new_flags);
    TEST_ASSERT_NOT_NULL(change);
    TEST_ASSERT_INT_EQ(OLC_FIELD_FLAGS, change->field_type);
    TEST_ASSERT_INT_EQ(0x01, json_integer_value(change->old_value));
    TEST_ASSERT_INT_EQ(0x05, json_integer_value(change->new_value));

    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    json_decref(old_int);
    json_decref(new_int);
    json_decref(old_bool);
    json_decref(new_bool);
    json_decref(old_flags);
    json_decref(new_flags);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/* ========================================================================
 * List operation collapsing tests
 * ======================================================================== */

static test_result_t test_list_add_then_remove(test_case_t *test)
{
    /* ADD then REMOVE for same item → collapse to no-op, purge both */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *item = json_object();
    json_object_set_new(item, "keyword", json_string("statue"));
    json_object_set_new(item, "description", json_string("A stone statue."));

    /* ADD */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, item);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* REMOVE same → should collapse to no-op */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, item, NULL);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(item);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_list_add_then_update(test_case_t *test)
{
    /* ADD then UPDATE for same item → collapse to ADD with updated value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3002 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *item_v1 = json_string("A stone statue.");
    json_t *item_v2 = json_string("A marble statue.");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, item_v1);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* UPDATE same → should remain ADD with updated new_value */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, item_v1, item_v2);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_ADD, found->field_type);
    TEST_ASSERT_NULL(found->old_value);  /* ADD has null old_value */
    TEST_ASSERT_STR_EQ("A marble statue.", json_string_value(found->new_value));

    json_decref(item_v1);
    json_decref(item_v2);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_list_remove_then_add(test_case_t *test)
{
    /* REMOVE then ADD for same key → collapse to UPDATE */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3003 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_item = json_string("Old description.");
    json_t *new_item = json_string("New description.");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, old_item, NULL);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* ADD back → should collapse to UPDATE */
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_ADD, NULL, new_item);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_UPDATE, found->field_type);
    TEST_ASSERT_STR_EQ("Old description.", json_string_value(found->old_value));
    TEST_ASSERT_STR_EQ("New description.", json_string_value(found->new_value));

    json_decref(old_item);
    json_decref(new_item);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_list_update_then_update(test_case_t *test)
{
    /* UPDATE then UPDATE → keep original old_value, update new_value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3004 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *original = json_string("Original");
    json_t *first    = json_string("First Edit");
    json_t *second   = json_string("Second Edit");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, original, first);
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, first, second);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_STR_EQ("Original", json_string_value(found->old_value));
    TEST_ASSERT_STR_EQ("Second Edit", json_string_value(found->new_value));

    json_decref(original);
    json_decref(first);
    json_decref(second);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_list_update_then_remove(test_case_t *test)
{
    /* UPDATE then REMOVE → collapse to REMOVE with original old_value */
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3005 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *original  = json_string("Original");
    json_t *edited    = json_string("Edited");

    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_UPDATE, original, edited);
    olc_changeset_add_change(cs, "extra_descr/statue", OLC_FIELD_LIST_REMOVE, edited, NULL);

    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));
    olc_pending_change_t *found = olc_changeset_find_change(cs, "extra_descr/statue");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(OLC_FIELD_LIST_REMOVE, found->field_type);
    TEST_ASSERT_STR_EQ("Original", json_string_value(found->old_value));
    TEST_ASSERT_NULL(found->new_value);

    json_decref(original);
    json_decref(edited);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/*
 * Test dispatcher — routes test_type to specific test functions.
 */
test_result_t run_olc_changeset_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "OLC changeset test missing test case or test_type");
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "olccs_create_destroy") == 0)
        return test_olccs_create_destroy(test);
    if (strcmp(test->test_type, "olccs_add_find_remove") == 0)
        return test_olccs_add_find_remove(test);
    if (strcmp(test->test_type, "olccs_update_in_place") == 0)
        return test_olccs_update_in_place(test);
    if (strcmp(test->test_type, "olccs_noop_detection") == 0)
        return test_olccs_noop_detection(test);
    if (strcmp(test->test_type, "olccs_clear") == 0)
        return test_olccs_clear(test);
    if (strcmp(test->test_type, "olccs_safety_limit") == 0)
        return test_olccs_safety_limit(test);
    if (strcmp(test->test_type, "olccs_edit_state_lifecycle") == 0)
        return test_olccs_edit_state_lifecycle(test);
    if (strcmp(test->test_type, "olccs_numeric_types") == 0)
        return test_olccs_numeric_types(test);
    if (strcmp(test->test_type, "olccs_list_add_remove") == 0)
        return test_list_add_then_remove(test);
    if (strcmp(test->test_type, "olccs_list_add_update") == 0)
        return test_list_add_then_update(test);
    if (strcmp(test->test_type, "olccs_list_remove_add") == 0)
        return test_list_remove_then_add(test);
    if (strcmp(test->test_type, "olccs_list_update_update") == 0)
        return test_list_update_then_update(test);
    if (strcmp(test->test_type, "olccs_list_update_remove") == 0)
        return test_list_update_then_remove(test);

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown OLC changeset test type: %s", test->test_type);
    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
