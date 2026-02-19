#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../scripts.h"
#include "../framework/test_framework.h"
#include <string.h>
#include <stdlib.h>

static test_result_t test_script_entity_lookup(test_case_t *test);
static test_result_t test_script_ifcheck_lookup(test_case_t *test);
static test_result_t test_script_compile_string_bounds(test_case_t *test);
static test_result_t test_script_entity_table_validation(test_case_t *test);
static void log_entity_table_validation_diagnostics(void);

static ENT_FIELD *resolve_entity_field_table(const char *table_name)
{
    if (!table_name)
        return NULL;

    if (strcmp(table_name, "primary") == 0)
        return entity_primary;
    if (strcmp(table_name, "types") == 0)
        return entity_types;
    if (strcmp(table_name, "mobile") == 0)
        return script_entity_fields(ENT_MOBILE);
    if (strcmp(table_name, "object") == 0)
        return script_entity_fields(ENT_OBJECT);
    if (strcmp(table_name, "room") == 0)
        return script_entity_fields(ENT_ROOM);
    if (strcmp(table_name, "string") == 0)
        return script_entity_fields(ENT_STRING);

    return NULL;
}

static int resolve_ifcheck_type_mask(const char *type_name)
{
    if (!type_name || type_name[0] == '\0')
        return IFC_ANY;

    if (strcmp(type_name, "IFC_ANY") == 0)
        return IFC_ANY;
    if (strcmp(type_name, "IFC_M") == 0)
        return IFC_M;
    if (strcmp(type_name, "IFC_O") == 0)
        return IFC_O;
    if (strcmp(type_name, "IFC_R") == 0)
        return IFC_R;
    if (strcmp(type_name, "IFC_T") == 0)
        return IFC_T;
    if (strcmp(type_name, "IFC_A") == 0)
        return IFC_A;
    if (strcmp(type_name, "IFC_I") == 0)
        return IFC_I;
    if (strcmp(type_name, "IFC_D") == 0)
        return IFC_D;

    return IFC_ANY;
}

test_result_t run_script_engine_test_case(test_case_t *test)
{
    if (!test || !test->test_type)
        return TEST_ERROR;

    if (strcmp(test->test_type, "script_engine_entity_lookup_test") == 0)
        return test_script_entity_lookup(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_lookup_test") == 0)
        return test_script_ifcheck_lookup(test);

    if (strcmp(test->test_type, "script_engine_compile_bounds_test") == 0)
        return test_script_compile_string_bounds(test);

    if (strcmp(test->test_type, "script_engine_entity_table_validation_test") == 0)
        return test_script_entity_table_validation(test);

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                  "Unknown script engine test type: %s", test->test_type);
    return TEST_ERROR;
}

static test_result_t test_script_entity_lookup(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    const char *table_name;
    ENT_FIELD *table;
    size_t index;
    json_t *tc;

    if (!test || !test->config)
        return TEST_ERROR;

    input = json_object_get(test->config, "input");
    if (!json_is_object(input))
        return TEST_ERROR;

    table_name = test_json_get_string(input, "table");
    test_cases = json_object_get(input, "test_cases");

    if (!table_name || !json_is_array(test_cases))
        return TEST_ERROR;

    table = resolve_entity_field_table(table_name);
    if (!table) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "Unknown entity field table '%s'", table_name);
        return TEST_ERROR;
    }

    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        bool should_exist = test_json_get_bool(tc, "should_exist");
        int expected_type = test_json_get_int(tc, "expected_type");
        ENT_FIELD *field;

        if (!name)
            return TEST_ERROR;

        field = entity_type_lookup((char *)name, table);

        if (should_exist && !field) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "entity_type_lookup('%s', %s) returned NULL",
                          name, table_name);
            return TEST_FAILURE;
        }

        if (!should_exist && field) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "entity_type_lookup('%s', %s) unexpectedly resolved",
                          name, table_name);
            return TEST_FAILURE;
        }

        if (field && expected_type > 0 && field->type != expected_type) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "entity_type_lookup('%s', %s) type=%d expected=%d",
                          name, table_name, field->type, expected_type);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_lookup(test_case_t *test)
{
    json_t *input;
    json_t *test_cases;
    size_t index;
    json_t *tc;

    if (!test || !test->config)
        return TEST_ERROR;

    input = json_object_get(test->config, "input");
    test_cases = json_object_get(input, "test_cases");

    if (!json_is_array(test_cases))
        return TEST_ERROR;

    json_array_foreach(test_cases, index, tc) {
        const char *name = test_json_get_string(tc, "name");
        const char *type_name = test_json_get_string(tc, "ifc_type");
        bool should_exist = test_json_get_bool(tc, "should_exist");
        int mask = resolve_ifcheck_type_mask(type_name);
        int found;

        if (!name)
            return TEST_ERROR;

        found = ifcheck_lookup((char *)name, mask);

        if (should_exist && found < 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "ifcheck_lookup('%s', %s) returned %d, expected valid",
                          name,
                          type_name ? type_name : "IFC_ANY",
                          found);
            return TEST_FAILURE;
        }

        if (!should_exist && found >= 0) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "ifcheck_lookup('%s', %s) returned %d, expected invalid",
                          name,
                          type_name ? type_name : "IFC_ANY",
                          found);
            return TEST_FAILURE;
        }

        if (found >= 0 && !(ifcheck_table[found].type & mask)) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "ifcheck_lookup('%s', %s) resolved index=%d with mismatched type mask",
                          name,
                          type_name ? type_name : "IFC_ANY",
                          found);
            return TEST_FAILURE;
        }
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_compile_string_bounds(test_case_t *test)
{
    json_t *input;
    int safe_len = (MSL * 2) - 16;
    int overflow_len = (MSL * 2) + 64;
    char *safe_src;
    char *overflow_src;
    char *safe_compiled;
    char *overflow_compiled;
    int compiled_len = 0;

    if (!test || !test->config)
        return TEST_ERROR;

    input = json_object_get(test->config, "input");
    if (json_is_object(input)) {
        int cfg_safe = test_json_get_int(input, "safe_len");
        int cfg_overflow = test_json_get_int(input, "overflow_len");

        if (cfg_safe > 0)
            safe_len = cfg_safe;
        if (cfg_overflow > 0)
            overflow_len = cfg_overflow;
    }

    if (safe_len < 8)
        safe_len = 8;
    if (overflow_len <= safe_len)
        overflow_len = safe_len + 64;

    safe_src = malloc((size_t)safe_len + 1);
    overflow_src = malloc((size_t)overflow_len + 1);
    if (!safe_src || !overflow_src) {
        free(safe_src);
        free(overflow_src);
        return TEST_ERROR;
    }

    memset(safe_src, 'x', (size_t)safe_len);
    safe_src[safe_len] = '\0';

    memset(overflow_src, 'x', (size_t)overflow_len);
    overflow_src[overflow_len] = '\0';

    safe_compiled = compile_string(safe_src, IFC_M, &compiled_len, true);
    if (!safe_compiled) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "compile_string failed at safe length %d", safe_len);
        free(safe_src);
        free(overflow_src);
        return TEST_FAILURE;
    }

    free_mem(safe_compiled, compiled_len + 1);
    compiled_len = 0;

    overflow_compiled = compile_string(overflow_src, IFC_M, &compiled_len, true);
    if (overflow_compiled) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "compile_string unexpectedly succeeded at overflow length %d", overflow_len);
        free_mem(overflow_compiled, compiled_len + 1);
        free(safe_src);
        free(overflow_src);
        return TEST_FAILURE;
    }

    free(safe_src);
    free(overflow_src);
    return TEST_SUCCESS;
}

static test_result_t test_script_entity_table_validation(test_case_t *test)
{
    json_t *input = NULL;
    bool strict = false;
    bool valid;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (json_is_object(input)) {
            strict = test_json_get_bool(input, "strict");
        }
    }

    valid = script_validate_entity_tables();

    if (strict && !valid) {
        log_entity_table_validation_diagnostics();
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_validate_entity_tables() returned false");
        return TEST_FAILURE;
    }

    if (!valid) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                    "script_validate_entity_tables() returned false (non-strict mode)");
    }

    return TEST_SUCCESS;
}

static void log_entity_table_validation_diagnostics(void)
{
    int i;

    for (i = 0; entity_type_info[i].type_min < ENT_MAX; i++) {
        ENT_FIELD *fields = entity_type_info[i].fields;
        int j;

        if (entity_type_info[i].type_min > entity_type_info[i].type_max) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "validator diag: invalid range index=%d min=%d max=%d",
                          i,
                          entity_type_info[i].type_min,
                          entity_type_info[i].type_max);
        }

        if (entity_type_info[i].type_min < ENT_NONE || entity_type_info[i].type_max >= ENT_MAX) {
            log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                          "validator diag: out-of-range registry index=%d min=%d max=%d",
                          i,
                          entity_type_info[i].type_min,
                          entity_type_info[i].type_max);
        }

        for (j = i + 1; entity_type_info[j].type_min < ENT_MAX; j++) {
            if (entity_type_info[i].type_min <= entity_type_info[j].type_max
            && entity_type_info[j].type_min <= entity_type_info[i].type_max) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "validator diag: overlap [%d..%d] with [%d..%d]",
                              entity_type_info[i].type_min,
                              entity_type_info[i].type_max,
                              entity_type_info[j].type_min,
                              entity_type_info[j].type_max);
            }
        }

        if (!fields)
            continue;

        for (j = 0; fields[j].name; j++) {
            int k;

            if (fields[j].name[0] == '\0') {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "validator diag: empty name table=%d field=%d",
                              i,
                              j);
            }

            if (fields[j].code < ESCAPE_EXTRA || fields[j].code >= ESCAPE_UA) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "validator diag: code out-of-band table=%d field='%s' code=%u",
                              i,
                              fields[j].name,
                              (unsigned int)fields[j].code);
            }

            if (fields[j].type != ENT_UNKNOWN
            && (fields[j].type < ENT_NONE || fields[j].type >= ENT_MAX)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                              "validator diag: invalid type table=%d field='%s' type=%u",
                              i,
                              fields[j].name,
                              (unsigned int)fields[j].type);
            }

            for (k = j + 1; fields[k].name; k++) {
                if (!str_cmp(fields[j].name, fields[k].name)) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                  "validator diag: duplicate name table=%d name='%s'",
                                  i,
                                  fields[j].name);
                }
            }
        }
    }
}

#endif /* BUILD_TESTS */
