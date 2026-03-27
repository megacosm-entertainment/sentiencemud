/**
 * @file gmcp_editor_tests.c
 * @brief Tests for GMCP Editor Protocol message builders and entity ID formatting.
 */

#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../olc.h"
#include "../framework/test_framework.h"
#include "../../gmcp_editor.h"
#include "../../editors/common/olc_changeset.h"
#include <jansson.h>
#include <string.h>

/* =========================================================================
 * Message Builder Tests
 * ========================================================================= */

static test_result_t test_build_field_message(test_case_t *test)
{
    (void)test;
    json_t *val = json_string("A Glowing Cavern");
    json_t *msg = gmcp_editor_build_field("room:5#3001", "name",
        val, "string", true);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("name",
        json_string_value(json_object_get(msg, "field")));
    TEST_ASSERT_STR_EQ("A Glowing Cavern",
        json_string_value(json_object_get(msg, "value")));
    TEST_ASSERT_STR_EQ("string",
        json_string_value(json_object_get(msg, "type")));
    TEST_ASSERT_TRUE(json_is_true(json_object_get(msg, "is_pending")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    json_decref(val);
    return TEST_SUCCESS;
}

static test_result_t test_build_close_message(test_case_t *test)
{
    (void)test;
    json_t *msg = gmcp_editor_build_close("room:5#3001", "done");

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("done",
        json_string_value(json_object_get(msg, "reason")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    return TEST_SUCCESS;
}

static test_result_t test_build_error_message(test_case_t *test)
{
    (void)test;
    json_t *msg = gmcp_editor_build_error("room:5#3001", "level",
        "out_of_range", "Level must be between 1 and 200.");

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("level",
        json_string_value(json_object_get(msg, "field")));
    TEST_ASSERT_STR_EQ("out_of_range",
        json_string_value(json_object_get(msg, "error")));
    TEST_ASSERT_STR_EQ("Level must be between 1 and 200.",
        json_string_value(json_object_get(msg, "message")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    return TEST_SUCCESS;
}

static test_result_t test_build_commit_result(test_case_t *test)
{
    (void)test;
    json_t *msg = gmcp_editor_build_commit_result("room:5#3001", "success", 3);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_STR_EQ("success",
        json_string_value(json_object_get(msg, "status")));
    TEST_ASSERT_INT_EQ(3,
        (int)json_integer_value(json_object_get(msg, "changes_applied")));
    TEST_ASSERT_INT_EQ(1, (int)json_integer_value(json_object_get(msg, "_v")));

    json_decref(msg);
    return TEST_SUCCESS;
}

static test_result_t test_build_state_with_changes(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test Room", "Builder");

    json_t *old_v = json_string("Old Name");
    json_t *new_v = json_string("New Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    json_t *msg = gmcp_editor_build_state("room:5#3001", cs, false);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("room:5#3001",
        json_string_value(json_object_get(msg, "entity_id")));
    TEST_ASSERT_INT_EQ(1,
        (int)json_integer_value(json_object_get(msg, "pending_count")));
    TEST_ASSERT_FALSE(json_is_true(json_object_get(msg, "draft_restored")));

    json_t *changes = json_object_get(msg, "changes");
    TEST_ASSERT_NOT_NULL(changes);
    TEST_ASSERT_TRUE(json_is_array(changes));
    TEST_ASSERT_INT_EQ(1, (int)json_array_size(changes));

    json_t *entry = json_array_get(changes, 0);
    TEST_ASSERT_STR_EQ("name", json_string_value(json_object_get(entry, "field")));
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(entry, "type")));
    TEST_ASSERT_STR_EQ("New Name", json_string_value(json_object_get(entry, "value")));

    json_decref(old_v);
    json_decref(new_v);
    json_decref(msg);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_build_state_empty(test_case_t *test)
{
    (void)test;
    json_t *msg = gmcp_editor_build_state("room:5#3001", NULL, false);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_INT_EQ(0,
        (int)json_integer_value(json_object_get(msg, "pending_count")));

    json_t *changes = json_object_get(msg, "changes");
    TEST_ASSERT_NOT_NULL(changes);
    TEST_ASSERT_INT_EQ(0, (int)json_array_size(changes));

    json_decref(msg);
    return TEST_SUCCESS;
}

static test_result_t test_build_group_commit_result(test_case_t *test)
{
    (void)test;
    json_t *results = json_array();
    json_t *r1 = json_object();
    json_object_set_new(r1, "entity_id", json_string("room:5#3001"));
    json_object_set_new(r1, "status", json_string("success"));
    json_array_append_new(results, r1);

    json_t *msg = gmcp_editor_build_group_commit_result(42, results, "batch update");

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_INT_EQ(42,
        (int)json_integer_value(json_object_get(msg, "group_id")));
    TEST_ASSERT_STR_EQ("batch update",
        json_string_value(json_object_get(msg, "comment")));

    json_t *res = json_object_get(msg, "results");
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_INT_EQ(1, (int)json_array_size(res));

    json_decref(results);
    json_decref(msg);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Entity ID Tests
 * ========================================================================= */

static test_result_t test_entity_id_room(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    const char *id = gmcp_editor_entity_id(ED_ROOM, wnum);
    TEST_ASSERT_STR_EQ("room:5#3001", id);
    return TEST_SUCCESS;
}

static test_result_t test_entity_id_area(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 42, .vnum = 0 };
    const char *id = gmcp_editor_entity_id(ED_AREA, wnum);
    TEST_ASSERT_STR_EQ("area:42", id);
    return TEST_SUCCESS;
}

static test_result_t test_entity_id_parse_roundtrip(test_case_t *test)
{
    (void)test;
    int editor_type = 0;
    WNUM_LOAD wnum = { 0, 0 };

    TEST_ASSERT_TRUE(gmcp_editor_parse_entity_id("mob:10#500", &editor_type, &wnum));
    TEST_ASSERT_INT_EQ(ED_MOBILE, editor_type);
    TEST_ASSERT_INT_EQ(10, (int)wnum.auid);
    TEST_ASSERT_INT_EQ(500, (int)wnum.vnum);

    TEST_ASSERT_TRUE(gmcp_editor_parse_entity_id("area:7", &editor_type, &wnum));
    TEST_ASSERT_INT_EQ(ED_AREA, editor_type);
    TEST_ASSERT_INT_EQ(7, (int)wnum.auid);

    TEST_ASSERT_FALSE(gmcp_editor_parse_entity_id("invalid", &editor_type, &wnum));
    TEST_ASSERT_FALSE(gmcp_editor_parse_entity_id(NULL, &editor_type, &wnum));

    return TEST_SUCCESS;
}

static test_result_t test_build_field_null_value(test_case_t *test)
{
    (void)test;
    json_t *msg = gmcp_editor_build_field("room:1#1", "desc", NULL, "string", false);

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_TRUE(json_is_null(json_object_get(msg, "value")));
    TEST_ASSERT_FALSE(json_is_true(json_object_get(msg, "is_pending")));

    json_decref(msg);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Dispatcher
 * ========================================================================= */

test_result_t run_gmcp_editor_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    const char *type = test->test_type;
    if (!strcmp(type, "gmcped_build_field"))          return test_build_field_message(test);
    if (!strcmp(type, "gmcped_build_close"))          return test_build_close_message(test);
    if (!strcmp(type, "gmcped_build_error"))          return test_build_error_message(test);
    if (!strcmp(type, "gmcped_build_commit_result"))  return test_build_commit_result(test);
    if (!strcmp(type, "gmcped_build_state"))          return test_build_state_with_changes(test);
    if (!strcmp(type, "gmcped_build_state_empty"))    return test_build_state_empty(test);
    if (!strcmp(type, "gmcped_build_group_commit"))   return test_build_group_commit_result(test);
    if (!strcmp(type, "gmcped_entity_id_room"))       return test_entity_id_room(test);
    if (!strcmp(type, "gmcped_entity_id_area"))       return test_entity_id_area(test);
    if (!strcmp(type, "gmcped_entity_id_parse"))      return test_entity_id_parse_roundtrip(test);
    if (!strcmp(type, "gmcped_build_field_null"))     return test_build_field_null_value(test);

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
