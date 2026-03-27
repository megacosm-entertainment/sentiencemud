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
    return TEST_SKIP;
}

static test_result_t test_capture_number(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_bool(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_text(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_type(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_flags(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_readonly(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_section(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_pair(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_capture_skip_hr(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
}

static test_result_t test_flag_options_json(test_case_t *test)
{
    (void)test;
    return TEST_SKIP;
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
