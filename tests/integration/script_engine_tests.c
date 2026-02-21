#ifdef BUILD_TESTS

#include "../../merc.h"
#include "../../scripts.h"
#include "../../recycle.h"
#include "../../class_data.h"
#include "../framework/test_framework.h"
#include <string.h>
#include <stdlib.h>

static test_result_t test_script_entity_lookup(test_case_t *test);
static test_result_t test_script_ifcheck_lookup(test_case_t *test);
static test_result_t test_script_compile_string_bounds(test_case_t *test);
static test_result_t test_script_compile_error_context_reset(test_case_t *test);
static test_result_t test_script_compile_invalid_syntax(test_case_t *test);
static test_result_t test_script_compile_direct_entity_if(test_case_t *test);
static test_result_t test_script_entity_table_validation(test_case_t *test);
static test_result_t test_script_entity_field_metadata(test_case_t *test);
static test_result_t test_script_entity_field_pressure_report(test_case_t *test);
static test_result_t test_script_entity_high_byte_expansion(test_case_t *test);
static test_result_t test_script_expression_entity_operand(test_case_t *test);
static test_result_t test_script_ifcheck_entity_bitvector_compare(test_case_t *test);
static test_result_t test_script_ifcheck_entity_reference_compare(test_case_t *test);
static test_result_t test_script_ifcheck_entity_truthiness(test_case_t *test);
static test_result_t test_script_ifcheck_entity_string_compare(test_case_t *test);
static test_result_t test_script_ifcheck_room_sector_entity(test_case_t *test);
static test_result_t test_script_event_entity_expansion(test_case_t *test);
static test_result_t test_script_entity_mission_alias_fields(test_case_t *test);
static test_result_t test_script_mission_alias_semantics(test_case_t *test);
static test_result_t test_script_chained_expansion(test_case_t *test);
static test_result_t test_script_vnumname_room_null_progs(test_case_t *test);
static test_result_t test_script_vnumname_match_helpers(test_case_t *test);
static test_result_t test_script_vnumname_owner_context_null_progs(test_case_t *test);
static test_result_t test_script_vnumname_owner_context_exec_parity(test_case_t *test);
static test_result_t test_script_room_resolution_null_hosts(test_case_t *test);
static test_result_t test_script_string_trigger_guard_and_wildcard(test_case_t *test);
static test_result_t test_script_number_trigger_guard_and_wildcard(test_case_t *test);
static test_result_t test_script_number_sight_slot_guard(test_case_t *test);
static test_result_t test_script_direction_trigger_exec(test_case_t *test);
static test_result_t test_script_greet_trigger_exec(test_case_t *test);
static test_result_t test_script_varset_classlevel_mobile_class(test_case_t *test);
static void log_entity_table_validation_diagnostics(void);

int test_vnumname_trigger(char *name, int vnum, AREA_DATA *entity_area, int type,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2,
            char *phrase);
int test_number_sight_trigger(int number, int wildcard, bool (*match)(int a, int b), int type, int typeall,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2,
            char *phrase);
CHAR_DATA *get_random_char(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token);
int count_people_room(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, int iFlag);
int get_order(CHAR_DATA *ch, OBJ_DATA *obj);
CHAR_DATA *get_mob_vnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, long vnum, AREA_DATA *area);
OBJ_DATA *get_obj_vnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, long vnum, AREA_DATA *area);
CHAR_DATA *script_get_char_room(SCRIPT_VARINFO *info, char *name, bool see_all);
OBJ_DATA *script_get_obj_here(SCRIPT_VARINFO *info, char *name);
int ifcheck_comparison(SCRIPT_VARINFO *info, short param, char *rest, SCRIPT_PARAM *arg);

static bool test_match_equal(int a, int b)
{
    return a == b;
}

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
    if (strcmp(table_name, "area") == 0)
        return script_entity_fields(ENT_AREA);
    if (strcmp(table_name, "aregion") == 0)
        return script_entity_fields(ENT_AREA_REGION);
    if (strcmp(table_name, "event") == 0)
        return script_entity_fields(ENT_EVENT);
    if (strcmp(table_name, "race") == 0)
        return script_entity_fields(ENT_RACE);
    if (strcmp(table_name, "class") == 0)
        return script_entity_fields(ENT_CLASS);
    if (strcmp(table_name, "classlevel") == 0)
        return script_entity_fields(ENT_CLASSLEVEL);
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

    if (strcmp(test->test_type, "script_engine_compile_error_context_reset_test") == 0)
        return test_script_compile_error_context_reset(test);

    if (strcmp(test->test_type, "script_engine_compile_invalid_syntax_test") == 0)
        return test_script_compile_invalid_syntax(test);

    if (strcmp(test->test_type, "script_engine_compile_direct_entity_if_test") == 0)
        return test_script_compile_direct_entity_if(test);

    if (strcmp(test->test_type, "script_engine_entity_table_validation_test") == 0)
        return test_script_entity_table_validation(test);

    if (strcmp(test->test_type, "script_engine_entity_field_metadata_test") == 0)
        return test_script_entity_field_metadata(test);

    if (strcmp(test->test_type, "script_engine_entity_field_pressure_report_test") == 0)
        return test_script_entity_field_pressure_report(test);

    if (strcmp(test->test_type, "script_engine_entity_high_byte_expansion_test") == 0)
        return test_script_entity_high_byte_expansion(test);

    if (strcmp(test->test_type, "script_engine_expression_entity_operand_test") == 0)
        return test_script_expression_entity_operand(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_entity_bitvector_compare_test") == 0)
        return test_script_ifcheck_entity_bitvector_compare(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_entity_reference_compare_test") == 0)
        return test_script_ifcheck_entity_reference_compare(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_entity_truthiness_test") == 0)
        return test_script_ifcheck_entity_truthiness(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_entity_string_compare_test") == 0)
        return test_script_ifcheck_entity_string_compare(test);

    if (strcmp(test->test_type, "script_engine_ifcheck_room_sector_entity_test") == 0)
        return test_script_ifcheck_room_sector_entity(test);

    if (strcmp(test->test_type, "script_engine_event_entity_expansion_test") == 0)
        return test_script_event_entity_expansion(test);

    if (strcmp(test->test_type, "script_engine_entity_mission_alias_fields_test") == 0)
        return test_script_entity_mission_alias_fields(test);

    if (strcmp(test->test_type, "script_engine_mission_alias_semantics_test") == 0)
        return test_script_mission_alias_semantics(test);

    if (strcmp(test->test_type, "script_engine_chained_expansion_test") == 0)
        return test_script_chained_expansion(test);

    if (strcmp(test->test_type, "script_engine_vnumname_room_null_progs_test") == 0)
        return test_script_vnumname_room_null_progs(test);

    if (strcmp(test->test_type, "script_engine_vnumname_match_helpers_test") == 0)
        return test_script_vnumname_match_helpers(test);

    if (strcmp(test->test_type, "script_engine_vnumname_owner_context_null_progs_test") == 0)
        return test_script_vnumname_owner_context_null_progs(test);

    if (strcmp(test->test_type, "script_engine_vnumname_owner_context_exec_parity_test") == 0)
        return test_script_vnumname_owner_context_exec_parity(test);

    if (strcmp(test->test_type, "script_engine_room_resolution_null_hosts_test") == 0)
        return test_script_room_resolution_null_hosts(test);

    if (strcmp(test->test_type, "script_engine_string_trigger_guard_wildcard_test") == 0)
        return test_script_string_trigger_guard_and_wildcard(test);

    if (strcmp(test->test_type, "script_engine_number_trigger_guard_wildcard_test") == 0)
        return test_script_number_trigger_guard_and_wildcard(test);

    if (strcmp(test->test_type, "script_engine_number_sight_slot_guard_test") == 0)
        return test_script_number_sight_slot_guard(test);

    if (strcmp(test->test_type, "script_engine_direction_trigger_exec_test") == 0)
        return test_script_direction_trigger_exec(test);

    if (strcmp(test->test_type, "script_engine_greet_trigger_exec_test") == 0)
        return test_script_greet_trigger_exec(test);

    if (strcmp(test->test_type, "script_engine_varset_classlevel_mobile_class_test") == 0)
        return test_script_varset_classlevel_mobile_class(test);

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

static test_result_t test_script_varset_classlevel_mobile_class(test_case_t *test)
{
    CHAR_DATA player;
    PC_DATA pcdata;
    CLASS_LEVEL *expected_class_level;
    CLASS_DATA *clazz;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    pVARIABLE vars = NULL;
    pVARIABLE out;
    char command[MSL];
    char *compiled = NULL;
    int compiled_len = 0;
    (void)test;

    memset(&player, 0, sizeof(player));
    memset(&pcdata, 0, sizeof(pcdata));
    memset(&info, 0, sizeof(info));

    clazz = class_find("gladiator");
    if (!clazz)
        clazz = class_get_default();
    if (!clazz) {
        log_message(LOG_LEVEL_WARN, LOG_UNIT_TESTS,
                    "No class data available; skipping varset classlevel test");
        return TEST_SKIP;
    }

    player.valid = true;
    player.pcdata = &pcdata;

    add_class_level(&player, clazz, 12);
    expected_class_level = get_class_level(&player, clazz);
    if (!expected_class_level) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_class_level failed after add_class_level in varset classlevel test");
        variable_freelist(&vars);
        return TEST_ERROR;
    }
    expected_class_level->xp = 12345;

    info.var = &vars;

    info.ch = &player;

    if (!variables_set_mobile(&vars, "srcmob", &player)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "variables_set_mobile failed in varset classlevel test");
        variable_freelist(&vars);
        remove_class_level(&player, clazz);
        return TEST_ERROR;
    }

    snprintf(command, sizeof(command), "dst classlevel $(srcmob) \"%s\"", class_name(clazz));

    compiled = compile_string(command, IFC_ANY, &compiled_len, true);
    if (!compiled) {
        variable_freelist(&vars);
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for varset classlevel command");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        variable_freelist(&vars);
        return TEST_ERROR;
    }

    script_varseton(&info, &vars, compiled, arg);

    out = variable_get(vars, "dst");
    if (!out || out->type != VAR_CLASSLEVEL || !out->_.classlevel ||
        out->_.classlevel->clazz != clazz || out->_.classlevel->level != expected_class_level->level) {
        free_script_param(arg);
        free_mem(compiled, compiled_len + 1);
        variable_freelist(&vars);
        remove_class_level(&player, clazz);
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "varset classlevel mismatch: out=%p type=%d cl=%p cl_class=%p expected_class=%p cl_level=%d expected_level=%d",
                      (void *)out,
                      out ? out->type : -1,
                      out && out->type == VAR_CLASSLEVEL ? (void *)out->_.classlevel : NULL,
                      out && out->type == VAR_CLASSLEVEL && out->_.classlevel ? (void *)out->_.classlevel->clazz : NULL,
                      (void *)clazz,
                      out && out->type == VAR_CLASSLEVEL && out->_.classlevel ? out->_.classlevel->level : -1,
                      expected_class_level->level);
        return TEST_FAILURE;
    }

    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    variable_freelist(&vars);
    remove_class_level(&player, clazz);
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

static test_result_t test_script_compile_error_context_reset(test_case_t *test)
{
    SCRIPT_DATA script;
    BUFFER *err_buf;
    char *compile_source;
    char *overflow_src;
    char *compiled;
    int compiled_len = 0;
    int overflow_len = (MSL * 2) + 64;
    (void)test;

    memset(&script, 0, sizeof(script));

    err_buf = new_buf();
    if (!err_buf)
        return TEST_ERROR;

    compile_source = str_dup("end\n");
    script.vnum = 900002;
    if (!compile_script(err_buf, &script, compile_source, IFC_M)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "compile_script failed in context reset test: %s",
                      buf_string(err_buf));
        free_buf(err_buf);
        free_string(compile_source);
        return TEST_FAILURE;
    }

    free_buf(err_buf);

    overflow_src = malloc((size_t)overflow_len + 1);
    if (!overflow_src) {
        free_script_code(script.code, script.lines);
        free_string(script.src);
        return TEST_ERROR;
    }

    memset(overflow_src, 'x', (size_t)overflow_len);
    overflow_src[overflow_len] = '\0';

    compiled = compile_string(overflow_src, IFC_M, &compiled_len, true);
    if (compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string unexpectedly succeeded after compile_script context reset scenario");
        free_mem(compiled, compiled_len + 1);
        free(overflow_src);
        free_script_code(script.code, script.lines);
        free_string(script.src);
        return TEST_FAILURE;
    }

    free(overflow_src);
    free_script_code(script.code, script.lines);
    free_string(script.src);

    return TEST_SUCCESS;
}

static test_result_t test_script_compile_invalid_syntax(test_case_t *test)
{
    SCRIPT_DATA script;
    BUFFER *err_buf;
    char *compile_source;
    char *overflow_src;
    char *compiled;
    int compiled_len = 0;
    int overflow_len = (MSL * 2) + 64;
    bool compiled_ok;
    const char *diagnostics;
    (void)test;

    memset(&script, 0, sizeof(script));

    err_buf = new_buf();
    if (!err_buf)
        return TEST_ERROR;

    compile_source = str_dup("if definitely_not_ifcheck 1\nend\n");
    script.vnum = 900003;
    compiled_ok = compile_script(err_buf, &script, compile_source, IFC_M);

    if (compiled_ok) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_script unexpectedly succeeded for intentionally invalid source");
        free_script_code(script.code, script.lines);
        free_string(script.src);
        free_buf(err_buf);
        free_string(compile_source);
        return TEST_FAILURE;
    }

    diagnostics = buf_string(err_buf);
    if (!diagnostics || diagnostics[0] == '\0') {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_script failure did not emit diagnostics for invalid source");
        free_buf(err_buf);
        free_string(compile_source);
        return TEST_FAILURE;
    }

    free_buf(err_buf);

    overflow_src = malloc((size_t)overflow_len + 1);
    if (!overflow_src) {
        free_string(compile_source);
        return TEST_ERROR;
    }

    memset(overflow_src, 'x', (size_t)overflow_len);
    overflow_src[overflow_len] = '\0';

    compiled = compile_string(overflow_src, IFC_M, &compiled_len, true);
    if (compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string unexpectedly succeeded after invalid compile_script scenario");
        free_mem(compiled, compiled_len + 1);
        free(overflow_src);
        free_string(compile_source);
        return TEST_FAILURE;
    }

    free(overflow_src);
    free_string(compile_source);
    return TEST_SUCCESS;
}

static test_result_t test_script_compile_direct_entity_if(test_case_t *test)
{
    SCRIPT_DATA script;
    BUFFER *err_buf;
    char *compile_source;
    bool compiled_ok;
    (void)test;

    memset(&script, 0, sizeof(script));

    err_buf = new_buf();
    if (!err_buf)
        return TEST_ERROR;

    compile_source = str_dup(
        "if $(enactor.mount)\n"
        "endif\n"
        "if $(enactor.offense)\n"
        "endif\n"
        "if $(enactor.mount) == $(self)\n"
        "endif\n");

    script.vnum = 900004;
    compiled_ok = compile_script(err_buf, &script, compile_source, IFC_M);
    if (!compiled_ok) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "compile_script failed for direct entity-if syntax: %s",
                      buf_string(err_buf));
        free_buf(err_buf);
        free_string(compile_source);
        return TEST_FAILURE;
    }

    free_buf(err_buf);
    free_script_code(script.code, script.lines);
    free_string(script.src);
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

static test_result_t test_script_entity_field_metadata(test_case_t *test)
{
    ENT_FIELD *primary_table;
    ENT_FIELD *field_mxp;
    ENT_FIELD *field_tab;
    const char *desc;
    (void)test;

    primary_table = entity_primary;

    field_mxp = entity_type_lookup("mxp", primary_table);
    if (!field_mxp) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "entity_type_lookup('mxp', entity_primary) returned NULL");
        return TEST_FAILURE;
    }

    desc = script_entity_field_description(field_mxp);
    if (!desc || desc[0] == '\0') {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_entity_field_description('mxp') missing metadata");
        return TEST_FAILURE;
    }

    if (script_entity_field_deprecated(field_mxp)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_entity_field_deprecated('mxp') unexpectedly true");
        return TEST_FAILURE;
    }

    field_tab = entity_type_lookup("tab", primary_table);
    if (!field_tab) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "entity_type_lookup('tab', entity_primary) returned NULL");
        return TEST_FAILURE;
    }

    if (!script_entity_field_deprecated(field_tab)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_entity_field_deprecated('tab') expected true");
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_entity_field_pressure_report(test_case_t *test)
{
    json_t *input = NULL;
    int threshold = 75;

    if (test && test->config) {
        input = json_object_get(test->config, "input");
        if (json_is_object(input)) {
            int cfg_threshold = test_json_get_int(input, "threshold_pct");
            if (cfg_threshold >= 0 && cfg_threshold <= 100)
                threshold = cfg_threshold;
        }
    }

    script_log_entity_field_pressure_report(threshold);
    return TEST_SUCCESS;
}

static test_result_t test_script_entity_high_byte_expansion(test_case_t *test)
{
    static const char *candidate_names[] = {
        "event_phase",
        "event_goal",
        "event_items",
        "event_kills",
        "event_active",
        "event_bracket",
        "event_instance",
        "event_uid",
        "tempstring",
        "vuln",
        NULL
    };
    pVARIABLE vars = NULL;
    SCRIPT_VARINFO info;
    BUFFER *expanded;
    ENT_FIELD *mobile_table;
    ENT_FIELD *target_field = NULL;
    char expression[128];
    char *compiled = NULL;
    int compiled_len = 0;
    bool ok;
    int i;
    (void)test;

    mobile_table = script_entity_fields(ENT_MOBILE);
    if (!mobile_table)
        return TEST_ERROR;

    for (i = 0; candidate_names[i] != NULL; i++) {
        ENT_FIELD *field = entity_type_lookup((char *)candidate_names[i], mobile_table);
        if (field && field->code >= ESCAPE_UA) {
            target_field = field;
            break;
        }
    }

    if (!target_field) {
        log_message(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                    "No high-byte mobile entity field available; skipping expansion regression");
        return TEST_SKIP;
    }

    memset(&info, 0, sizeof(info));
    info.var = &vars;

    expanded = new_buf();
    if (!expanded)
        return TEST_ERROR;

    snprintf(expression, sizeof(expression), "$(enactor.%s)", target_field->name);

    compiled = compile_string(expression, IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "compile_string failed for high-byte entity field '%s' (code=%u)",
                      target_field->name,
                      (unsigned int)target_field->code);
        free_buf(expanded);
        return TEST_FAILURE;
    }

    clear_buf(expanded);
    ok = expand_string(&info, compiled, expanded);
    if (!ok) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "expand_string failed for high-byte entity field '%s' (code=%u)",
                      target_field->name,
                      (unsigned int)target_field->code);
        free_mem(compiled, compiled_len + 1);
        free_buf(expanded);
        return TEST_FAILURE;
    }

    free_mem(compiled, compiled_len + 1);
    free_buf(expanded);
    return TEST_SUCCESS;
}

static test_result_t test_script_expression_entity_operand(test_case_t *test)
{
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    pVARIABLE vars = NULL;
    SCRIPT_VARINFO info;
    BUFFER *expanded;
    char *compiled = NULL;
    int compiled_len = 0;
    bool ok;
    (void)test;

    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&info, 0, sizeof(info));

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.off_flags = 4;

    info.var = &vars;
    info.ch = &mob;

    expanded = new_buf();
    if (!expanded)
        return TEST_ERROR;

    compiled = compile_string("$[$(enactor.offense)+2]", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity arithmetic expression");
        free_buf(expanded);
        return TEST_FAILURE;
    }

    clear_buf(expanded);
    ok = expand_string(&info, compiled, expanded);
    if (!ok || str_cmp(buf_string(expanded), "6")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "entity arithmetic expression mismatch: got='%s' expected='6'",
                      buf_string(expanded));
        free_mem(compiled, compiled_len + 1);
        free_buf(expanded);
        return TEST_FAILURE;
    }

    free_mem(compiled, compiled_len + 1);
    free_buf(expanded);
    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_entity_bitvector_compare(test_case_t *test)
{
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&info, 0, sizeof(info));

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.off_flags = 8;

    info.ch = &mob;

    compiled = compile_string("$(enactor.offense) > 1", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for bitvector ifcheck comparison");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "ifcheck bitvector comparison returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_entity_reference_compare(test_case_t *test)
{
    CHAR_DATA enactor;
    CHAR_DATA self;
    CHAR_DATA other;
    MOB_INDEX_DATA enactor_index;
    MOB_INDEX_DATA self_index;
    MOB_INDEX_DATA other_index;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&enactor, 0, sizeof(enactor));
    memset(&self, 0, sizeof(self));
    memset(&other, 0, sizeof(other));
    memset(&enactor_index, 0, sizeof(enactor_index));
    memset(&self_index, 0, sizeof(self_index));
    memset(&other_index, 0, sizeof(other_index));
    memset(&info, 0, sizeof(info));

    enactor.valid = true;
    enactor.act[0] = ACT_IS_NPC;
    enactor.pIndexData = &enactor_index;
    enactor.mount = &self;

    self.valid = true;
    self.act[0] = ACT_IS_NPC;
    self.pIndexData = &self_index;

    other.valid = true;
    other.act[0] = ACT_IS_NPC;
    other.pIndexData = &other_index;

    info.ch = &enactor;
    info.mob = &self;

    compiled = compile_string("$(enactor.mount) == $(self)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity reference equality comparison");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "ifcheck entity reference equality returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    info.mob = &other;

    compiled = compile_string("$(enactor.mount) == $(self)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity reference inequality comparison");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "ifcheck entity reference inequality returned %d, expected 0",
                      result);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_entity_truthiness(test_case_t *test)
{
    CHAR_DATA enactor;
    CHAR_DATA self;
    MOB_INDEX_DATA enactor_index;
    MOB_INDEX_DATA self_index;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&enactor, 0, sizeof(enactor));
    memset(&self, 0, sizeof(self));
    memset(&enactor_index, 0, sizeof(enactor_index));
    memset(&self_index, 0, sizeof(self_index));
    memset(&info, 0, sizeof(info));

    enactor.valid = true;
    enactor.act[0] = ACT_IS_NPC;
    enactor.pIndexData = &enactor_index;
    enactor.mount = &self;
    enactor.off_flags = 8;

    self.valid = true;
    self.act[0] = ACT_IS_NPC;
    self.pIndexData = &self_index;

    info.ch = &enactor;
    info.mob = &self;

    compiled = compile_string("$(enactor.mount)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity truthiness expression");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "entity truthiness returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(enactor.offense)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for numeric truthiness expression");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "numeric truthiness returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    enactor.mount = NULL;
    enactor.off_flags = 0;

    compiled = compile_string("$(enactor.mount)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for false entity truthiness expression");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "entity false truthiness returned %d, expected 0",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(enactor.offense)", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for false numeric truthiness expression");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "numeric false truthiness returned %d, expected 0",
                      result);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_entity_string_compare(test_case_t *test)
{
    CHAR_DATA self;
    MOB_INDEX_DATA self_index;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&self, 0, sizeof(self));
    memset(&self_index, 0, sizeof(self_index));
    memset(&info, 0, sizeof(info));

    self.valid = true;
    self.act[0] = ACT_IS_NPC;
    self.pIndexData = &self_index;
    self.name = "script_test_mob";

    info.mob = &self;

    compiled = compile_string("$(self.name) == script_test_mob", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity string equality comparison");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "entity string equality returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(self.name) != wrong_name", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for entity string inequality comparison");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "entity string inequality returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_ifcheck_room_sector_entity(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    SECTOR_RUNTIME_DATA sector;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&sector, 0, sizeof(sector));
    memset(&info, 0, sizeof(info));

    sector.name = "test_sector";
    sector.description = "test sector description";
    sector.comments = "test sector comments";
    sector.flags = SECTOR_NATURE;
    sector.move_cost = 3;
    sector.heal_rate = 105;
    sector.mana_rate = 110;
    sector.move_rate = 95;
    sector.sector_class = 2;
    sector.soil = 1;
    sector.affinities[0].catalyst = 1;
    sector.affinities[0].value = 7;
    sector.affinities[1].catalyst = 2;
    sector.affinities[1].value = 11;
    sector.affinities[2].catalyst = 3;
    sector.affinities[2].value = 13;

    room.sector = &sector;
    info.room = &room;

    compiled = compile_string("$(here.sector) == $(here.sector)", IFC_R, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for room sector entity compare");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "room sector entity compare returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(here.sector.name) == test_sector", IFC_R, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for room sector nested string compare");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "room sector name compare returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(here.sectorflags) != 0", IFC_R, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for room sectorflags compare");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "room sectorflags compare returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(here.sector.affinity1_catalyst) == 1", IFC_R, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for sector affinity catalyst compare");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "sector affinity catalyst compare returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    compiled = compile_string("$(here.sector.affinity3_value) == 13", IFC_R, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for sector affinity value compare");
        return TEST_FAILURE;
    }

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);

    if (result != 1) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "sector affinity value compare returned %d, expected 1",
                      result);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_event_entity_expansion(test_case_t *test)
{
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&info, 0, sizeof(info));

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.event_source_uid = 321;
    mob.event_source_instance_id = 17;

    info.mob = &mob;
    info.ch = &mob;

    compiled = compile_string("$(self.event.uid) == 321", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }
    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    compiled = compile_string("$(self.event.instance) == 17", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }
    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    compiled = compile_string("$(self.event.source_uid) == $(self.event.event_uid)", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }
    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    return TEST_SUCCESS;
}

static test_result_t test_script_mission_alias_semantics(test_case_t *test)
{
    CHAR_DATA player;
    PC_DATA pcdata;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg_mobile;
    SCRIPT_PARAM *argv[1];
    int quest_ret = -1;
    int mission_ret = -1;
    int totalquests_ret = -1;
    int totalmissions_ret = -1;
    int isquesting_ret = -1;
    int onmission_ret = -1;
    bool ok_quest;
    bool ok_mission;
    bool ok_totalquests;
    bool ok_totalmissions;
    bool ok_isquesting;
    bool ok_onmission;
    (void)test;

    memset(&player, 0, sizeof(player));
    memset(&pcdata, 0, sizeof(pcdata));
    memset(&info, 0, sizeof(info));

    player.valid = true;
    player.pcdata = &pcdata;
    player.questpoints = 73;
    player.pcdata->quests_completed = 19;

    info.ch = &player;

    arg_mobile = new_script_param();
    if (!arg_mobile)
        return TEST_ERROR;

    arg_mobile->type = ENT_MOBILE;
    arg_mobile->d.mob = &player;
    argv[0] = arg_mobile;

    ok_quest = ifc_quest(&info, NULL, NULL, NULL, NULL, NULL, &quest_ret, 1, argv);
    ok_mission = ifc_mission(&info, NULL, NULL, NULL, NULL, NULL, &mission_ret, 1, argv);
    ok_totalquests = ifc_totalquests(&info, NULL, NULL, NULL, NULL, NULL, &totalquests_ret, 1, argv);
    ok_totalmissions = ifc_totalmissions(&info, NULL, NULL, NULL, NULL, NULL, &totalmissions_ret, 1, argv);
    ok_isquesting = ifc_isquesting(&info, NULL, NULL, NULL, NULL, NULL, &isquesting_ret, 1, argv);
    ok_onmission = ifc_onmission(&info, NULL, NULL, NULL, NULL, NULL, &onmission_ret, 1, argv);

    free_script_param(arg_mobile);

    if (!ok_quest || !ok_mission || !ok_totalquests || !ok_totalmissions || !ok_isquesting || !ok_onmission) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "mission alias ifcheck invocation failed");
        return TEST_FAILURE;
    }

    if (quest_ret != mission_ret) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "missionpoint alias mismatch: quest=%d mission=%d",
                      quest_ret, mission_ret);
        return TEST_FAILURE;
    }

    if (totalquests_ret != totalmissions_ret) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "totalmissions alias mismatch: totalquests=%d totalmissions=%d",
                      totalquests_ret, totalmissions_ret);
        return TEST_FAILURE;
    }

    if (isquesting_ret != onmission_ret) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "onmission alias mismatch: isquesting=%d onmission=%d",
                      isquesting_ret, onmission_ret);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_entity_mission_alias_fields(test_case_t *test)
{
    CHAR_DATA player;
    PC_DATA pcdata;
    QUEST_DATA quest;
    SCRIPT_VARINFO info;
    SCRIPT_PARAM *arg;
    char *compiled = NULL;
    int compiled_len = 0;
    int result;
    (void)test;

    memset(&player, 0, sizeof(player));
    memset(&pcdata, 0, sizeof(pcdata));
    memset(&quest, 0, sizeof(quest));
    memset(&info, 0, sizeof(info));

    player.valid = true;
    player.pcdata = &pcdata;
    player.questpoints = 73;
    player.pcdata->quests_completed = 19;
    player.quest = &quest;

    info.mob = &player;

    compiled = compile_string("$(self.missionpoint) == 73", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    compiled = compile_string("$(self.totalmissions) == 19", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    compiled = compile_string("$(self.onmission)", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 1)
        return TEST_FAILURE;

    player.quest = 0;

    compiled = compile_string("$(self.onmission)", IFC_M, &compiled_len, true);
    if (!compiled)
        return TEST_FAILURE;

    arg = new_script_param();
    if (!arg) {
        free_mem(compiled, compiled_len + 1);
        return TEST_ERROR;
    }

    result = ifcheck_comparison(&info, -1, compiled, arg);
    free_script_param(arg);
    free_mem(compiled, compiled_len + 1);
    if (result != 0)
        return TEST_FAILURE;

    return TEST_SUCCESS;
}

static test_result_t test_script_chained_expansion(test_case_t *test)
{
    pVARIABLE vars = NULL;
    SCRIPT_VARINFO info;
    BUFFER *expanded;
    char *compiled;
    int compiled_len;
    bool ok;
    (void)test;

    memset(&info, 0, sizeof(info));
    info.var = &vars;

    expanded = new_buf();
    if (!expanded)
        return TEST_ERROR;

    if (!variables_set_string(&vars, "suffix", "2", false)
    || !variables_set_string(&vars, "name2", "alpha", false)
    || !variables_set_string(&vars, "name3", "beta", false)) {
        free_buf(expanded);
        variable_freelist(&vars);
        return TEST_ERROR;
    }

    compiled_len = 0;
    compiled = compile_string("$<name<suffix>>", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for nested variable chain");
        free_buf(expanded);
        variable_freelist(&vars);
        return TEST_FAILURE;
    }

    clear_buf(expanded);
    ok = expand_string(&info, compiled, expanded);
    if (!ok || str_cmp(buf_string(expanded), "alpha")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "nested variable chain mismatch: got='%s' expected='alpha'",
                      buf_string(expanded));
        free_mem(compiled, compiled_len + 1);
        free_buf(expanded);
        variable_freelist(&vars);
        return TEST_FAILURE;
    }
    free_mem(compiled, compiled_len + 1);

    compiled_len = 0;
    compiled = compile_string("prefix-$<name[1+2]>-$[2*3]", IFC_M, &compiled_len, true);
    if (!compiled) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "compile_string failed for mixed expression/variable chain");
        free_buf(expanded);
        variable_freelist(&vars);
        return TEST_FAILURE;
    }

    clear_buf(expanded);
    ok = expand_string(&info, compiled, expanded);
    if (!ok || str_cmp(buf_string(expanded), "prefix-beta-6")) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "mixed chained expansion mismatch: got='%s' expected='prefix-beta-6'",
                      buf_string(expanded));
        free_mem(compiled, compiled_len + 1);
        free_buf(expanded);
        variable_freelist(&vars);
        return TEST_FAILURE;
    }
    free_mem(compiled, compiled_len + 1);

    free_buf(expanded);
    variable_freelist(&vars);
    return TEST_SUCCESS;
}

static test_result_t test_script_vnumname_room_null_progs(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    int ret;
    const int trigger_type = TRIG_GIVE;
    (void)test;

    memset(&room, 0, sizeof(room));
    room.source = NULL;
    room.progs = NULL;
    room.ltokens = NULL;

    ret = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                NULL, NULL, &room,
                                NULL, NULL, NULL,
                                NULL, NULL,
                                NULL);

    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger room/null-progs returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_vnumname_match_helpers(test_case_t *test)
{
    PROG_LIST prg;
    AREA_DATA area_a;
    AREA_DATA area_b;
    (void)test;

    memset(&prg, 0, sizeof(prg));
    memset(&area_a, 0, sizeof(area_a));
    memset(&area_b, 0, sizeof(area_b));

    prg.numeric = true;
    prg.trig_is_widevnum = false;
    prg.trig_number = 42;

    if (!script_vnumname_match_primary(&prg, &area_a, 42, "anything")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "legacy numeric primary match failed for equal vnum");
        return TEST_FAILURE;
    }
    if (script_vnumname_match_primary(&prg, &area_a, 41, "anything")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "legacy numeric primary match unexpectedly succeeded for different vnum");
        return TEST_FAILURE;
    }

    prg.trig_is_widevnum = true;
    prg.trig_wnum.pArea = &area_a;
    prg.trig_wnum.vnum = 10;

    if (!script_vnumname_match_primary(&prg, &area_a, 10, "anything")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "widevnum primary match failed for same area+vnum");
        return TEST_FAILURE;
    }
    if (script_vnumname_match_primary(&prg, &area_b, 10, "anything")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "widevnum primary match unexpectedly succeeded for different area");
        return TEST_FAILURE;
    }

    prg.numeric = false;
    prg.trig_phrase = "all sword";
    if (!script_vnumname_match_primary(&prg, &area_a, 10, "dagger")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "name primary match failed for 'all' wildcard token");
        return TEST_FAILURE;
    }
    prg.trig_phrase = "sword axe";
    if (script_vnumname_match_primary(&prg, &area_a, 10, "dagger")) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "name primary match unexpectedly succeeded for non-matching target name");
        return TEST_FAILURE;
    }

    prg.numeric = true;
    prg.trig_is_widevnum = false;
    prg.trig_number = 0;
    if (!script_vnumname_match_wildcard(&prg)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "wildcard match failed for legacy numeric zero");
        return TEST_FAILURE;
    }

    prg.trig_is_widevnum = true;
    if (script_vnumname_match_wildcard(&prg)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "wildcard match unexpectedly succeeded for widevnum trigger");
        return TEST_FAILURE;
    }

    prg.numeric = false;
    prg.trig_phrase = "*";
    if (!script_vnumname_match_wildcard(&prg)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "wildcard match failed for '*' phrase trigger");
        return TEST_FAILURE;
    }

    prg.trig_phrase = "not_star";
    if (script_vnumname_match_wildcard(&prg)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "wildcard match unexpectedly succeeded for non-'*' phrase trigger");
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_vnumname_owner_context_null_progs(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    OBJ_DATA obj;
    OBJ_INDEX_DATA obj_index;
    int ret_mob;
    int ret_obj;
    int ret_room;
    const int trigger_type = TRIG_GIVE;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&obj, 0, sizeof(obj));
    memset(&obj_index, 0, sizeof(obj_index));

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;

    obj.valid = true;
    obj.pIndexData = &obj_index;
    obj.in_room = &room;

    room.source = NULL;
    room.progs = NULL;
    room.ltokens = NULL;

    ret_mob = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                    &mob, NULL, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL);

    if (ret_mob != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger mob/null-progs returned %d, expected %d",
                      ret_mob, PRET_NOSCRIPT);
        return TEST_FAILURE;
    }

    ret_obj = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                    NULL, &obj, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL);

    if (ret_obj != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger obj/null-progs returned %d, expected %d",
                      ret_obj, PRET_NOSCRIPT);
        return TEST_FAILURE;
    }

    ret_room = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                     NULL, NULL, &room,
                                     NULL, NULL, NULL,
                                     NULL, NULL,
                                     NULL);

    if (ret_room != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger room/null-progs returned %d, expected %d",
                      ret_room, PRET_NOSCRIPT);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_vnumname_owner_context_exec_parity(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    OBJ_DATA obj;
    OBJ_INDEX_DATA obj_index;
    PROG_LIST *mob_prg;
    PROG_LIST *obj_prg;
    PROG_LIST *room_prg;
    SCRIPT_DATA script;
    int ret_mob;
    int ret_obj;
    int ret_room;
    const int trigger_type = TRIG_GIVE;
    const int trigger_slot = trigger_table[TRIG_GIVE].slot;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&obj, 0, sizeof(obj));
    memset(&obj_index, 0, sizeof(obj_index));
    memset(&script, 0, sizeof(script));

    script.vnum = 900001;
    script.code = alloc_mem(sizeof(SCRIPT_CODE));
    if (!script.code)
        return TEST_ERROR;
    memset(script.code, 0, sizeof(SCRIPT_CODE));
    script.code[0].opcode = OP_END;
    script.code[0].level = 0;
    script.code[0].rest = str_dup("");
    script.code[0].length = 0;
    script.lines = 1;

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;
    mob.progs = new_prog_data();

    obj.valid = true;
    obj.pIndexData = &obj_index;
    obj.in_room = &room;
    obj.progs = new_prog_data();

    room.progs = new_prog_data();
    room.source = NULL;
    room.ltokens = NULL;

    mob_index.progs = new_prog_bank();
    obj_index.progs = new_prog_bank();
    room.progs->progs = new_prog_bank();

    mob_prg = new_trigger();
    mob_prg->trig_type = trigger_type;
    mob_prg->numeric = true;
    mob_prg->trig_number = 1;
    mob_prg->vnum = 900001;
    mob_prg->script = &script;
    list_appendlink(mob_index.progs[trigger_slot], mob_prg);

    obj_prg = new_trigger();
    obj_prg->trig_type = trigger_type;
    obj_prg->numeric = true;
    obj_prg->trig_number = 1;
    obj_prg->vnum = 900001;
    obj_prg->script = &script;
    list_appendlink(obj_index.progs[trigger_slot], obj_prg);

    room_prg = new_trigger();
    room_prg->trig_type = trigger_type;
    room_prg->numeric = true;
    room_prg->trig_number = 1;
    room_prg->vnum = 900001;
    room_prg->script = &script;
    list_appendlink(room.progs->progs[trigger_slot], room_prg);

    ret_mob = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                    &mob, NULL, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL);
    if (ret_mob != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger mob/exec-parity returned %d, expected %d",
                      ret_mob, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_list(obj_index.progs);
        free_prog_list(room.progs->progs);
        free_prog_data(mob.progs);
        free_prog_data(obj.progs);
        free_prog_data(room.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    ret_obj = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                    NULL, &obj, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL);
    if (ret_obj != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger obj/exec-parity returned %d, expected %d",
                      ret_obj, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_list(obj_index.progs);
        free_prog_list(room.progs->progs);
        free_prog_data(mob.progs);
        free_prog_data(obj.progs);
        free_prog_data(room.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    ret_room = test_vnumname_trigger("dummy", 1, NULL, trigger_type,
                                     NULL, NULL, &room,
                                     NULL, NULL, NULL,
                                     NULL, NULL,
                                     NULL);
    if (ret_room != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_vnumname_trigger room/exec-parity returned %d, expected %d",
                      ret_room, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_list(obj_index.progs);
        free_prog_list(room.progs->progs);
        free_prog_data(mob.progs);
        free_prog_data(obj.progs);
        free_prog_data(room.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    free_prog_list(mob_index.progs);
    free_prog_list(obj_index.progs);
    free_prog_list(room.progs->progs);
    free_prog_data(mob.progs);
    free_prog_data(obj.progs);
    free_prog_data(room.progs);
    free_script_code(script.code, script.lines);

    return TEST_SUCCESS;
}

static test_result_t test_script_room_resolution_null_hosts(test_case_t *test)
{
    OBJ_DATA obj;
    TOKEN_DATA token;
    CHAR_DATA mob;
    SCRIPT_VARINFO info;
    (void)test;

    memset(&obj, 0, sizeof(obj));
    memset(&token, 0, sizeof(token));
    memset(&mob, 0, sizeof(mob));
    memset(&info, 0, sizeof(info));

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.in_room = NULL;

    if (get_random_char(NULL, &obj, NULL, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_random_char unexpectedly resolved for object with no room context");
        return TEST_FAILURE;
    }

    if (get_random_char(NULL, NULL, NULL, &token) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_random_char unexpectedly resolved for token with no room context");
        return TEST_FAILURE;
    }

    if (get_random_char(&mob, NULL, NULL, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_random_char unexpectedly resolved for mobile with no room context");
        return TEST_FAILURE;
    }

    if (count_people_room(NULL, &obj, NULL, NULL, 0) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "count_people_room unexpectedly non-zero for object with no room context");
        return TEST_FAILURE;
    }

    if (count_people_room(NULL, NULL, NULL, &token, 0) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "count_people_room unexpectedly non-zero for token with no room context");
        return TEST_FAILURE;
    }

    if (count_people_room(&mob, NULL, NULL, NULL, 0) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "count_people_room unexpectedly non-zero for mobile with no room context");
        return TEST_FAILURE;
    }

    if (get_mob_vnum_room(NULL, &obj, NULL, NULL, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_mob_vnum_room unexpectedly resolved for object with no room context");
        return TEST_FAILURE;
    }

    if (get_mob_vnum_room(NULL, NULL, NULL, &token, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_mob_vnum_room unexpectedly resolved for token with no room context");
        return TEST_FAILURE;
    }

    if (get_mob_vnum_room(&mob, NULL, NULL, NULL, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_mob_vnum_room unexpectedly resolved for mobile with no room context");
        return TEST_FAILURE;
    }

    if (get_obj_vnum_room(NULL, &obj, NULL, NULL, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_obj_vnum_room unexpectedly resolved for object with no room context");
        return TEST_FAILURE;
    }

    if (get_obj_vnum_room(NULL, NULL, NULL, &token, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_obj_vnum_room unexpectedly resolved for token with no room context");
        return TEST_FAILURE;
    }

    if (get_obj_vnum_room(&mob, NULL, NULL, NULL, 1, NULL) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_obj_vnum_room unexpectedly resolved for mobile with no room context");
        return TEST_FAILURE;
    }

    if (get_order(NULL, &obj) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_order unexpectedly non-zero for object with no room context");
        return TEST_FAILURE;
    }

    if (get_order(&mob, NULL) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "get_order unexpectedly non-zero for mobile with no room context");
        return TEST_FAILURE;
    }

    info.mob = &mob;
    if (script_get_char_room(&info, "nobody", true) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_char_room unexpectedly resolved for mobile with no room context");
        return TEST_FAILURE;
    }

    if (script_get_obj_here(&info, "nothing") != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_obj_here unexpectedly resolved for mobile with no room context");
        return TEST_FAILURE;
    }

    info.mob = NULL;
    info.obj = &obj;
    if (script_get_char_room(&info, "nobody", true) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_char_room unexpectedly resolved for object with no room context");
        return TEST_FAILURE;
    }

    if (script_get_obj_here(&info, "nothing") != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_obj_here unexpectedly resolved for object with no room context");
        return TEST_FAILURE;
    }

    info.obj = NULL;
    info.token = &token;
    if (script_get_char_room(&info, "nobody", true) != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_char_room unexpectedly resolved for token with no room context");
        return TEST_FAILURE;
    }

    if (script_get_obj_here(&info, "nothing") != NULL) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                    "script_get_obj_here unexpectedly resolved for token with no room context");
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_string_trigger_guard_and_wildcard(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    OBJ_DATA obj;
    OBJ_INDEX_DATA obj_index;
    PROG_LIST *mob_prg;
    SCRIPT_DATA script;
    int ret;
    const int trigger_type = TRIG_ACT;
    const int trigger_slot = trigger_table[TRIG_ACT].slot;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&obj, 0, sizeof(obj));
    memset(&obj_index, 0, sizeof(obj_index));
    memset(&script, 0, sizeof(script));

    script.vnum = 900011;
    script.code = alloc_mem(sizeof(SCRIPT_CODE));
    if (!script.code)
        return TEST_ERROR;
    memset(script.code, 0, sizeof(SCRIPT_CODE));
    script.code[0].opcode = OP_END;
    script.code[0].level = 0;
    script.code[0].rest = str_dup("");
    script.code[0].length = 0;
    script.lines = 1;

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;
    mob.progs = new_prog_data();

    obj.valid = true;
    obj.pIndexData = &obj_index;
    obj.in_room = &room;

    room.source = NULL;
    room.progs = NULL;
    room.ltokens = NULL;

    mob_index.progs = new_prog_bank();

    mob_prg = new_trigger();
    mob_prg->trig_type = trigger_type;
    mob_prg->numeric = false;
    mob_prg->trig_phrase = str_dup("*");
    mob_prg->vnum = script.vnum;
    mob_prg->script = &script;
    list_appendlink(mob_index.progs[trigger_slot], mob_prg);

    ret = p_exact_trigger("no_match_phrase", &mob, NULL, NULL,
                          NULL, NULL, NULL, NULL, NULL, trigger_type);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_exact_trigger wildcard fallback returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    ret = p_act_trigger("anything", &mob, &obj, NULL,
                        NULL, NULL, NULL, NULL, NULL, trigger_type);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_act_trigger mixed owner guard returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    free_prog_list(mob_index.progs);
    free_prog_data(mob.progs);
    free_script_code(script.code, script.lines);
    return TEST_SUCCESS;
}

static test_result_t test_script_number_trigger_guard_and_wildcard(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    OBJ_DATA obj;
    OBJ_INDEX_DATA obj_index;
    PROG_LIST *mob_prg;
    SCRIPT_DATA script;
    int ret;
    const int trigger_type = TRIG_GIVE;
    const int trigger_slot = trigger_table[TRIG_GIVE].slot;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&obj, 0, sizeof(obj));
    memset(&obj_index, 0, sizeof(obj_index));
    memset(&script, 0, sizeof(script));

    script.vnum = 900012;
    script.code = alloc_mem(sizeof(SCRIPT_CODE));
    if (!script.code)
        return TEST_ERROR;
    memset(script.code, 0, sizeof(SCRIPT_CODE));
    script.code[0].opcode = OP_END;
    script.code[0].level = 0;
    script.code[0].rest = str_dup("");
    script.code[0].length = 0;
    script.lines = 1;

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;
    mob.progs = new_prog_data();

    obj.valid = true;
    obj.pIndexData = &obj_index;
    obj.in_room = &room;

    room.source = NULL;
    room.progs = NULL;
    room.ltokens = NULL;

    mob_index.progs = new_prog_bank();

    mob_prg = new_trigger();
    mob_prg->trig_type = trigger_type;
    mob_prg->numeric = true;
    mob_prg->trig_number = 0;
    mob_prg->vnum = script.vnum;
    mob_prg->script = &script;
    list_appendlink(mob_index.progs[trigger_slot], mob_prg);

    ret = p_number_trigger(42, 0, &mob, NULL, NULL, NULL,
                           NULL, NULL, NULL, NULL, NULL,
                           trigger_type, NULL);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_number_trigger wildcard fallback returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    ret = p_number_trigger(42, 0, &mob, &obj, NULL, NULL,
                           NULL, NULL, NULL, NULL, NULL,
                           trigger_type, NULL);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_number_trigger mixed owner guard returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    free_prog_list(mob_index.progs);
    free_prog_data(mob.progs);
    free_script_code(script.code, script.lines);
    return TEST_SUCCESS;
}

static test_result_t test_script_number_sight_slot_guard(test_case_t *test)
{
    int ret;
    (void)test;

    ret = test_number_sight_trigger(1, 0, test_match_equal, TRIG_GIVE, TRIG_GRALL,
                                    NULL, NULL, NULL,
                                    NULL, NULL, NULL,
                                    NULL, NULL,
                                    NULL);

    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "test_number_sight_trigger slot guard returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        return TEST_FAILURE;
    }

    return TEST_SUCCESS;
}

static test_result_t test_script_direction_trigger_exec(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA ch;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    PROG_LIST *mob_prg;
    SCRIPT_DATA script;
    int ret;
    const int direction = 3;
    const int trigger = TRIG_EXIT;
    const int trigger_slot = trigger_table[TRIG_EXIT].slot;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&ch, 0, sizeof(ch));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&script, 0, sizeof(script));

    script.vnum = 900013;
    script.code = alloc_mem(sizeof(SCRIPT_CODE));
    if (!script.code)
        return TEST_ERROR;
    memset(script.code, 0, sizeof(SCRIPT_CODE));
    script.code[0].opcode = OP_END;
    script.code[0].level = 0;
    script.code[0].rest = str_dup("");
    script.code[0].length = 0;
    script.lines = 1;

    room.lpeople = list_create(false);
    if (!room.lpeople) {
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    ch.in_room = &room;

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;
    mob.progs = new_prog_data();

    mob_index.progs = new_prog_bank();
    if (!mob.progs || !mob_index.progs) {
        if (mob_index.progs)
            free_prog_list(mob_index.progs);
        if (mob.progs)
            free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    if (!list_appendlink(room.lpeople, &mob)) {
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    mob_prg = new_trigger();
    if (!mob_prg) {
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    mob_prg->trig_type = trigger;
    mob_prg->numeric = true;
    mob_prg->trig_number = direction;
    mob_prg->vnum = script.vnum;
    mob_prg->script = &script;
    list_appendlink(mob_index.progs[trigger_slot], mob_prg);

    ret = p_direction_trigger(&ch, &room, direction, PRG_MPROG, trigger);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_direction_trigger execution returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    free_prog_list(mob_index.progs);
    free_prog_data(mob.progs);
    list_destroy(room.lpeople);
    free_script_code(script.code, script.lines);
    return TEST_SUCCESS;
}

static test_result_t test_script_greet_trigger_exec(test_case_t *test)
{
    ROOM_INDEX_DATA room;
    CHAR_DATA ch;
    CHAR_DATA mob;
    MOB_INDEX_DATA mob_index;
    PROG_LIST *mob_prg;
    SCRIPT_DATA script;
    int ret;
    const int trigger_slot = trigger_table[TRIG_GRALL].slot;
    (void)test;

    memset(&room, 0, sizeof(room));
    memset(&ch, 0, sizeof(ch));
    memset(&mob, 0, sizeof(mob));
    memset(&mob_index, 0, sizeof(mob_index));
    memset(&script, 0, sizeof(script));

    script.vnum = 900014;
    script.code = alloc_mem(sizeof(SCRIPT_CODE));
    if (!script.code)
        return TEST_ERROR;
    memset(script.code, 0, sizeof(SCRIPT_CODE));
    script.code[0].opcode = OP_END;
    script.code[0].level = 0;
    script.code[0].rest = str_dup("");
    script.code[0].length = 0;
    script.lines = 1;

    room.lpeople = list_create(false);
    if (!room.lpeople) {
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    ch.in_room = &room;

    mob.valid = true;
    mob.act[0] = ACT_IS_NPC;
    mob.pIndexData = &mob_index;
    mob.in_room = &room;
    mob.progs = new_prog_data();

    mob_index.progs = new_prog_bank();
    if (!mob.progs || !mob_index.progs) {
        if (mob_index.progs)
            free_prog_list(mob_index.progs);
        if (mob.progs)
            free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    if (!list_appendlink(room.lpeople, &mob)) {
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    mob_prg = new_trigger();
    if (!mob_prg) {
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_ERROR;
    }

    mob_prg->trig_type = TRIG_GRALL;
    mob_prg->numeric = true;
    mob_prg->trig_number = 101;
    mob_prg->vnum = script.vnum;
    mob_prg->script = &script;
    list_appendlink(mob_index.progs[trigger_slot], mob_prg);

    ret = p_greet_trigger(&ch, PRG_MPROG);
    if (ret != PRET_NOSCRIPT) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                      "p_greet_trigger execution returned %d, expected %d",
                      ret, PRET_NOSCRIPT);
        free_prog_list(mob_index.progs);
        free_prog_data(mob.progs);
        list_destroy(room.lpeople);
        free_script_code(script.code, script.lines);
        return TEST_FAILURE;
    }

    free_prog_list(mob_index.progs);
    free_prog_data(mob.progs);
    list_destroy(room.lpeople);
    free_script_code(script.code, script.lines);
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
