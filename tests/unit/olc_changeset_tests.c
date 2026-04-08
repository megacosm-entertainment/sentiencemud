#ifdef BUILD_TESTS

#include <string.h>

#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../editors/common/olc_changeset.h"
#include "../../editors/common/olc_field_handlers.h"
#include "../../editors/common/olc_staged.h"
#include "../../editors/common/olc_commands.h"
#include "../../editors/common/olc_editor.h"
#include "../../item_types.h"
#include "../../gmcp_editor.h"

/* Apply functions from oedit.c / oedit_types.c used by integration tests */
extern bool oedit_apply_affect_ops(void *entity, olc_pending_change_t *change);
extern bool oedit_apply_lock(void *entity, olc_pending_change_t *change);
extern bool oedit_apply_typedata(void *entity, olc_pending_change_t *change);
extern json_t *oedit_type_schema(const OBJ_INDEX_DATA *pObj);

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

/* ========================================================================
 * Field handler framework tests
 * ======================================================================== */

/* Dummy apply function for handler lookup tests */
static bool dummy_apply_fn(void *entity, olc_pending_change_t *change)
{
    (void)entity;
    (void)change;
    return true;
}

static test_result_t test_olccs_fh_lookup(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "name",    OLC_FIELD_STRING, NULL, dummy_apply_fn, NULL },
        { "level",   OLC_FIELD_INT,    NULL, dummy_apply_fn, NULL },
        { "exits/*", OLC_FIELD_EXIT,   NULL, dummy_apply_fn, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    /* Exact match */
    const olc_field_handler_t *h = olc_find_field_handler(handlers, "name", OLC_FIELD_STRING);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("name", h->field_path);

    h = olc_find_field_handler(handlers, "level", OLC_FIELD_INT);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("level", h->field_path);

    /* Miss */
    h = olc_find_field_handler(handlers, "nonexistent", OLC_FIELD_STRING);
    TEST_ASSERT_NULL(h);

    /* NULL handlers array */
    h = olc_find_field_handler(NULL, "name", OLC_FIELD_STRING);
    TEST_ASSERT_NULL(h);

    /* NULL field_path */
    h = olc_find_field_handler(handlers, NULL, OLC_FIELD_STRING);
    TEST_ASSERT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_olccs_wildcard_match(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "exits/north", OLC_FIELD_EXIT,   NULL, dummy_apply_fn, NULL },
        { "exits/*",     OLC_FIELD_EXIT,   NULL, dummy_apply_fn, NULL },
        { "extra/*",     OLC_FIELD_STRING, NULL, dummy_apply_fn, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    /* Wildcard matches */
    const olc_field_handler_t *h = olc_find_field_handler(handlers, "exits/south", OLC_FIELD_EXIT);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("exits/*", h->field_path);

    h = olc_find_field_handler(handlers, "extra/statue", OLC_FIELD_STRING);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("extra/*", h->field_path);

    /* Exact match takes priority over wildcard */
    h = olc_find_field_handler(handlers, "exits/north", OLC_FIELD_EXIT);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("exits/north", h->field_path);

    /* Wildcard does not match deeper paths */
    h = olc_find_field_handler(handlers, "exits/north/key", OLC_FIELD_EXIT);
    TEST_ASSERT_NULL(h);

    /* Wildcard does not match the prefix alone */
    h = olc_find_field_handler(handlers, "exits/", OLC_FIELD_EXIT);
    TEST_ASSERT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_string(test_case_t *test)
{
    (void)test;

    char *field = str_dup("original value");
    json_t *new_val = json_string("updated value");

    olc_pending_change_t change = {
        .field_path = str_dup("name"),
        .field_type = OLC_FIELD_STRING,
        .old_value  = NULL,
        .new_value  = new_val
    };

    TEST_ASSERT_TRUE(olc_apply_generic_string(&field, &change));
    TEST_ASSERT_STR_EQ("updated value", field);

    /* NULL field_ptr */
    TEST_ASSERT_FALSE(olc_apply_generic_string(NULL, &change));

    /* NULL change */
    TEST_ASSERT_FALSE(olc_apply_generic_string(&field, NULL));

    /* NULL new_value */
    olc_pending_change_t change_null = {
        .field_path = str_dup("name"),
        .field_type = OLC_FIELD_STRING,
        .old_value  = NULL,
        .new_value  = NULL
    };
    TEST_ASSERT_FALSE(olc_apply_generic_string(&field, &change_null));

    /* Non-string JSON value */
    olc_pending_change_t change_bad = {
        .field_path = str_dup("name"),
        .field_type = OLC_FIELD_STRING,
        .old_value  = NULL,
        .new_value  = json_integer(42)
    };
    TEST_ASSERT_FALSE(olc_apply_generic_string(&field, &change_bad));

    free_string(field);
    json_decref(new_val);
    free_string(change.field_path);
    free_string(change_null.field_path);
    json_decref(change_bad.new_value);
    free_string(change_bad.field_path);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_int(test_case_t *test)
{
    (void)test;

    int field = 5;
    json_t *new_val = json_integer(42);

    olc_pending_change_t change = {
        .field_path = str_dup("level"),
        .field_type = OLC_FIELD_INT,
        .old_value  = NULL,
        .new_value  = new_val
    };

    TEST_ASSERT_TRUE(olc_apply_generic_int(&field, &change));
    TEST_ASSERT_INT_EQ(42, field);

    /* NULL field_ptr */
    TEST_ASSERT_FALSE(olc_apply_generic_int(NULL, &change));

    /* Non-integer JSON value */
    olc_pending_change_t change_bad = {
        .field_path = str_dup("level"),
        .field_type = OLC_FIELD_INT,
        .old_value  = NULL,
        .new_value  = json_string("not a number")
    };
    TEST_ASSERT_FALSE(olc_apply_generic_int(&field, &change_bad));
    TEST_ASSERT_INT_EQ(42, field); /* Unchanged after failed apply */

    json_decref(new_val);
    free_string(change.field_path);
    json_decref(change_bad.new_value);
    free_string(change_bad.field_path);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_bool(test_case_t *test)
{
    (void)test;

    bool field = false;
    json_t *new_val = json_true();

    olc_pending_change_t change = {
        .field_path = str_dup("is_aggressive"),
        .field_type = OLC_FIELD_BOOL,
        .old_value  = NULL,
        .new_value  = new_val
    };

    TEST_ASSERT_TRUE(olc_apply_generic_bool(&field, &change));
    TEST_ASSERT_TRUE(field);

    /* Set back to false */
    json_t *false_val = json_false();
    olc_pending_change_t change2 = {
        .field_path = str_dup("is_aggressive"),
        .field_type = OLC_FIELD_BOOL,
        .old_value  = NULL,
        .new_value  = false_val
    };
    TEST_ASSERT_TRUE(olc_apply_generic_bool(&field, &change2));
    TEST_ASSERT_FALSE(field);

    /* Non-boolean JSON value */
    olc_pending_change_t change_bad = {
        .field_path = str_dup("is_aggressive"),
        .field_type = OLC_FIELD_BOOL,
        .old_value  = NULL,
        .new_value  = json_integer(1)
    };
    TEST_ASSERT_FALSE(olc_apply_generic_bool(&field, &change_bad));

    json_decref(new_val);
    json_decref(false_val);
    free_string(change.field_path);
    free_string(change2.field_path);
    json_decref(change_bad.new_value);
    free_string(change_bad.field_path);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_flags(test_case_t *test)
{
    (void)test;

    long field = 0x01;
    json_t *new_val = json_integer(0x05);

    olc_pending_change_t change = {
        .field_path = str_dup("act_flags"),
        .field_type = OLC_FIELD_FLAGS,
        .old_value  = NULL,
        .new_value  = new_val
    };

    TEST_ASSERT_TRUE(olc_apply_generic_flags(&field, &change));
    TEST_ASSERT_INT_EQ(0x05, (int)field);

    /* NULL field_ptr */
    TEST_ASSERT_FALSE(olc_apply_generic_flags(NULL, &change));

    /* Non-integer JSON value */
    olc_pending_change_t change_bad = {
        .field_path = str_dup("act_flags"),
        .field_type = OLC_FIELD_FLAGS,
        .old_value  = NULL,
        .new_value  = json_string("not flags")
    };
    TEST_ASSERT_FALSE(olc_apply_generic_flags(&field, &change_bad));

    json_decref(new_val);
    free_string(change.field_path);
    json_decref(change_bad.new_value);
    free_string(change_bad.field_path);
    return TEST_SUCCESS;
}

/* Test entity struct for commit/revert tests */
typedef struct test_entity {
    char    *name;
    int      level;
    bool     is_aggressive;
    int16_t  condition;
    long     flags;
} test_entity_t;

/* Apply functions that use generic helpers with typed offsets */
static bool test_apply_name(void *entity, olc_pending_change_t *change)
{
    test_entity_t *e = (test_entity_t *)entity;
    return olc_apply_generic_string(&e->name, change);
}

static bool test_apply_level(void *entity, olc_pending_change_t *change)
{
    test_entity_t *e = (test_entity_t *)entity;
    return olc_apply_generic_int(&e->level, change);
}

static bool test_apply_aggressive(void *entity, olc_pending_change_t *change)
{
    test_entity_t *e = (test_entity_t *)entity;
    return olc_apply_generic_bool(&e->is_aggressive, change);
}

/* Macro-generated apply functions for additional fields */
OLC_FIELD_APPLY_INT16(test_apply_condition, test_entity_t, condition)
OLC_FIELD_APPLY_FLAGS(test_apply_flags,     test_entity_t, flags)

static test_result_t test_olccs_commit_basic(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "name",          OLC_FIELD_STRING, NULL, test_apply_name,       NULL },
        { "level",         OLC_FIELD_INT,    NULL, test_apply_level,      NULL },
        { "is_aggressive", OLC_FIELD_BOOL,   NULL, test_apply_aggressive, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    test_entity_t entity = {
        .name          = str_dup("Old Name"),
        .level         = 1,
        .is_aggressive = false
    };

    WNUM_LOAD wnum = { .auid = 10, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Builder");

    json_t *old_name = json_string("Old Name");
    json_t *new_name = json_string("New Name");
    json_t *old_level = json_integer(1);
    json_t *new_level = json_integer(50);
    json_t *old_aggr = json_false();
    json_t *new_aggr = json_true();

    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_name, new_name);
    olc_changeset_add_change(cs, "level", OLC_FIELD_INT, old_level, new_level);
    olc_changeset_add_change(cs, "is_aggressive", OLC_FIELD_BOOL, old_aggr, new_aggr);
    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    const char *error_field = NULL;
    int result = olc_changeset_commit(cs, &entity, handlers, &error_field);
    TEST_ASSERT_INT_EQ(3, result);
    TEST_ASSERT_NULL(error_field);

    /* Verify entity was updated */
    TEST_ASSERT_STR_EQ("New Name", entity.name);
    TEST_ASSERT_INT_EQ(50, entity.level);
    TEST_ASSERT_TRUE(entity.is_aggressive);

    /* Changeset should be cleared after commit */
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    /* Commit with no handler → error */
    olc_changeset_add_change(cs, "unknown_field", OLC_FIELD_STRING,
                              json_string("a"), json_string("b"));
    result = olc_changeset_commit(cs, &entity, handlers, &error_field);
    TEST_ASSERT_INT_EQ(-1, result);
    TEST_ASSERT_NOT_NULL(error_field);
    TEST_ASSERT_STR_EQ("unknown_field", error_field);

    /* Commit with NULL apply_fn handler → skip, not crash */
    olc_changeset_clear(cs);
    olc_field_handler_t handlers_null_apply[] = {
        { "name", OLC_FIELD_STRING, NULL, NULL, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING,
                              json_string("x"), json_string("y"));
    error_field = NULL;
    result = olc_changeset_commit(cs, &entity, handlers_null_apply, &error_field);
    TEST_ASSERT_INT_EQ(0, result);  /* skipped, not applied */
    TEST_ASSERT_NULL(error_field);

    free_string(entity.name);
    json_decref(old_name);
    json_decref(new_name);
    json_decref(old_level);
    json_decref(new_level);
    json_decref(old_aggr);
    json_decref(new_aggr);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/**
 * Test commit with all 5 scalar types via macro-generated handlers.
 * Validates the OLC_FIELD_APPLY_* macros produce correct apply functions.
 */
static test_result_t test_olccs_commit_all_types(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "name",       OLC_FIELD_STRING, NULL, test_apply_name,       NULL },
        { "level",      OLC_FIELD_INT,    NULL, test_apply_level,      NULL },
        { "aggressive", OLC_FIELD_BOOL,   NULL, test_apply_aggressive, NULL },
        { "condition",  OLC_FIELD_INT16,  NULL, test_apply_condition,  NULL },
        { "flags",      OLC_FIELD_FLAGS,  NULL, test_apply_flags,      NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    test_entity_t entity = {
        .name          = str_dup("Original"),
        .level         = 5,
        .is_aggressive = false,
        .condition     = 80,
        .flags         = 0x10
    };

    WNUM_LOAD wnum = { .auid = 10, .vnum = 300 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Obj", "Builder");

    olc_changeset_add_change(cs, "name",       OLC_FIELD_STRING, json_string("Original"),  json_string("Updated"));
    olc_changeset_add_change(cs, "level",      OLC_FIELD_INT,    json_integer(5),           json_integer(25));
    olc_changeset_add_change(cs, "condition",  OLC_FIELD_INT16,  json_integer(80),          json_integer(50));
    olc_changeset_add_change(cs, "aggressive", OLC_FIELD_BOOL,   json_false(),              json_true());
    olc_changeset_add_change(cs, "flags",      OLC_FIELD_FLAGS,  json_integer(0x10),        json_integer(0x30));
    TEST_ASSERT_INT_EQ(5, olc_changeset_count(cs));

    const char *error_field = NULL;
    int result = olc_changeset_commit(cs, &entity, handlers, &error_field);
    TEST_ASSERT_INT_EQ(5, result);
    TEST_ASSERT_NULL(error_field);

    TEST_ASSERT_STR_EQ("Updated", entity.name);
    TEST_ASSERT_INT_EQ(25, entity.level);
    TEST_ASSERT_INT_EQ(50, (int)entity.condition);
    TEST_ASSERT_TRUE(entity.is_aggressive);
    TEST_ASSERT_INT_EQ(0x30, (int)entity.flags);

    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    free_string(entity.name);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_revert(test_case_t *test)
{
    (void)test;

    WNUM_LOAD wnum = { .auid = 10, .vnum = 200 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    json_t *old_val = json_string("old");
    json_t *new_val = json_string("new");

    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_val, new_val);
    olc_changeset_add_change(cs, "desc", OLC_FIELD_STRING, old_val, new_val);
    olc_changeset_add_change(cs, "level", OLC_FIELD_INT,
                              json_integer(1), json_integer(10));
    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    /* Revert single field */
    TEST_ASSERT_TRUE(olc_changeset_revert_field(cs, "name"));
    TEST_ASSERT_INT_EQ(2, olc_changeset_count(cs));
    TEST_ASSERT_NULL(olc_changeset_find_change(cs, "name"));

    /* Revert nonexistent field returns false */
    TEST_ASSERT_FALSE(olc_changeset_revert_field(cs, "nonexistent"));
    TEST_ASSERT_INT_EQ(2, olc_changeset_count(cs));

    /* Revert all */
    olc_changeset_revert(cs);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(cs));

    json_decref(old_val);
    json_decref(new_val);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Preview Helper Tests
 * ========================================================================= */

static test_result_t test_staged_string(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    /* No pending change → returns live value */
    const char *result = olc_staged_string(cs, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Live Name", result);

    /* Add pending change → returns staged value */
    json_t *old_v = json_string("Live Name");
    json_t *new_v = json_string("Staged Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    result = olc_staged_string(cs, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Staged Name", result);

    /* NULL changeset → returns live value */
    result = olc_staged_string(NULL, "name", "Live Name");
    TEST_ASSERT_STR_EQ("Live Name", result);

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_staged_int(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    /* No change → live */
    TEST_ASSERT_INT_EQ(100, olc_staged_int(cs, "heal_rate", 100));

    /* With pending change */
    json_t *old_v = json_integer(100);
    json_t *new_v = json_integer(200);
    olc_changeset_add_change(cs, "heal_rate", OLC_FIELD_INT, old_v, new_v);

    TEST_ASSERT_INT_EQ(200, olc_staged_int(cs, "heal_rate", 100));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_staged_flags(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    json_t *old_v = json_integer(0x03);
    json_t *new_v = json_integer(0x0F);
    olc_changeset_add_change(cs, "room_flags", OLC_FIELD_FLAGS, old_v, new_v);

    long result = olc_staged_flags(cs, "room_flags", 0x03);
    TEST_ASSERT_INT_EQ(0x0F, (int)result);

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_staged_bool(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    TEST_ASSERT_FALSE(olc_staged_bool(cs, "no_recall", false));

    json_t *old_v = json_false();
    json_t *new_v = json_true();
    olc_changeset_add_change(cs, "no_recall", OLC_FIELD_BOOL, old_v, new_v);

    TEST_ASSERT_TRUE(olc_staged_bool(cs, "no_recall", false));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_is_field_staged(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 3001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Test", "Builder");

    TEST_ASSERT_FALSE(olc_is_field_staged(cs, "name"));

    json_t *old_v = json_string("Old");
    json_t *new_v = json_string("New");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    TEST_ASSERT_TRUE(olc_is_field_staged(cs, "name"));
    TEST_ASSERT_FALSE(olc_is_field_staged(cs, "description"));

    json_decref(old_v);
    json_decref(new_v);
    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Serialization Tests
 * ========================================================================= */

static test_result_t test_olccs_serialize_roundtrip(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 7, .vnum = 5001 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Roundtrip Room", "Tester");

    json_t *old1 = json_string("Old Name");
    json_t *new1 = json_string("New Name");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old1, new1);

    json_t *old2 = json_integer(0);
    json_t *new2 = json_integer(42);
    olc_changeset_add_change(cs, "heal_rate", OLC_FIELD_INT, old2, new2);

    json_t *json = olc_changeset_serialize(cs);
    TEST_ASSERT_NOT_NULL(json);

    olc_changeset_t *restored = olc_changeset_deserialize(json);
    json_decref(json);
    TEST_ASSERT_NOT_NULL(restored);

    TEST_ASSERT_INT_EQ(ED_ROOM, restored->editor_type);
    TEST_ASSERT_INT_EQ(7, restored->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(5001, restored->entity_wnum.vnum);
    TEST_ASSERT_STR_EQ("Roundtrip Room", restored->entity_label);
    TEST_ASSERT_STR_EQ("Tester", restored->author);
    TEST_ASSERT_INT_EQ(2, olc_changeset_count(restored));
    TEST_ASSERT_FALSE(restored->is_dirty);

    olc_pending_change_t *c = olc_changeset_find_change(restored, "name");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_STR_EQ("New Name", json_string_value(c->new_value));

    c = olc_changeset_find_change(restored, "heal_rate");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_INT_EQ(42, json_integer_value(c->new_value));

    json_decref(old1); json_decref(new1);
    json_decref(old2); json_decref(new2);
    olc_changeset_destroy(cs);
    olc_changeset_destroy(restored);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_serialize_empty(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 1 };
    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Empty", "Builder");

    json_t *json = olc_changeset_serialize(cs);
    TEST_ASSERT_NOT_NULL(json);

    olc_changeset_t *restored = olc_changeset_deserialize(json);
    json_decref(json);
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_INT_EQ(0, olc_changeset_count(restored));
    TEST_ASSERT_INT_EQ(ED_MOBILE, restored->editor_type);

    olc_changeset_destroy(cs);
    olc_changeset_destroy(restored);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_deserialize_corrupt(test_case_t *test)
{
    /* NULL input */
    TEST_ASSERT_NULL(olc_changeset_deserialize(NULL));

    /* Not an object */
    json_t *arr = json_array();
    TEST_ASSERT_NULL(olc_changeset_deserialize(arr));
    json_decref(arr);

    /* Missing required fields */
    json_t *partial = json_object();
    json_object_set_new(partial, "editor_type", json_integer(ED_ROOM));
    TEST_ASSERT_NULL(olc_changeset_deserialize(partial));
    json_decref(partial);

    /* Invalid types in fields */
    json_t *bad = json_pack("{s:s, s:i, s:i, s:s, s:s}",
        "editor_type", "not_a_number",
        "entity_wnum_auid", 1,
        "entity_wnum_vnum", 1,
        "entity_label", "Test",
        "author", "Builder");
    TEST_ASSERT_NULL(olc_changeset_deserialize(bad));
    json_decref(bad);

    return TEST_SUCCESS;
}

static test_result_t test_olccs_draft_save_load(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 99, .vnum = 9999 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Draft Test", "DraftTester");

    json_t *old_v = json_string("old");
    json_t *new_v = json_string("new");
    olc_changeset_add_change(cs, "name", OLC_FIELD_STRING, old_v, new_v);

    TEST_ASSERT_TRUE(olc_draft_save(cs));
    TEST_ASSERT_TRUE(olc_draft_exists("DraftTester", ED_ROOM, wnum));

    olc_changeset_t *loaded = olc_draft_load("DraftTester", ED_ROOM, wnum);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(loaded));
    TEST_ASSERT_STR_EQ("Draft Test", loaded->entity_label);

    olc_pending_change_t *c = olc_changeset_find_change(loaded, "name");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_STR_EQ("new", json_string_value(c->new_value));

    /* Clean up */
    TEST_ASSERT_TRUE(olc_draft_discard("DraftTester", ED_ROOM, wnum));
    TEST_ASSERT_FALSE(olc_draft_exists("DraftTester", ED_ROOM, wnum));

    json_decref(old_v); json_decref(new_v);
    olc_changeset_destroy(cs);
    olc_changeset_destroy(loaded);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_long(test_case_t *test)
{
    long field = 500;
    olc_pending_change_t change = {
        .field_path = "Cost",
        .field_type = OLC_FIELD_LONG,
        .old_value = json_integer(500),
        .new_value = json_integer(99999)
    };
    TEST_ASSERT_TRUE(olc_apply_generic_long(&field, &change));
    TEST_ASSERT_TRUE(field == 99999);
    json_decref(change.old_value);
    json_decref(change.new_value);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_apply_dice(test_case_t *test)
{
    DICE_DATA dice = { .number = 1, .size = 6, .bonus = 0 };
    json_t *new_val = json_pack("{s:i, s:i, s:i}", "number", 10, "size", 8, "bonus", 200);
    olc_pending_change_t change = {
        .field_path = "Hit Dice",
        .field_type = OLC_FIELD_EMBEDDED,
        .old_value = json_null(),
        .new_value = new_val
    };
    TEST_ASSERT_TRUE(olc_apply_generic_dice(&dice, &change));
    TEST_ASSERT_INT_EQ(dice.number, 10);
    TEST_ASSERT_INT_EQ(dice.size, 8);
    TEST_ASSERT_INT_EQ(dice.bonus, 200);
    json_decref(new_val);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_long_roundtrip(test_case_t *test)
{
    WNUM_LOAD wl = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wl, "Test Obj", "tester");
    json_t *old_v = json_integer(0);
    json_t *new_v = json_integer(2000000000L);
    olc_changeset_add_change(cs, "Cost", OLC_FIELD_LONG, old_v, new_v);
    json_decref(old_v);
    json_decref(new_v);

    json_t *j = olc_changeset_serialize(cs);
    olc_changeset_t *cs2 = olc_changeset_deserialize(j);
    json_decref(j);

    olc_pending_change_t *ch = olc_changeset_find_change(cs2, "Cost");
    TEST_ASSERT_NOT_NULL(ch);
    TEST_ASSERT_TRUE(json_integer_value(ch->new_value) == 2000000000L);

    olc_changeset_destroy(cs);
    olc_changeset_destroy(cs2);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Phase 3 Infrastructure Tests
 * ========================================================================= */

static test_result_t test_olccs_next_seq(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    /* Empty changeset returns 0 for any prefix */
    TEST_ASSERT_INT_EQ(0, olc_changeset_next_seq(cs, "affects/add"));

    /* After adding affects/add:0, next seq is 1 */
    json_t *val = json_string("test");
    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD, NULL, val);
    json_decref(val);
    TEST_ASSERT_INT_EQ(1, olc_changeset_next_seq(cs, "affects/add"));

    /* After adding affects/add:5 (non-sequential), returns 6 (max + 1) */
    val = json_string("test2");
    olc_changeset_add_change(cs, "affects/add:5", OLC_FIELD_LIST_ADD, NULL, val);
    json_decref(val);
    TEST_ASSERT_INT_EQ(6, olc_changeset_next_seq(cs, "affects/add"));

    /* Different prefix still returns 0 */
    TEST_ASSERT_INT_EQ(0, olc_changeset_next_seq(cs, "affects/rm"));

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_revert_prefix(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");

    /* Stage: affects/add:0, affects/add:1, affects/rm:0, Name */
    json_t *v = json_string("aff0");
    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD, NULL, v);
    json_decref(v);

    v = json_string("aff1");
    olc_changeset_add_change(cs, "affects/add:1", OLC_FIELD_LIST_ADD, NULL, v);
    json_decref(v);

    v = json_pack("{s:i}", "index", 0);
    olc_changeset_add_change(cs, "affects/rm:0", OLC_FIELD_LIST_REMOVE, NULL, v);
    json_decref(v);

    json_t *old_name = json_string("Old Room");
    json_t *new_name = json_string("New Room");
    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING, old_name, new_name);
    json_decref(old_name);
    json_decref(new_name);

    TEST_ASSERT_INT_EQ(4, olc_changeset_count(cs));

    /* Revert prefix "affects" removes all 3 affects entries */
    int removed = olc_changeset_revert_prefix(cs, "affects");
    TEST_ASSERT_INT_EQ(3, removed);
    TEST_ASSERT_INT_EQ(1, olc_changeset_count(cs));

    /* Name change still exists */
    olc_pending_change_t *found = olc_changeset_find_change(cs, "Name");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("Name", found->field_path);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_multilevel_wildcard(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "typedata/**", OLC_FIELD_TYPE_DATA, NULL, dummy_apply_fn, NULL },
        { "var/*",       OLC_FIELD_STRING,    NULL, dummy_apply_fn, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    /* typedata/weapon/class matches typedata multi-level wildcard */
    const olc_field_handler_t *h = olc_find_field_handler(
        handlers, "typedata/weapon/class", OLC_FIELD_TYPE_DATA);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("typedata/**", h->field_path);

    /* typedata/+armor matches typedata multi-level wildcard */
    h = olc_find_field_handler(handlers, "typedata/+armor", OLC_FIELD_TYPE_DATA);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("typedata/**", h->field_path);

    /* var/mykey matches var single-level wildcard */
    h = olc_find_field_handler(handlers, "var/mykey", OLC_FIELD_STRING);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_STR_EQ("var/*", h->field_path);

    /* var/nested/key does NOT match var single-level wildcard */
    h = olc_find_field_handler(handlers, "var/nested/key", OLC_FIELD_STRING);
    TEST_ASSERT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_olccs_stage_list_ops(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Obj", "Builder");

    /* Stage two adds */
    json_t *v1 = json_string("affect1");
    olc_pending_change_t *c1 = olc_stage_list_add(cs, "affects", v1);
    json_decref(v1);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_STR_EQ("affects/add:0", c1->field_path);

    json_t *v2 = json_string("affect2");
    olc_pending_change_t *c2 = olc_stage_list_add(cs, "affects", v2);
    json_decref(v2);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_STR_EQ("affects/add:1", c2->field_path);

    /* Stage a remove with index 2 */
    json_t *old_v = json_string("old_affect");
    olc_pending_change_t *c3 = olc_stage_list_remove(cs, "affects", 2, old_v);
    json_decref(old_v);
    TEST_ASSERT_NOT_NULL(c3);
    TEST_ASSERT_STR_EQ("affects/rm:0", c3->field_path);

    /* Verify remove's new_value has {"index": 2} */
    TEST_ASSERT_NOT_NULL(c3->new_value);
    TEST_ASSERT_INT_EQ(2, (int)json_integer_value(json_object_get(c3->new_value, "index")));

    /* Total count should be 3 */
    TEST_ASSERT_INT_EQ(3, olc_changeset_count(cs));

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_embedded_snapshot(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Obj", "Builder");

    /* Add a change for "lock" with embedded JSON */
    json_t *old_lock = json_pack("{s:i, s:i}", "key_vnum", 0, "pick_mod", 0);
    json_t *new_lock = json_pack("{s:i, s:i}", "key_vnum", 100, "pick_mod", 5);
    olc_changeset_add_change(cs, "lock", OLC_FIELD_EMBEDDED, old_lock, new_lock);
    json_decref(old_lock);
    json_decref(new_lock);

    /* olc_staged_embedded should return the new_value JSON */
    json_t *embedded = olc_staged_embedded(cs, "lock");
    TEST_ASSERT_NOT_NULL(embedded);
    TEST_ASSERT_INT_EQ(100, (int)json_integer_value(json_object_get(embedded, "key_vnum")));
    TEST_ASSERT_INT_EQ(5, (int)json_integer_value(json_object_get(embedded, "pick_mod")));

    /* Reset dirty flag so we can verify the set marks it */
    cs->is_dirty = false;

    /* olc_staged_embedded_set should update pick_mod to 10 */
    json_t *new_pm = json_integer(10);
    TEST_ASSERT_TRUE(olc_staged_embedded_set(cs, "lock", "pick_mod", new_pm));
    json_decref(new_pm);

    /* Verify pick_mod is now 10 and is_dirty is true */
    embedded = olc_staged_embedded(cs, "lock");
    TEST_ASSERT_NOT_NULL(embedded);
    TEST_ASSERT_INT_EQ(10, (int)json_integer_value(json_object_get(embedded, "pick_mod")));
    TEST_ASSERT_TRUE(cs->is_dirty);

    /* Nonexistent field returns NULL */
    TEST_ASSERT_NULL(olc_staged_embedded(cs, "nonexistent"));

    /* Setting on nonexistent field returns false */
    json_t *dummy = json_integer(1);
    TEST_ASSERT_FALSE(olc_staged_embedded_set(cs, "nonexistent", "key", dummy));
    json_decref(dummy);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

static test_result_t test_olccs_flags_or(test_case_t *test)
{
    (void)test;
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Obj", "Builder");

    /* Unstaged field returns live value */
    TEST_ASSERT_TRUE(olc_staged_flags_or(cs, "extra_flags", 0x0F) == 0x0F);

    /* Stage an integer value for extra_flags */
    json_t *old_v = json_integer(0x0F);
    json_t *new_v = json_integer(0xFF);
    olc_changeset_add_change(cs, "extra_flags", OLC_FIELD_FLAGS, old_v, new_v);
    json_decref(old_v);
    json_decref(new_v);

    /* Staged integer field returns staged value, not live */
    TEST_ASSERT_TRUE(olc_staged_flags_or(cs, "extra_flags", 0x0F) == 0xFF);

    /* Stage a non-integer value — should fall back to live */
    olc_changeset_remove_change(cs, "extra_flags");
    json_t *old_null = json_null();
    json_t *new_null = json_null();
    olc_changeset_add_change(cs, "extra_flags", OLC_FIELD_FLAGS, old_null, new_null);
    json_decref(old_null);
    json_decref(new_null);
    TEST_ASSERT_TRUE(olc_staged_flags_or(cs, "extra_flags", 0x0F) == 0x0F);

    olc_changeset_destroy(cs);
    return TEST_SUCCESS;
}

/**
 * Test list add/remove apply: stage a REMOVE and ADD on affects, commit, verify.
 */
static test_result_t test_olccs_apply_list_ops(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "affects/**", OLC_FIELD_LIST_ADD, NULL, oedit_apply_affect_ops, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    OBJ_INDEX_DATA *pObj = new_obj_index();

    /* Add 3 affects: prepend order means list is INT(idx0), DEX(idx1), STR(idx2) */
    for (int i = 0; i < 3; i++) {
        AFFECT_DATA *pAf = new_affect();
        pAf->where    = TO_OBJECT;
        pAf->location = APPLY_STR + i;  /* STR=1, DEX=2, INT=3 */
        pAf->modifier = 10 + i;
        pAf->next     = pObj->affected;
        pObj->affected = pAf;
    }

    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum,
                                                "Test Object", "TestBuilder");

    /* Stage remove of index 1 (DEX affect in TO_OBJECT subset) */
    json_t *rm_val = json_pack("{s:i, s:s}", "index", 1, "where_filter", "object");
    olc_changeset_add_change(cs, "affects/rm:0", OLC_FIELD_LIST_REMOVE,
                              json_null(), rm_val);
    json_decref(rm_val);

    /* Stage add of APPLY_CON affect */
    json_t *add_val = json_pack("{s:s, s:s, s:i, s:i, s:i, s:I, s:i, s:i}",
        "where", "object", "location", "constitution",
        "modifier", 5, "type", -1, "duration", -1,
        "bitvector", (json_int_t)0, "level", 0, "random", 0);
    olc_changeset_add_change(cs, "affects/add:0", OLC_FIELD_LIST_ADD,
                              json_null(), add_val);
    json_decref(add_val);

    const char *error_field = NULL;
    int result = olc_changeset_commit(cs, pObj, handlers, &error_field);
    TEST_ASSERT_TRUE(result >= 0);

    /* Was 3 affects, removed 1, added 1 → still 3 */
    int count = 0;
    bool found_con = false;
    bool found_dex = false;
    for (AFFECT_DATA *pAf = pObj->affected; pAf; pAf = pAf->next) {
        if (pAf->where != TO_OBJECT) continue;
        count++;
        if (pAf->location == APPLY_CON) found_con = true;
        if (pAf->location == APPLY_DEX) found_dex = true;
    }
    TEST_ASSERT_INT_EQ(3, count);
    TEST_ASSERT_TRUE(found_con);
    TEST_ASSERT_FALSE(found_dex);

    olc_changeset_destroy(cs);
    free_obj_index(pObj);
    return TEST_SUCCESS;
}

/**
 * Test embedded snapshot apply: stage a lock, commit, verify fields.
 */
static test_result_t test_olccs_apply_embedded(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "lock", OLC_FIELD_EMBEDDED, NULL, oedit_apply_lock, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    OBJ_INDEX_DATA *pObj = new_obj_index();

    WNUM_LOAD wnum = { .auid = 1, .vnum = 200 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum,
                                                "Test Object", "TestBuilder");

    /* Stage a lock snapshot */
    json_t *new_lock = json_pack("{s:I, s:I, s:i, s:i}",
        "key_auid", (json_int_t)5, "key_vnum", (json_int_t)3001,
        "flags", 0, "pick_chance", 50);
    olc_changeset_add_change(cs, "lock", OLC_FIELD_EMBEDDED,
                              json_null(), new_lock);
    json_decref(new_lock);

    const char *error_field = NULL;
    int result = olc_changeset_commit(cs, pObj, handlers, &error_field);
    TEST_ASSERT_TRUE(result >= 0);

    TEST_ASSERT_NOT_NULL(pObj->lock);
    TEST_ASSERT_INT_EQ(50, pObj->lock->pick_chance);

    olc_changeset_destroy(cs);
    free_obj_index(pObj);
    return TEST_SUCCESS;
}

/**
 * Test type dispatch apply: stage weapon class change, commit, verify.
 */
static test_result_t test_olccs_apply_typedata(test_case_t *test)
{
    (void)test;

    olc_field_handler_t handlers[] = {
        { "typedata/**", OLC_FIELD_TYPE_DATA, NULL, oedit_apply_typedata, NULL },
        { NULL, 0, NULL, NULL, NULL }
    };

    OBJ_INDEX_DATA *pObj = new_obj_index();
    obj_index_alloc_type_data(pObj, ITEM_WEAPON);
    TEST_ASSERT_TRUE(IS_WEAPON(pObj));

    WNUM_LOAD wnum = { .auid = 1, .vnum = 300 };
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum,
                                                "Test Object", "TestBuilder");

    json_t *old_val = json_integer(WEAPON(pObj)->weapon_class);
    json_t *new_val = json_integer(WEAPON_SWORD);
    olc_changeset_add_change(cs, "typedata/weapon/class", OLC_FIELD_TYPE_DATA,
                              old_val, new_val);
    json_decref(old_val);
    json_decref(new_val);

    const char *error_field = NULL;
    int result = olc_changeset_commit(cs, pObj, handlers, &error_field);
    TEST_ASSERT_TRUE(result >= 0);

    TEST_ASSERT_INT_EQ(WEAPON_SWORD, WEAPON(pObj)->weapon_class);

    olc_changeset_destroy(cs);
    free_obj_index(pObj);
    return TEST_SUCCESS;
}

static test_result_t test_schema_update_build(test_case_t *test)
{
    json_t *fields = json_array();
    json_t *f = json_pack("{s:s, s:s, s:s, s:s}",
        "label", "Class:", "command", "typedata/weapon/class",
        "type", "enum", "value", "sword");
    json_array_append_new(fields, f);

    json_t *msg = gmcp_editor_build_schema_update("obj:5#3010", "Type", fields);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_STR_EQ("obj:5#3010", json_string_value(json_object_get(msg, "entity_id")));

    json_t *updates = json_object_get(msg, "updates");
    TEST_ASSERT_TRUE(json_is_array(updates) && json_array_size(updates) == 1);

    json_t *upd = json_array_get(updates, 0);
    TEST_ASSERT_STR_EQ("Type", json_string_value(json_object_get(upd, "tab")));
    TEST_ASSERT_STR_EQ("replace", json_string_value(json_object_get(upd, "action")));

    json_t *flds = json_object_get(upd, "fields");
    TEST_ASSERT_TRUE(json_is_array(flds) && json_array_size(flds) == 1);

    TEST_ASSERT_TRUE(json_integer_value(json_object_get(msg, "_v")) == 1);

    json_decref(msg);
    return TEST_SUCCESS;
}

static test_result_t test_type_schema_integration(test_case_t *test)
{
    (void)test;

    /* Create a test object with weapon + armor types */
    OBJ_INDEX_DATA *pObj = new_obj_index();
    obj_index_alloc_type_data(pObj, ITEM_WEAPON);
    obj_index_alloc_type_data(pObj, ITEM_ARMOUR);
    TEST_ASSERT_TRUE(IS_WEAPON(pObj));
    TEST_ASSERT_TRUE(IS_ARMOR(pObj));

    /* Get the type schema */
    json_t *schema = oedit_type_schema(pObj);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(json_is_array(schema));
    TEST_ASSERT_TRUE(json_array_size(schema) > 0);

    /* Verify we have section markers for both types */
    bool found_weapon_section = false;
    bool found_armor_section = false;
    for (size_t i = 0; i < json_array_size(schema); i++) {
        json_t *entry = json_array_get(schema, i);
        const char *type = json_string_value(json_object_get(entry, "type"));
        const char *label = json_string_value(json_object_get(entry, "label"));
        if (type && strcmp(type, "section") == 0) {
            if (label && strcmp(label, "weapon") == 0) found_weapon_section = true;
            if (label && strcmp(label, "armor") == 0) found_armor_section = true;
        }
    }
    TEST_ASSERT_TRUE(found_weapon_section);
    TEST_ASSERT_TRUE(found_armor_section);

    /* Verify weapon fields have correct command path prefix */
    bool found_weapon_class = false;
    bool found_weapon_dice = false;
    bool found_weapon_flags = false;
    for (size_t i = 0; i < json_array_size(schema); i++) {
        json_t *entry = json_array_get(schema, i);
        const char *cmd = json_string_value(json_object_get(entry, "command"));
        if (!cmd) continue;
        if (strcmp(cmd, "typedata/weapon/class") == 0) {
            /* Verify it's an enum with options */
            TEST_ASSERT_STR_EQ("enum", json_string_value(json_object_get(entry, "type")));
            TEST_ASSERT_NOT_NULL(json_object_get(entry, "options"));
            TEST_ASSERT_TRUE(json_is_array(json_object_get(entry, "options")));
            found_weapon_class = true;
        }
        if (strcmp(cmd, "typedata/weapon/dice") == 0) {
            TEST_ASSERT_STR_EQ("dice", json_string_value(json_object_get(entry, "type")));
            found_weapon_dice = true;
        }
        if (strcmp(cmd, "typedata/weapon/flags") == 0) {
            TEST_ASSERT_STR_EQ("flags", json_string_value(json_object_get(entry, "type")));
            TEST_ASSERT_NOT_NULL(json_object_get(entry, "options"));
            found_weapon_flags = true;
        }
    }
    TEST_ASSERT_TRUE(found_weapon_class);
    TEST_ASSERT_TRUE(found_weapon_dice);
    TEST_ASSERT_TRUE(found_weapon_flags);

    /* Verify armor fields have correct command path prefix */
    bool found_armor_type = false;
    bool found_armor_pierce = false;
    for (size_t i = 0; i < json_array_size(schema); i++) {
        json_t *entry = json_array_get(schema, i);
        const char *cmd = json_string_value(json_object_get(entry, "command"));
        if (!cmd) continue;
        if (strcmp(cmd, "typedata/armor/type") == 0) {
            TEST_ASSERT_STR_EQ("enum", json_string_value(json_object_get(entry, "type")));
            found_armor_type = true;
        }
        if (strcmp(cmd, "typedata/armor/pierce") == 0) {
            TEST_ASSERT_STR_EQ("int", json_string_value(json_object_get(entry, "type")));
            found_armor_pierce = true;
        }
    }
    TEST_ASSERT_TRUE(found_armor_type);
    TEST_ASSERT_TRUE(found_armor_pierce);

    json_decref(schema);
    free_obj_index(pObj);
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
    if (strcmp(test->test_type, "olccs_fh_lookup") == 0)
        return test_olccs_fh_lookup(test);
    if (strcmp(test->test_type, "olccs_wildcard_match") == 0)
        return test_olccs_wildcard_match(test);
    if (strcmp(test->test_type, "olccs_apply_string") == 0)
        return test_olccs_apply_string(test);
    if (strcmp(test->test_type, "olccs_apply_int") == 0)
        return test_olccs_apply_int(test);
    if (strcmp(test->test_type, "olccs_apply_bool") == 0)
        return test_olccs_apply_bool(test);
    if (strcmp(test->test_type, "olccs_apply_flags") == 0)
        return test_olccs_apply_flags(test);
    if (strcmp(test->test_type, "olccs_commit_basic") == 0)
        return test_olccs_commit_basic(test);
    if (strcmp(test->test_type, "olccs_revert") == 0)
        return test_olccs_revert(test);
    if (strcmp(test->test_type, "olccs_staged_string") == 0)
        return test_staged_string(test);
    if (strcmp(test->test_type, "olccs_staged_int") == 0)
        return test_staged_int(test);
    if (strcmp(test->test_type, "olccs_staged_flags") == 0)
        return test_staged_flags(test);
    if (strcmp(test->test_type, "olccs_staged_bool") == 0)
        return test_staged_bool(test);
    if (strcmp(test->test_type, "olccs_is_field_staged") == 0)
        return test_is_field_staged(test);
    if (strcmp(test->test_type, "olccs_serialize_roundtrip") == 0)
        return test_olccs_serialize_roundtrip(test);
    if (strcmp(test->test_type, "olccs_serialize_empty") == 0)
        return test_olccs_serialize_empty(test);
    if (strcmp(test->test_type, "olccs_deserialize_corrupt") == 0)
        return test_olccs_deserialize_corrupt(test);
    if (strcmp(test->test_type, "olccs_draft_save_load") == 0)
        return test_olccs_draft_save_load(test);
    if (strcmp(test->test_type, "olccs_commit_all_types") == 0)
        return test_olccs_commit_all_types(test);
    if (strcmp(test->test_type, "olccs_apply_long") == 0)
        return test_olccs_apply_long(test);
    if (strcmp(test->test_type, "olccs_apply_dice") == 0)
        return test_olccs_apply_dice(test);
    if (strcmp(test->test_type, "olccs_long_roundtrip") == 0)
        return test_olccs_long_roundtrip(test);
    if (strcmp(test->test_type, "olccs_next_seq") == 0)
        return test_olccs_next_seq(test);
    if (strcmp(test->test_type, "olccs_revert_prefix") == 0)
        return test_olccs_revert_prefix(test);
    if (strcmp(test->test_type, "olccs_multilevel_wildcard") == 0)
        return test_olccs_multilevel_wildcard(test);
    if (strcmp(test->test_type, "olccs_stage_list_ops") == 0)
        return test_olccs_stage_list_ops(test);
    if (strcmp(test->test_type, "olccs_embedded_snapshot") == 0)
        return test_olccs_embedded_snapshot(test);
    if (strcmp(test->test_type, "olccs_flags_or") == 0)
        return test_olccs_flags_or(test);
    if (strcmp(test->test_type, "olccs_apply_list") == 0)
        return test_olccs_apply_list_ops(test);
    if (strcmp(test->test_type, "olccs_apply_embed") == 0)
        return test_olccs_apply_embedded(test);
    if (strcmp(test->test_type, "olccs_apply_typed") == 0)
        return test_olccs_apply_typedata(test);
    if (strcmp(test->test_type, "olccs_schema_update_build") == 0)
        return test_schema_update_build(test);
    if (strcmp(test->test_type, "olccs_type_schema_integ") == 0)
        return test_type_schema_integration(test);

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown OLC changeset test type: %s", test->test_type);
    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
