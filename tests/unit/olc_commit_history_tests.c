#ifdef BUILD_TESTS

#include <string.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../editors/common/olc_commit_history.h"
#include "../../editors/common/olc_changeset.h"

/* --- olchist_create_destroy --- */
static test_result_t test_olchist_create_destroy(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 100 };
    olc_commit_history_t *history = olc_commit_history_create(ED_OBJECT, wnum);

    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_INT_EQ(ED_OBJECT, history->editor_type);
    TEST_ASSERT_INT_EQ(1, (int)history->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(100, (int)history->entity_wnum.vnum);
    TEST_ASSERT_INT_EQ(0, olc_commit_history_count(history));
    TEST_ASSERT_INT_EQ(1, history->next_id);
    TEST_ASSERT_FALSE(history->is_dirty);

    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_archive --- */
static test_result_t test_olchist_archive(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 200 };
    olc_commit_history_t *history = olc_commit_history_create(ED_OBJECT, wnum);
    olc_changeset_t *cs = olc_changeset_create(ED_OBJECT, wnum, "Test Obj", "Builder");

    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING,
        json_string("old name"), json_string("new name"));
    olc_changeset_add_change(cs, "Weight", OLC_FIELD_INT16,
        json_integer(10), json_integer(20));

    olc_commit_record_t *record = olc_commit_history_archive(history, cs, 0, "test commit");

    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_INT_EQ(1, record->id);
    TEST_ASSERT_INT_EQ(0, record->group_id);
    TEST_ASSERT_STR_EQ("Builder", record->author);
    TEST_ASSERT_STR_EQ("test commit", record->comment);
    TEST_ASSERT_INT_EQ(2, list_size(record->changes));
    TEST_ASSERT_INT_EQ(1, olc_commit_history_count(history));
    TEST_ASSERT_TRUE(history->is_dirty);
    TEST_ASSERT_INT_EQ(2, history->next_id);

    /* Verify lookup */
    olc_commit_record_t *found = olc_commit_history_find(history, 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_INT_EQ(1, found->id);

    TEST_ASSERT_NULL(olc_commit_history_find(history, 999));

    olc_changeset_destroy(cs);
    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_eviction --- */
static test_result_t test_olchist_eviction(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 300 };
    olc_commit_history_t *history = olc_commit_history_create(ED_ROOM, wnum);
    history->max_records = 3;

    /* Archive 5 changesets — only newest 3 should survive */
    for (int i = 0; i < 5; i++) {
        olc_changeset_t *cs = olc_changeset_create(ED_ROOM, wnum, "Room", "Builder");
        char field[32];
        snprintf(field, sizeof(field), "Field_%d", i);
        olc_changeset_add_change(cs, field, OLC_FIELD_INT,
            json_integer(i), json_integer(i + 10));
        olc_commit_history_archive(history, cs, 0, NULL);
        olc_changeset_destroy(cs);
    }

    TEST_ASSERT_INT_EQ(3, olc_commit_history_count(history));

    /* Newest should be ID 5, oldest should be ID 3 */
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 5));
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 4));
    TEST_ASSERT_NOT_NULL(olc_commit_history_find(history, 3));
    TEST_ASSERT_NULL(olc_commit_history_find(history, 2));
    TEST_ASSERT_NULL(olc_commit_history_find(history, 1));

    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- olchist_serialize_roundtrip --- */
static test_result_t test_olchist_serialize_roundtrip(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 5, .vnum = 42 };
    olc_commit_history_t *history = olc_commit_history_create(ED_MOBILE, wnum);

    olc_changeset_t *cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Tester");
    olc_changeset_add_change(cs, "Name", OLC_FIELD_STRING,
        json_string("goblin"), json_string("cave goblin"));
    olc_changeset_add_change(cs, "Level", OLC_FIELD_INT,
        json_integer(5), json_integer(10));
    olc_commit_history_archive(history, cs, 42, "first edit");
    olc_changeset_destroy(cs);

    cs = olc_changeset_create(ED_MOBILE, wnum, "Mob", "Tester2");
    olc_changeset_add_change(cs, "Hitroll", OLC_FIELD_INT16,
        json_integer(3), json_integer(7));
    olc_commit_history_archive(history, cs, 0, NULL);
    olc_changeset_destroy(cs);

    /* Serialize */
    json_t *json = olc_commit_history_serialize(history);
    TEST_ASSERT_NOT_NULL(json);

    /* Deserialize */
    olc_commit_history_t *restored = olc_commit_history_deserialize(json);
    json_decref(json);
    TEST_ASSERT_NOT_NULL(restored);

    TEST_ASSERT_INT_EQ(ED_MOBILE, restored->editor_type);
    TEST_ASSERT_INT_EQ(5, (int)restored->entity_wnum.auid);
    TEST_ASSERT_INT_EQ(42, (int)restored->entity_wnum.vnum);
    TEST_ASSERT_INT_EQ(3, restored->next_id);
    TEST_ASSERT_INT_EQ(2, olc_commit_history_count(restored));

    /* Check first record */
    olc_commit_record_t *r1 = olc_commit_history_find(restored, 1);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_STR_EQ("Tester", r1->author);
    TEST_ASSERT_STR_EQ("first edit", r1->comment);
    TEST_ASSERT_INT_EQ(42, r1->group_id);
    TEST_ASSERT_INT_EQ(2, list_size(r1->changes));

    /* Check change values survived roundtrip */
    olc_committed_change_t *c1 = list_nthdata(r1->changes, 1);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_STR_EQ("Name", c1->field_path);
    TEST_ASSERT_INT_EQ(OLC_FIELD_STRING, c1->field_type);
    TEST_ASSERT_STR_EQ("goblin", json_string_value(c1->old_value));
    TEST_ASSERT_STR_EQ("cave goblin", json_string_value(c1->new_value));

    /* Verify list ordering: records should be newest-first after roundtrip.
     * Record 2 (id=2) was added second, so it should appear first in iteration. */
    ITERATOR it;
    iterator_start(&it, restored->records);
    olc_commit_record_t *first = iterator_nextdata(&it);
    olc_commit_record_t *second = iterator_nextdata(&it);
    iterator_stop(&it);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_INT_EQ(2, first->id);  /* newest first */
    TEST_ASSERT_INT_EQ(1, second->id); /* oldest last */

    olc_commit_history_destroy(history);
    olc_commit_history_destroy(restored);
    return TEST_SUCCESS;
}

/* --- olchist_group_id --- */
static test_result_t test_olchist_group_id(test_case_t *test)
{
    WNUM_LOAD wnum1 = { .auid = 1, .vnum = 10 };
    WNUM_LOAD wnum2 = { .auid = 1, .vnum = 20 };

    olc_commit_history_t *h1 = olc_commit_history_create(ED_OBJECT, wnum1);
    olc_commit_history_t *h2 = olc_commit_history_create(ED_OBJECT, wnum2);

    int gid = olc_next_group_id++;

    olc_changeset_t *cs1 = olc_changeset_create(ED_OBJECT, wnum1, "Obj1", "Builder");
    olc_changeset_add_change(cs1, "Name", OLC_FIELD_STRING,
        json_string("a"), json_string("b"));
    olc_commit_record_t *r1 = olc_commit_history_archive(h1, cs1, gid, NULL);

    olc_changeset_t *cs2 = olc_changeset_create(ED_OBJECT, wnum2, "Obj2", "Builder");
    olc_changeset_add_change(cs2, "Name", OLC_FIELD_STRING,
        json_string("c"), json_string("d"));
    olc_commit_record_t *r2 = olc_commit_history_archive(h2, cs2, gid, NULL);

    TEST_ASSERT_INT_EQ(gid, r1->group_id);
    TEST_ASSERT_INT_EQ(gid, r2->group_id);
    TEST_ASSERT_INT_EQ(r1->group_id, r2->group_id);

    olc_changeset_destroy(cs1);
    olc_changeset_destroy(cs2);
    olc_commit_history_destroy(h1);
    olc_commit_history_destroy(h2);
    return TEST_SUCCESS;
}

/* --- olchist_empty_changeset --- */
static test_result_t test_olchist_empty_changeset(test_case_t *test)
{
    WNUM_LOAD wnum = { .auid = 1, .vnum = 50 };
    olc_commit_history_t *history = olc_commit_history_create(ED_AREA, wnum);
    olc_changeset_t *cs = olc_changeset_create(ED_AREA, wnum, "Area", "Builder");

    /* Empty changeset should not archive */
    olc_commit_record_t *record = olc_commit_history_archive(history, cs, 0, NULL);
    TEST_ASSERT_NULL(record);
    TEST_ASSERT_INT_EQ(0, olc_commit_history_count(history));
    TEST_ASSERT_FALSE(history->is_dirty);

    olc_changeset_destroy(cs);
    olc_commit_history_destroy(history);
    return TEST_SUCCESS;
}

/* --- Dispatcher --- */

typedef struct {
    const char *test_type;
    test_result_t (*handler)(test_case_t *);
} olchist_handler_entry_t;

static const olchist_handler_entry_t olchist_handlers[] = {
    { "olchist_create_destroy",     test_olchist_create_destroy },
    { "olchist_archive",            test_olchist_archive },
    { "olchist_eviction",           test_olchist_eviction },
    { "olchist_serialize_roundtrip", test_olchist_serialize_roundtrip },
    { "olchist_group_id",           test_olchist_group_id },
    { "olchist_empty_changeset",    test_olchist_empty_changeset },
    { NULL, NULL }
};

test_result_t run_olc_commit_history_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    for (int i = 0; olchist_handlers[i].test_type; i++) {
        if (!strcmp(test->test_type, olchist_handlers[i].test_type))
            return olchist_handlers[i].handler(test);
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown olchist test type: %s", test->test_type);
    return TEST_ERROR;
}

#endif /* BUILD_TESTS */
