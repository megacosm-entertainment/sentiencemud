#ifdef BUILD_TESTS

#include <string.h>

#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../editors/common/olc_changeset.h"
#include "../../editors/common/olc_actions.h"

/* =========================================================================
 * Session Management Tests
 * ========================================================================= */

static test_result_t test_olcact_session_create(test_case_t *test)
{
    (void)test;
    olc_edit_state_t *state = olc_edit_state_create();

    olc_action_session_t *session = olc_action_session_create(state, ED_ROOM, "add_affect");
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_STR_EQ("add_affect", session->action_name);
    TEST_ASSERT_INT_EQ(ED_ROOM, session->editor_type);
    TEST_ASSERT_TRUE(state->has_action_session);
    TEST_ASSERT_TRUE(session->created_at > 0);

    /* Session ID should start with "act_" */
    TEST_ASSERT_TRUE(strncmp(session->session_id, "act_", 4) == 0);

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_find(test_case_t *test)
{
    (void)test;
    olc_edit_state_t *state = olc_edit_state_create();

    olc_action_session_t *session = olc_action_session_create(state, ED_OBJECT, "remove_affect");
    TEST_ASSERT_NOT_NULL(session);

    /* Find by correct ID succeeds */
    olc_action_session_t *found = olc_action_session_find(state, session->session_id);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ(session->session_id, found->session_id);

    /* Find by wrong ID returns NULL */
    TEST_ASSERT_NULL(olc_action_session_find(state, "act_99999"));

    /* NULL session_id returns NULL */
    TEST_ASSERT_NULL(olc_action_session_find(state, NULL));

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_clear(test_case_t *test)
{
    (void)test;
    olc_edit_state_t *state = olc_edit_state_create();

    olc_action_session_t *session = olc_action_session_create(state, ED_MOBILE, "add_affect");
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_TRUE(state->has_action_session);

    char saved_id[32];
    strlcpy(saved_id, session->session_id, sizeof(saved_id));

    olc_action_session_clear(state);
    TEST_ASSERT_FALSE(state->has_action_session);
    TEST_ASSERT_NULL(olc_action_session_find(state, saved_id));

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_session_replace(test_case_t *test)
{
    (void)test;
    olc_edit_state_t *state = olc_edit_state_create();

    olc_action_session_t *s1 = olc_action_session_create(state, ED_ROOM, "add_affect");
    TEST_ASSERT_NOT_NULL(s1);
    char first_id[32];
    strlcpy(first_id, s1->session_id, sizeof(first_id));

    /* Create a second session — replaces the first */
    olc_action_session_t *s2 = olc_action_session_create(state, ED_ROOM, "remove_affect");
    TEST_ASSERT_NOT_NULL(s2);

    /* First session no longer findable */
    TEST_ASSERT_NULL(olc_action_session_find(state, first_id));

    /* Second session findable */
    TEST_ASSERT_NOT_NULL(olc_action_session_find(state, s2->session_id));
    TEST_ASSERT_STR_EQ("remove_affect", s2->action_name);

    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

/* =========================================================================
 * GMCP Message Builder Tests
 * ========================================================================= */

static test_result_t test_olcact_build_form(test_case_t *test)
{
    (void)test;
    olc_edit_state_t *state = olc_edit_state_create();

    olc_action_handler_t handler = {
        .action_name  = "add_affect",
        .editor_type  = ED_OBJECT,
        .display_hint = "dialog",
        .title        = "Add Affect",
        .list_name    = "affects",
        .form_fn      = NULL,
        .stage_fn     = NULL
    };

    olc_action_session_t *session = olc_action_session_create(state, ED_OBJECT, "add_affect");
    TEST_ASSERT_NOT_NULL(session);

    json_t *fields = json_array();
    json_array_append_new(fields, json_string("location"));
    json_array_append_new(fields, json_string("modifier"));

    json_t *form = olc_action_build_form("obj:1:100", &handler, session, fields);
    TEST_ASSERT_NOT_NULL(form);

    /* Verify all fields present */
    TEST_ASSERT_STR_EQ("obj:1:100", json_string_value(json_object_get(form, "entity_id")));
    TEST_ASSERT_STR_EQ("add_affect", json_string_value(json_object_get(form, "action")));
    TEST_ASSERT_STR_EQ(session->session_id, json_string_value(json_object_get(form, "session_id")));
    TEST_ASSERT_STR_EQ("dialog", json_string_value(json_object_get(form, "display_hint")));
    TEST_ASSERT_STR_EQ("Add Affect", json_string_value(json_object_get(form, "title")));
    TEST_ASSERT_STR_EQ("affects", json_string_value(json_object_get(form, "list_name")));
    TEST_ASSERT_NOT_NULL(json_object_get(form, "fields"));
    TEST_ASSERT_INT_EQ(2, (int)json_array_size(json_object_get(form, "fields")));

    json_decref(fields);
    json_decref(form);
    olc_edit_state_destroy(state);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_build_result_success(test_case_t *test)
{
    (void)test;

    json_t *result = olc_action_build_result("obj:1:100", "act_0", true, NULL);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_STR_EQ("obj:1:100", json_string_value(json_object_get(result, "entity_id")));
    TEST_ASSERT_STR_EQ("act_0", json_string_value(json_object_get(result, "session_id")));
    TEST_ASSERT_STR_EQ("success", json_string_value(json_object_get(result, "status")));

    /* No message field on success */
    TEST_ASSERT_NULL(json_object_get(result, "message"));

    json_decref(result);
    return TEST_SUCCESS;
}

static test_result_t test_olcact_build_result_error(test_case_t *test)
{
    (void)test;

    json_t *result = olc_action_build_result("obj:1:100", "act_0", false, "Invalid modifier");
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_STR_EQ("obj:1:100", json_string_value(json_object_get(result, "entity_id")));
    TEST_ASSERT_STR_EQ("act_0", json_string_value(json_object_get(result, "session_id")));
    TEST_ASSERT_STR_EQ("error", json_string_value(json_object_get(result, "status")));
    TEST_ASSERT_STR_EQ("Invalid modifier", json_string_value(json_object_get(result, "message")));

    json_decref(result);
    return TEST_SUCCESS;
}

/* =========================================================================
 * Editor Registration Tests
 * ========================================================================= */

static test_result_t test_oedit_registered(test_case_t *test)
{
    (void)test;
    extern void oedit_register_actions(void);
    oedit_register_actions();

    const olc_action_handler_t *h;

    h = olc_find_action(ED_OBJECT, "addaffect");
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_NOT_NULL(h->form_fn);
    TEST_ASSERT_NOT_NULL(h->stage_fn);

    h = olc_find_action(ED_OBJECT, "addtype");
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_action(ED_OBJECT, "removetype");
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_action(ED_OBJECT, "addoprog");
    TEST_ASSERT_NOT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_medit_registered(test_case_t *test)
{
    (void)test;
    extern void medit_register_actions(void);
    medit_register_actions();

    const olc_action_handler_t *h;

    h = olc_find_action(ED_MOBILE, "addmprog");
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_NOT_NULL(h->form_fn);
    TEST_ASSERT_NOT_NULL(h->stage_fn);

    h = olc_find_action(ED_MOBILE, "addquest");
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_action(ED_MOBILE, "addreputation");
    TEST_ASSERT_NOT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_redit_registered(test_case_t *test)
{
    (void)test;
    extern void redit_register_actions(void);
    redit_register_actions();

    const olc_action_handler_t *h;

    h = olc_find_action(ED_ROOM, "mreset");
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_NOT_NULL(h->form_fn);
    TEST_ASSERT_NOT_NULL(h->stage_fn);

    h = olc_find_action(ED_ROOM, "oreset");
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_action(ED_ROOM, "addrprog");
    TEST_ASSERT_NOT_NULL(h);

    h = olc_find_action(ED_ROOM, "addcdesc");
    TEST_ASSERT_NOT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_aedit_registered(test_case_t *test)
{
    (void)test;
    extern void aedit_register_actions(void);
    aedit_register_actions();

    const olc_action_handler_t *h;

    h = olc_find_action(ED_AREA, "addaprog");
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_NOT_NULL(h->form_fn);
    TEST_ASSERT_NOT_NULL(h->stage_fn);

    h = olc_find_action(ED_AREA, "addtrade");
    TEST_ASSERT_NOT_NULL(h);

    return TEST_SUCCESS;
}

static test_result_t test_form_fields(test_case_t *test)
{
    (void)test;
    extern void oedit_register_actions(void);
    oedit_register_actions();

    const olc_action_handler_t *h = olc_find_action(ED_OBJECT, "addaffect");
    TEST_ASSERT_NOT_NULL(h);

    json_t *fields = h->form_fn(NULL, NULL);
    TEST_ASSERT_NOT_NULL(fields);
    TEST_ASSERT_TRUE(json_is_array(fields));
    TEST_ASSERT_TRUE(json_array_size(fields) > 0);

    json_t *f0 = json_array_get(fields, 0);
    TEST_ASSERT_NOT_NULL(json_object_get(f0, "field"));
    TEST_ASSERT_NOT_NULL(json_object_get(f0, "label"));
    TEST_ASSERT_NOT_NULL(json_object_get(f0, "type"));

    json_decref(fields);
    return TEST_SUCCESS;
}

static test_result_t test_cross_editor(test_case_t *test)
{
    (void)test;
    extern void oedit_register_actions(void);
    extern void medit_register_actions(void);
    extern void redit_register_actions(void);
    extern void aedit_register_actions(void);
    oedit_register_actions();
    medit_register_actions();
    redit_register_actions();
    aedit_register_actions();

    /* Each editor's actions should be findable */
    TEST_ASSERT_NOT_NULL(olc_find_action(ED_OBJECT, "addaffect"));
    TEST_ASSERT_NOT_NULL(olc_find_action(ED_MOBILE, "addmprog"));
    TEST_ASSERT_NOT_NULL(olc_find_action(ED_ROOM, "mreset"));
    TEST_ASSERT_NOT_NULL(olc_find_action(ED_AREA, "addaprog"));

    /* Wrong editor type should NOT find the action */
    TEST_ASSERT_NULL(olc_find_action(ED_MOBILE, "addaffect"));
    TEST_ASSERT_NULL(olc_find_action(ED_OBJECT, "addmprog"));

    return TEST_SUCCESS;
}

/* =========================================================================
 * Test Dispatcher
 * ========================================================================= */

test_result_t run_olc_action_test_case(test_case_t *test)
{
    if (!test || !test->test_type) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "OLC action test missing test case or test_type");
        return TEST_ERROR;
    }

    if (strcmp(test->test_type, "olcact_session_create") == 0)
        return test_olcact_session_create(test);
    if (strcmp(test->test_type, "olcact_session_find") == 0)
        return test_olcact_session_find(test);
    if (strcmp(test->test_type, "olcact_session_clear") == 0)
        return test_olcact_session_clear(test);
    if (strcmp(test->test_type, "olcact_session_replace") == 0)
        return test_olcact_session_replace(test);
    if (strcmp(test->test_type, "olcact_build_form") == 0)
        return test_olcact_build_form(test);
    if (strcmp(test->test_type, "olcact_build_result_success") == 0)
        return test_olcact_build_result_success(test);
    if (strcmp(test->test_type, "olcact_build_result_error") == 0)
        return test_olcact_build_result_error(test);
    if (strcmp(test->test_type, "olcact_oedit_registered") == 0)
        return test_oedit_registered(test);
    if (strcmp(test->test_type, "olcact_medit_registered") == 0)
        return test_medit_registered(test);
    if (strcmp(test->test_type, "olcact_redit_registered") == 0)
        return test_redit_registered(test);
    if (strcmp(test->test_type, "olcact_aedit_registered") == 0)
        return test_aedit_registered(test);
    if (strcmp(test->test_type, "olcact_form_fields") == 0)
        return test_form_fields(test);
    if (strcmp(test->test_type, "olcact_cross_editor") == 0)
        return test_cross_editor(test);

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown OLC action test type: %s", test->test_type);
    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
