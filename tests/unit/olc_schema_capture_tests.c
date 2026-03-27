/**
 * OLC Schema Capture Tests
 *
 * Tests for capture_mode in olc_display_* functions, the
 * olc_schema_capture() orchestrator, and Editor.Open message building.
 */
#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <jansson.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../gmcp_editor.h"
#include "../../tables.h"
#include "../../editors/common.h"
#include "../../editors/common/olc_display.h"
#include "../../editors/common/olc_editor.h"
#include "../../editors/common/olc_changeset.h"

/*
 * Helper: create a minimal capture-mode layout context.
 * Does not need a real CHAR_DATA — capture mode skips rendering.
 */
static OLC_LAYOUT_CTX *create_capture_ctx(void)
{
    OLC_LAYOUT_CTX *ctx = alloc_mem(sizeof(OLC_LAYOUT_CTX));
    memset(ctx, 0, sizeof(OLC_LAYOUT_CTX));
    ctx->capture_mode = true;
    ctx->captured_fields = json_array();
    ctx->screen_width = 80;
    ctx->label_width = 16;
    ctx->value_width = 58;
    return ctx;
}

static void free_capture_ctx(OLC_LAYOUT_CTX *ctx)
{
    if (!ctx) return;
    if (ctx->captured_fields)
        json_decref(ctx->captured_fields);
    free_mem(ctx, sizeof(OLC_LAYOUT_CTX));
}

/* ---- Stub tests — each will be filled as capture branches are implemented ---- */

static test_result_t test_capture_string(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_string(ctx, &olc_theme_default, "Name", "name", "Test Room");

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Name", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("name", json_string_value(json_object_get(field, "command")));
    TEST_ASSERT_STR_EQ("Test Room", json_string_value(json_object_get(field, "value")));
    TEST_ASSERT_NULL(json_object_get(field, "readonly"));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_number(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_number(ctx, &olc_theme_default, "Level", "level", 42);

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("int", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Level", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("level", json_string_value(json_object_get(field, "command")));
    TEST_ASSERT_INT_EQ(42, json_integer_value(json_object_get(field, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_bool(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_bool(ctx, &olc_theme_default, "Enabled", "enabled", true);

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("bool", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Enabled", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("enabled", json_string_value(json_object_get(field, "command")));
    TEST_ASSERT_TRUE(json_is_true(json_object_get(field, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_text(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_text(ctx, &olc_theme_default, "Description", "desc", "A long description\nwith newlines");

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("multiline", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Description", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("desc", json_string_value(json_object_get(field, "command")));
    TEST_ASSERT_STR_EQ("A long description\nwith newlines", json_string_value(json_object_get(field, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_type(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_type(ctx, &olc_theme_default, "Sector", "sector", sector_flags, 0);

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("enum", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Sector", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("sector", json_string_value(json_object_get(field, "command")));
    
    json_t *options = json_object_get(field, "options");
    TEST_ASSERT_NOT_NULL(options);
    TEST_ASSERT_TRUE(json_is_array(options));
    TEST_ASSERT_TRUE(json_array_size(options) > 0);
    
    json_t *value = json_object_get(field, "value");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(json_is_string(value));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_flags(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_flags(ctx, &olc_theme_default, "Room Flags", "flags", room_flags, 0);

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("flags", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Room Flags", json_string_value(json_object_get(field, "label")));
    TEST_ASSERT_STR_EQ("flags", json_string_value(json_object_get(field, "command")));
    
    json_t *options = json_object_get(field, "options");
    TEST_ASSERT_NOT_NULL(options);
    TEST_ASSERT_TRUE(json_is_array(options));
    TEST_ASSERT_TRUE(json_array_size(options) > 0);
    
    json_t *value = json_object_get(field, "value");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(json_is_string(value));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_readonly(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_string(ctx, &olc_theme_default, "VNUM", NULL, "12345");

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_TRUE(json_is_true(json_object_get(field, "readonly")));
    TEST_ASSERT_NULL(json_object_get(field, "command"));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_section(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_section(ctx, &olc_theme_default, "Properties");

    TEST_ASSERT_INT_EQ(1, json_array_size(ctx->captured_fields));
    json_t *field = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("_section", json_string_value(json_object_get(field, "type")));
    TEST_ASSERT_STR_EQ("Properties", json_string_value(json_object_get(field, "title")));
    TEST_ASSERT_STR_EQ("Properties", ctx->current_section);

    olc_display_string(ctx, &olc_theme_default, "Name", "name", "Test");
    TEST_ASSERT_INT_EQ(2, json_array_size(ctx->captured_fields));
    json_t *field2 = json_array_get(ctx->captured_fields, 1);
    TEST_ASSERT_STR_EQ("Properties", json_string_value(json_object_get(field2, "section")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_pair(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_pair(ctx, &olc_theme_default, "X", "setx", "10", "Y", "sety", "20");

    TEST_ASSERT_INT_EQ(2, json_array_size(ctx->captured_fields));
    
    json_t *field1 = json_array_get(ctx->captured_fields, 0);
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(field1, "type")));
    TEST_ASSERT_STR_EQ("X", json_string_value(json_object_get(field1, "label")));
    TEST_ASSERT_STR_EQ("setx", json_string_value(json_object_get(field1, "command")));
    TEST_ASSERT_STR_EQ("10", json_string_value(json_object_get(field1, "value")));
    
    json_t *field2 = json_array_get(ctx->captured_fields, 1);
    TEST_ASSERT_STR_EQ("string", json_string_value(json_object_get(field2, "type")));
    TEST_ASSERT_STR_EQ("Y", json_string_value(json_object_get(field2, "label")));
    TEST_ASSERT_STR_EQ("sety", json_string_value(json_object_get(field2, "command")));
    TEST_ASSERT_STR_EQ("20", json_string_value(json_object_get(field2, "value")));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_capture_skip_hr(test_case_t *test)
{
    (void)test;
    OLC_LAYOUT_CTX *ctx = create_capture_ctx();

    olc_display_hr(ctx, &olc_theme_default);
    olc_display_blank(ctx);
    olc_display_footer(ctx, &olc_theme_default);

    TEST_ASSERT_INT_EQ(0, json_array_size(ctx->captured_fields));

    free_capture_ctx(ctx);
    return TEST_SUCCESS;
}

static test_result_t test_flag_options_json(test_case_t *test)
{
    (void)test;
    
    json_t *result = olc_flag_options_json(sex_flags);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(json_is_array(result));
    TEST_ASSERT_TRUE(json_array_size(result) > 0);
    json_decref(result);

    json_t *null_result = olc_flag_options_json(NULL);
    TEST_ASSERT_NOT_NULL(null_result);
    TEST_ASSERT_TRUE(json_is_array(null_result));
    TEST_ASSERT_INT_EQ(0, json_array_size(null_result));
    json_decref(null_result);

    return TEST_SUCCESS;
}

static test_result_t test_annotation_merge(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_build_open(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_build_open_editor_type(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

/* ---- Dispatcher ---- */

test_result_t run_olc_schema_capture_test_case(test_case_t *test)
{
    if (!test || !test->test_type) return TEST_ERROR;

    const char *type = test->test_type;
    if (!strcmp(type, "olcsc_capture_string"))        return test_capture_string(test);
    if (!strcmp(type, "olcsc_capture_number"))        return test_capture_number(test);
    if (!strcmp(type, "olcsc_capture_bool"))          return test_capture_bool(test);
    if (!strcmp(type, "olcsc_capture_text"))          return test_capture_text(test);
    if (!strcmp(type, "olcsc_capture_type"))          return test_capture_type(test);
    if (!strcmp(type, "olcsc_capture_flags"))         return test_capture_flags(test);
    if (!strcmp(type, "olcsc_capture_readonly"))      return test_capture_readonly(test);
    if (!strcmp(type, "olcsc_capture_section"))       return test_capture_section(test);
    if (!strcmp(type, "olcsc_capture_pair"))          return test_capture_pair(test);
    if (!strcmp(type, "olcsc_capture_skip_hr"))       return test_capture_skip_hr(test);
    if (!strcmp(type, "olcsc_flag_options_json"))     return test_flag_options_json(test);
    if (!strcmp(type, "olcsc_annotation_merge"))      return test_annotation_merge(test);
    if (!strcmp(type, "olcsc_build_open"))            return test_build_open(test);
    if (!strcmp(type, "olcsc_build_open_editor_type"))return test_build_open_editor_type(test);

    return TEST_SKIP;
}

#endif /* BUILD_TESTS */
