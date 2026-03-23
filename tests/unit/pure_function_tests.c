#ifdef BUILD_TESTS

#include <stdio.h>
#include <string.h>
#include "../framework/test_framework.h"
#include "../../merc.h"
#include "../../log.h"
#include "../../tables.h"
#include "../../wilds.h"

bool string_argremove_index(char *src, int argindex, char *buf);
bool string_argremove_phrase(char *src, char *phrase, char *buf);
char *string_linedel(char *string, int line);
char *string_lineadd(char *string, char *newstr, int line);
char *olc_getline(char *str, char *buf);
char *numlineas(char *string);
bool is_valid_colour_code(const char *code);

// Forward declaration for SHA256 function
extern char *sha256_crypt(const char *pwd);

static const struct flag_type *resolve_flag_table_for_test(const char *table_name) {
    if (!table_name) {
        return NULL;
    }

    if (str_cmp(table_name, "type_flags") == 0) {
        return type_flags;
    }
    if (str_cmp(table_name, "room_flags") == 0) {
        return room_flags;
    }
    if (str_cmp(table_name, "exit_flags") == 0) {
        return exit_flags;
    }

    return NULL;
}

static bool ends_with_crlf(const char *str) {
    size_t len;

    if (!str) {
        return false;
    }

    len = strlen(str);
    return len >= 2 && str[len - 2] == '\n' && str[len - 1] == '\r';
}

test_result_t run_pure_function_test_case(test_case_t *test) {
    if (!test || !test->config) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing configuration");
        return TEST_ERROR;
    }

    json_t *input = json_object_get(test->config, "input");
    json_t *expected_output = json_object_get(test->config, "expected_output");

    if (!input || !expected_output) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test missing input or expected_output");
        return TEST_ERROR;
    }

    json_t *function_name = json_object_get(input, "function");
    json_t *test_cases = json_object_get(input, "test_cases");

    if (!function_name || !test_cases || !json_is_array(test_cases)) {
        log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Pure function test malformed: missing function or test_cases array");
        return TEST_ERROR;
    }

    const char *func_name = json_string_value(function_name);
    if (test->verbose_output) {
        log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Testing pure function: %s", func_name);
    }

    if (strcmp(func_name, "parse_widevnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_area = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            const char *context_area_name = test_json_get_string(test_case, "context_area");
            const char *input_area_uid_from = test_json_get_string(test_case, "input_area_uid_from");
            long input_vnum = test_json_get_int(test_case, "input_vnum");
            char dynamic_input[MAX_INPUT_LENGTH];

            dynamic_input[0] = '\0';

            if ((input_str == NULL || input_str[0] == '\0')
                && input_area_uid_from != NULL
                && input_vnum > 0)
            {
                AREA_DATA *uid_area = find_area((char *)input_area_uid_from);
                if (!uid_area) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "parse_widevnum dynamic input failed: area '%s' not found",
                                 input_area_uid_from);
                    return TEST_ERROR;
                }

                snprintf(dynamic_input, sizeof(dynamic_input), "%ld#%ld", uid_area->uid, input_vnum);
                input_str = dynamic_input;
            }

            if (!input_str) continue;

            AREA_DATA *context_area = context_area_name ? find_area((char*)context_area_name) : NULL;
            WNUM result;

            bool parse_success = parse_widevnum((char*)input_str, context_area, &result);

            if (!parse_success && expected_vnum > 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') failed when success expected",
                             input_str);
                return TEST_FAILURE;
            }

            if (parse_success && expected_vnum == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') succeeded when failure expected",
                             input_str);
                return TEST_FAILURE;
            }

            if (parse_success && result.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum('%s') returned vnum %ld, expected %ld",
                             input_str, result.vnum, expected_vnum);
                return TEST_FAILURE;
            }

            if (parse_success && expected_area && result.pArea) {
                if (strcmp(result.pArea->name, expected_area) != 0) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "parse_widevnum('%s') returned area '%s', expected '%s'",
                                 input_str, result.pArea->name, expected_area);
                    return TEST_FAILURE;
                }
            }

            if (test->verbose_output) {
                if (parse_success) {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                 "✓ parse_widevnum('%s') -> area:'%s', vnum:%ld",
                                 input_str,
                                 result.pArea ? result.pArea->name : "NULL",
                                 result.vnum);
                } else {
                    log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                                 "✓ parse_widevnum('%s') -> failed as expected",
                                 input_str);
                }
            }
        }

        if (test->verbose_output) {
            log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS, "Pure function test passed for %s", func_name);
        }
        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "parse_widevnum_load") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            bool expected_success = test_json_get_bool(test_case, "expected_success");
            long expected_auid = test_json_get_int(test_case, "expected_auid");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            WNUM_LOAD actual = { 0, 0 };
            bool success;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "parse_widevnum_load test case missing input");
                return TEST_ERROR;
            }

            success = parse_widevnum_load(input_str, &actual);

            if (success != expected_success) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum_load('%s') success=%s, expected %s",
                             input_str,
                             success ? "true" : "false",
                             expected_success ? "true" : "false");
                return TEST_FAILURE;
            }

            if (actual.auid != expected_auid || actual.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "parse_widevnum_load('%s') returned (%ld,%ld), expected (%ld,%ld)",
                             input_str,
                             actual.auid,
                             actual.vnum,
                             expected_auid,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_widevnum_format") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            bool expected = test_json_get_bool(test_case, "expected");
            bool actual;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_widevnum_format test case missing input");
                return TEST_ERROR;
            }

            actual = is_widevnum_format(input_str);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_widevnum_format('%s') returned %s, expected %s",
                             input_str,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wnum_match") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *wnum_area_name = test_json_get_string(test_case, "wnum_area");
            long wnum_vnum = test_json_get_int(test_case, "wnum_vnum");
            const char *compare_area_name = test_json_get_string(test_case, "compare_area");
            long compare_vnum = test_json_get_int(test_case, "compare_vnum");
            bool expected = test_json_get_bool(test_case, "expected");
            AREA_DATA *wnum_area = NULL;
            AREA_DATA *compare_area = NULL;
            WNUM wnum = wnum_zero;
            bool actual;

            if (wnum_area_name)
                wnum_area = find_area((char *)wnum_area_name);
            if (compare_area_name)
                compare_area = find_area((char *)compare_area_name);

            if (wnum_area_name && !wnum_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match test case references unknown wnum_area '%s'",
                             wnum_area_name);
                return TEST_ERROR;
            }

            if (compare_area_name && !compare_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match test case references unknown compare_area '%s'",
                             compare_area_name);
                return TEST_ERROR;
            }

            wnum.pArea = wnum_area;
            wnum.vnum = wnum_vnum;
            actual = wnum_match(wnum, compare_area, compare_vnum);

            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wnum_match_room") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *wnum_area_name = test_json_get_string(test_case, "wnum_area");
            long wnum_vnum = test_json_get_int(test_case, "wnum_vnum");
            const char *room_area_name = test_json_get_string(test_case, "room_area");
            long room_vnum = test_json_get_int(test_case, "room_vnum");
            bool room_is_clone = test_json_get_bool(test_case, "room_is_clone");
            bool use_null_room = test_json_get_bool(test_case, "use_null_room");
            bool expected = test_json_get_bool(test_case, "expected");
            AREA_DATA *wnum_area = NULL;
            AREA_DATA *room_area = NULL;
            ROOM_INDEX_DATA *room = NULL;
            ROOM_INDEX_DATA clone_room;
            WNUM wnum = wnum_zero;
            bool actual;

            if (wnum_area_name)
                wnum_area = find_area((char *)wnum_area_name);
            if (room_area_name)
                room_area = find_area((char *)room_area_name);

            if (wnum_area_name && !wnum_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_room test case references unknown wnum_area '%s'",
                             wnum_area_name);
                return TEST_ERROR;
            }

            if (room_area_name && !room_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_room test case references unknown room_area '%s'",
                             room_area_name);
                return TEST_ERROR;
            }

            if (!use_null_room) {
                room = get_room_index(room_area, room_vnum);
                if (!room) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "wnum_match_room test case references unknown room %s#%ld",
                                 room_area_name ? room_area_name : "(null)",
                                 room_vnum);
                    return TEST_ERROR;
                }

                if (room_is_clone) {
                    memset(&clone_room, 0, sizeof(clone_room));
                    clone_room.area = room->area;
                    clone_room.vnum = room->vnum;
                    clone_room.source = room;
                    room = &clone_room;
                }
            }

            wnum.pArea = wnum_area;
            wnum.vnum = wnum_vnum;
            actual = wnum_match_room(wnum, use_null_room ? NULL : room);

            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_room case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wnum_match_obj") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *wnum_area_name = test_json_get_string(test_case, "wnum_area");
            long wnum_vnum = test_json_get_int(test_case, "wnum_vnum");
            const char *index_area_name = test_json_get_string(test_case, "index_area");
            long index_vnum = test_json_get_int(test_case, "index_vnum");
            bool use_null_obj = test_json_get_bool(test_case, "use_null_obj");
            bool obj_valid = test_json_get_bool(test_case, "obj_valid");
            bool obj_has_index = test_json_get_bool(test_case, "obj_has_index");
            bool expected = test_json_get_bool(test_case, "expected");
            AREA_DATA *wnum_area = NULL;
            AREA_DATA *index_area = NULL;
            OBJ_DATA obj;
            OBJ_INDEX_DATA obj_index;
            WNUM wnum = wnum_zero;
            bool actual;

            if (wnum_area_name)
                wnum_area = find_area((char *)wnum_area_name);
            if (index_area_name)
                index_area = find_area((char *)index_area_name);

            if (wnum_area_name && !wnum_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_obj case references unknown wnum_area '%s'",
                             wnum_area_name);
                return TEST_ERROR;
            }

            if (index_area_name && !index_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_obj case references unknown index_area '%s'",
                             index_area_name);
                return TEST_ERROR;
            }

            memset(&obj, 0, sizeof(obj));
            memset(&obj_index, 0, sizeof(obj_index));
            obj.valid = obj_valid;

            if (obj_has_index) {
                obj_index.area = index_area;
                obj_index.vnum = index_vnum;
                obj.pIndexData = &obj_index;
            }

            wnum.pArea = wnum_area;
            wnum.vnum = wnum_vnum;
            actual = wnum_match_obj(wnum, use_null_obj ? NULL : &obj);

            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_obj case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wnum_match_mob") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *wnum_area_name = test_json_get_string(test_case, "wnum_area");
            long wnum_vnum = test_json_get_int(test_case, "wnum_vnum");
            const char *index_area_name = test_json_get_string(test_case, "index_area");
            long index_vnum = test_json_get_int(test_case, "index_vnum");
            bool use_null_mob = test_json_get_bool(test_case, "use_null_mob");
            bool mob_valid = test_json_get_bool(test_case, "mob_valid");
            bool mob_is_npc = test_json_get_bool(test_case, "mob_is_npc");
            bool mob_has_index = test_json_get_bool(test_case, "mob_has_index");
            bool expected = test_json_get_bool(test_case, "expected");
            AREA_DATA *wnum_area = NULL;
            AREA_DATA *index_area = NULL;
            CHAR_DATA mob;
            MOB_INDEX_DATA mob_index;
            WNUM wnum = wnum_zero;
            bool actual;

            if (wnum_area_name)
                wnum_area = find_area((char *)wnum_area_name);
            if (index_area_name)
                index_area = find_area((char *)index_area_name);

            if (wnum_area_name && !wnum_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_mob case references unknown wnum_area '%s'",
                             wnum_area_name);
                return TEST_ERROR;
            }

            if (index_area_name && !index_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_mob case references unknown index_area '%s'",
                             index_area_name);
                return TEST_ERROR;
            }

            memset(&mob, 0, sizeof(mob));
            memset(&mob_index, 0, sizeof(mob_index));
            mob.valid = mob_valid;
            if (mob_is_npc)
                SET_BIT(mob.act[0], ACT_IS_NPC);

            if (mob_has_index) {
                mob_index.area = index_area;
                mob_index.vnum = index_vnum;
                mob.pIndexData = &mob_index;
            }

            wnum.pArea = wnum_area;
            wnum.vnum = wnum_vnum;
            actual = wnum_match_mob(wnum, use_null_mob ? NULL : &mob);

            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_mob case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wnum_match_token") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *wnum_area_name = test_json_get_string(test_case, "wnum_area");
            long wnum_vnum = test_json_get_int(test_case, "wnum_vnum");
            const char *index_area_name = test_json_get_string(test_case, "index_area");
            long index_vnum = test_json_get_int(test_case, "index_vnum");
            bool use_null_token = test_json_get_bool(test_case, "use_null_token");
            bool token_valid = test_json_get_bool(test_case, "token_valid");
            bool token_has_index = test_json_get_bool(test_case, "token_has_index");
            bool expected = test_json_get_bool(test_case, "expected");
            AREA_DATA *wnum_area = NULL;
            AREA_DATA *index_area = NULL;
            TOKEN_DATA token;
            TOKEN_INDEX_DATA token_index;
            WNUM wnum = wnum_zero;
            bool actual;

            if (wnum_area_name)
                wnum_area = find_area((char *)wnum_area_name);
            if (index_area_name)
                index_area = find_area((char *)index_area_name);

            if (wnum_area_name && !wnum_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_token case references unknown wnum_area '%s'",
                             wnum_area_name);
                return TEST_ERROR;
            }

            if (index_area_name && !index_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_token case references unknown index_area '%s'",
                             index_area_name);
                return TEST_ERROR;
            }

            memset(&token, 0, sizeof(token));
            memset(&token_index, 0, sizeof(token_index));
            token.valid = token_valid;

            if (token_has_index) {
                token_index.area = index_area;
                token_index.vnum = index_vnum;
                token.pIndexData = &token_index;
            }

            wnum.pArea = wnum_area;
            wnum.vnum = wnum_vnum;
            actual = wnum_match_token(wnum, use_null_token ? NULL : &token);

            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wnum_match_token case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_room_wnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *room_area_name = test_json_get_string(test_case, "room_area");
            long room_vnum = test_json_get_int(test_case, "room_vnum");
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            bool use_null_room = test_json_get_bool(test_case, "use_null_room");
            bool use_null_out = test_json_get_bool(test_case, "use_null_out");
            AREA_DATA *room_area = NULL;
            AREA_DATA *expected_area = NULL;
            ROOM_INDEX_DATA room;
            WNUM out = wnum_zero;

            if (room_area_name)
                room_area = find_area((char *)room_area_name);
            if (expected_area_name)
                expected_area = find_area((char *)expected_area_name);

            if (room_area_name && !room_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_room_wnum case references unknown room_area '%s'",
                             room_area_name);
                return TEST_ERROR;
            }

            if (expected_area_name && !expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_room_wnum case references unknown expected_area '%s'",
                             expected_area_name);
                return TEST_ERROR;
            }

            memset(&room, 0, sizeof(room));
            room.area = room_area;
            room.vnum = room_vnum;

            out.pArea = (AREA_DATA *)0x1;
            out.vnum = -1;
            get_room_wnum(use_null_room ? NULL : &room, use_null_out ? NULL : &out);

            if (!use_null_out && (out.pArea != expected_area || out.vnum != expected_vnum)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_room_wnum case %zu returned (%p,%ld), expected (%p,%ld)",
                             index,
                             (void *)out.pArea,
                             out.vnum,
                             (void *)expected_area,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_mob_wnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *mob_area_name = test_json_get_string(test_case, "mob_area");
            long mob_vnum = test_json_get_int(test_case, "mob_vnum");
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            bool use_null_mob = test_json_get_bool(test_case, "use_null_mob");
            bool use_null_out = test_json_get_bool(test_case, "use_null_out");
            AREA_DATA *mob_area = NULL;
            AREA_DATA *expected_area = NULL;
            MOB_INDEX_DATA mob;
            WNUM out = wnum_zero;

            if (mob_area_name)
                mob_area = find_area((char *)mob_area_name);
            if (expected_area_name)
                expected_area = find_area((char *)expected_area_name);

            if (mob_area_name && !mob_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_mob_wnum case references unknown mob_area '%s'",
                             mob_area_name);
                return TEST_ERROR;
            }

            if (expected_area_name && !expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_mob_wnum case references unknown expected_area '%s'",
                             expected_area_name);
                return TEST_ERROR;
            }

            memset(&mob, 0, sizeof(mob));
            mob.area = mob_area;
            mob.vnum = mob_vnum;

            out.pArea = (AREA_DATA *)0x1;
            out.vnum = -1;
            get_mob_wnum(use_null_mob ? NULL : &mob, use_null_out ? NULL : &out);

            if (!use_null_out && (out.pArea != expected_area || out.vnum != expected_vnum)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_mob_wnum case %zu returned (%p,%ld), expected (%p,%ld)",
                             index,
                             (void *)out.pArea,
                             out.vnum,
                             (void *)expected_area,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_obj_wnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *obj_area_name = test_json_get_string(test_case, "obj_area");
            long obj_vnum = test_json_get_int(test_case, "obj_vnum");
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            bool use_null_obj = test_json_get_bool(test_case, "use_null_obj");
            bool use_null_out = test_json_get_bool(test_case, "use_null_out");
            AREA_DATA *obj_area = NULL;
            AREA_DATA *expected_area = NULL;
            OBJ_INDEX_DATA obj;
            WNUM out = wnum_zero;

            if (obj_area_name)
                obj_area = find_area((char *)obj_area_name);
            if (expected_area_name)
                expected_area = find_area((char *)expected_area_name);

            if (obj_area_name && !obj_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_obj_wnum case references unknown obj_area '%s'",
                             obj_area_name);
                return TEST_ERROR;
            }

            if (expected_area_name && !expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_obj_wnum case references unknown expected_area '%s'",
                             expected_area_name);
                return TEST_ERROR;
            }

            memset(&obj, 0, sizeof(obj));
            obj.area = obj_area;
            obj.vnum = obj_vnum;

            out.pArea = (AREA_DATA *)0x1;
            out.vnum = -1;
            get_obj_wnum(use_null_obj ? NULL : &obj, use_null_out ? NULL : &out);

            if (!use_null_out && (out.pArea != expected_area || out.vnum != expected_vnum)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_obj_wnum case %zu returned (%p,%ld), expected (%p,%ld)",
                             index,
                             (void *)out.pArea,
                             out.vnum,
                             (void *)expected_area,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_token_wnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *token_area_name = test_json_get_string(test_case, "token_area");
            long token_vnum = test_json_get_int(test_case, "token_vnum");
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            bool use_null_token = test_json_get_bool(test_case, "use_null_token");
            bool use_null_out = test_json_get_bool(test_case, "use_null_out");
            AREA_DATA *token_area = NULL;
            AREA_DATA *expected_area = NULL;
            TOKEN_INDEX_DATA token;
            WNUM out = wnum_zero;

            if (token_area_name)
                token_area = find_area((char *)token_area_name);
            if (expected_area_name)
                expected_area = find_area((char *)expected_area_name);

            if (token_area_name && !token_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_token_wnum case references unknown token_area '%s'",
                             token_area_name);
                return TEST_ERROR;
            }

            if (expected_area_name && !expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_token_wnum case references unknown expected_area '%s'",
                             expected_area_name);
                return TEST_ERROR;
            }

            memset(&token, 0, sizeof(token));
            token.area = token_area;
            token.vnum = token_vnum;

            out.pArea = (AREA_DATA *)0x1;
            out.vnum = -1;
            get_token_wnum(use_null_token ? NULL : &token, use_null_out ? NULL : &out);

            if (!use_null_out && (out.pArea != expected_area || out.vnum != expected_vnum)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_token_wnum case %zu returned (%p,%ld), expected (%p,%ld)",
                             index,
                             (void *)out.pArea,
                             out.vnum,
                             (void *)expected_area,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "widevnum_string") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *area_name = test_json_get_string(test_case, "area");
            long vnum = test_json_get_int(test_case, "vnum");
            const char *ref_area_name = test_json_get_string(test_case, "ref_area");
            const char *expected = test_json_get_string(test_case, "expected");
            char expected_dynamic[MSL];
            AREA_DATA *area = NULL;
            AREA_DATA *ref_area = NULL;
            const char *actual;

            if (!expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "widevnum_string test case missing expected");
                return TEST_ERROR;
            }

            if (area_name)
                area = find_area((char *)area_name);
            if (ref_area_name)
                ref_area = find_area((char *)ref_area_name);

            if (area_name && !area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string case references unknown area '%s'",
                             area_name);
                return TEST_ERROR;
            }

            if (ref_area_name && !ref_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string case references unknown ref_area '%s'",
                             ref_area_name);
                return TEST_ERROR;
            }

            if (strstr(expected, "%AREA_UID%") != NULL) {
                if (!area) {
                    log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                               "widevnum_string expected uses %AREA_UID% but area is NULL");
                    return TEST_ERROR;
                }
                snprintf(expected_dynamic, sizeof(expected_dynamic), "%ld#%ld", area->uid, vnum);
                expected = expected_dynamic;
            }

            actual = widevnum_string(area, vnum, ref_area);
            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string case %zu returned '%s', expected '%s'",
                             index,
                             actual ? actual : "(null)",
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "widevnum_string_wnum") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *area_name = test_json_get_string(test_case, "area");
            long vnum = test_json_get_int(test_case, "vnum");
            const char *ref_area_name = test_json_get_string(test_case, "ref_area");
            const char *expected = test_json_get_string(test_case, "expected");
            char expected_dynamic[MSL];
            AREA_DATA *area = NULL;
            AREA_DATA *ref_area = NULL;
            WNUM wnum = wnum_zero;
            const char *actual;

            if (!expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "widevnum_string_wnum test case missing expected");
                return TEST_ERROR;
            }

            if (area_name)
                area = find_area((char *)area_name);
            if (ref_area_name)
                ref_area = find_area((char *)ref_area_name);

            if (area_name && !area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string_wnum case references unknown area '%s'",
                             area_name);
                return TEST_ERROR;
            }

            if (ref_area_name && !ref_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string_wnum case references unknown ref_area '%s'",
                             ref_area_name);
                return TEST_ERROR;
            }

            if (strstr(expected, "%AREA_UID%") != NULL) {
                if (!area) {
                    log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                               "widevnum_string_wnum expected uses %AREA_UID% but area is NULL");
                    return TEST_ERROR;
                }
                snprintf(expected_dynamic, sizeof(expected_dynamic), "%ld#%ld", area->uid, vnum);
                expected = expected_dynamic;
            }

            wnum.pArea = area;
            wnum.vnum = vnum;
            actual = widevnum_string_wnum(wnum, ref_area);
            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "widevnum_string_wnum case %zu returned '%s', expected '%s'",
                             index,
                             actual ? actual : "(null)",
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "widevnum_string_mobile") == 0
        || strcmp(func_name, "widevnum_string_object") == 0
        || strcmp(func_name, "widevnum_string_room") == 0
        || strcmp(func_name, "widevnum_string_token") == 0
        || strcmp(func_name, "widevnum_string_script") == 0
        || strcmp(func_name, "widevnum_string_blueprint") == 0
        || strcmp(func_name, "widevnum_string_blueprint_section") == 0
        || strcmp(func_name, "widevnum_string_dungeon") == 0
        || strcmp(func_name, "widevnum_string_ship") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *area_name = test_json_get_string(test_case, "area");
            const char *ref_area_name = test_json_get_string(test_case, "ref_area");
            const char *expected = test_json_get_string(test_case, "expected");
            long vnum = test_json_get_int(test_case, "vnum");
            bool use_null_entity = test_json_get_bool(test_case, "use_null_entity");
            char expected_dynamic[MSL];
            AREA_DATA *area = NULL;
            AREA_DATA *ref_area = NULL;
            const char *actual = NULL;

            if (!expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "widevnum_string_* wrapper test case missing expected");
                return TEST_ERROR;
            }

            if (area_name)
                area = find_area((char *)area_name);
            if (ref_area_name)
                ref_area = find_area((char *)ref_area_name);

            if (area_name && !area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "%s case references unknown area '%s'",
                             func_name,
                             area_name);
                return TEST_ERROR;
            }

            if (ref_area_name && !ref_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "%s case references unknown ref_area '%s'",
                             func_name,
                             ref_area_name);
                return TEST_ERROR;
            }

            if (strstr(expected, "%AREA_UID%") != NULL) {
                if (!area) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "%s expected uses %%AREA_UID%% but area is NULL",
                                 func_name);
                    return TEST_ERROR;
                }
                snprintf(expected_dynamic, sizeof(expected_dynamic), "%ld#%ld", area->uid, vnum);
                expected = expected_dynamic;
            }

            if (strcmp(func_name, "widevnum_string_mobile") == 0) {
                MOB_INDEX_DATA mob;
                memset(&mob, 0, sizeof(mob));
                mob.area = area;
                mob.vnum = vnum;
                actual = widevnum_string_mobile(use_null_entity ? NULL : &mob, ref_area);
            } else if (strcmp(func_name, "widevnum_string_object") == 0) {
                OBJ_INDEX_DATA obj;
                memset(&obj, 0, sizeof(obj));
                obj.area = area;
                obj.vnum = vnum;
                actual = widevnum_string_object(use_null_entity ? NULL : &obj, ref_area);
            } else if (strcmp(func_name, "widevnum_string_room") == 0) {
                ROOM_INDEX_DATA room;
                memset(&room, 0, sizeof(room));
                room.area = area;
                room.vnum = vnum;
                actual = widevnum_string_room(use_null_entity ? NULL : &room, ref_area);
            } else if (strcmp(func_name, "widevnum_string_token") == 0) {
                TOKEN_INDEX_DATA token;
                memset(&token, 0, sizeof(token));
                token.area = area;
                token.vnum = vnum;
                actual = widevnum_string_token(use_null_entity ? NULL : &token, ref_area);
            } else if (strcmp(func_name, "widevnum_string_script") == 0) {
                SCRIPT_DATA script;
                memset(&script, 0, sizeof(script));
                script.area = area;
                script.vnum = (int)vnum;
                actual = widevnum_string_script(use_null_entity ? NULL : &script, ref_area);
            } else if (strcmp(func_name, "widevnum_string_blueprint") == 0) {
                BLUEPRINT bp;
                memset(&bp, 0, sizeof(bp));
                bp.area = area;
                bp.vnum = vnum;
                actual = widevnum_string_blueprint(use_null_entity ? NULL : &bp, ref_area);
            } else if (strcmp(func_name, "widevnum_string_blueprint_section") == 0) {
                BLUEPRINT_SECTION bs;
                memset(&bs, 0, sizeof(bs));
                bs.area = area;
                bs.vnum = vnum;
                actual = widevnum_string_blueprint_section(use_null_entity ? NULL : &bs, ref_area);
            } else if (strcmp(func_name, "widevnum_string_dungeon") == 0) {
                DUNGEON_INDEX_DATA dng;
                memset(&dng, 0, sizeof(dng));
                dng.area = area;
                dng.vnum = vnum;
                actual = widevnum_string_dungeon(use_null_entity ? NULL : &dng, ref_area);
            } else if (strcmp(func_name, "widevnum_string_ship") == 0) {
                SHIP_INDEX_DATA ship;
                memset(&ship, 0, sizeof(ship));
                ship.area = area;
                ship.vnum = vnum;
                actual = widevnum_string_ship(use_null_entity ? NULL : &ship, ref_area);
            }

            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "%s case %zu returned '%s', expected '%s'",
                             func_name,
                             index,
                             actual ? actual : "(null)",
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "resolve_wnum_load") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *load_area_name = test_json_get_string(test_case, "load_area");
            long load_auid = test_json_get_int(test_case, "load_auid");
            long load_vnum = test_json_get_int(test_case, "load_vnum");
            const char *ref_area_name = test_json_get_string(test_case, "ref_area");
            const char *expected_area_name = test_json_get_string(test_case, "expected_area");
            long expected_vnum = test_json_get_int(test_case, "expected_vnum");
            AREA_DATA *load_area = NULL;
            AREA_DATA *ref_area = NULL;
            AREA_DATA *expected_area = NULL;
            WNUM_LOAD load = { 0, 0 };
            WNUM out = wnum_zero;

            if (load_area_name)
                load_area = find_area((char *)load_area_name);
            if (ref_area_name)
                ref_area = find_area((char *)ref_area_name);
            if (expected_area_name)
                expected_area = find_area((char *)expected_area_name);

            if (load_area_name && !load_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "resolve_wnum_load case references unknown load_area '%s'",
                             load_area_name);
                return TEST_ERROR;
            }

            if (ref_area_name && !ref_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "resolve_wnum_load case references unknown ref_area '%s'",
                             ref_area_name);
                return TEST_ERROR;
            }

            if (expected_area_name && !expected_area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "resolve_wnum_load case references unknown expected_area '%s'",
                             expected_area_name);
                return TEST_ERROR;
            }

            if (load_area)
                load.auid = load_area->uid;
            else
                load.auid = load_auid;

            load.vnum = load_vnum;
            resolve_wnum_load(&load, &out, ref_area);

            if (out.pArea != expected_area || out.vnum != expected_vnum) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "resolve_wnum_load case %zu returned (area=%p,vnum=%ld), expected (area=%p,vnum=%ld)",
                             index,
                             (void *)out.pArea,
                             out.vnum,
                             (void *)expected_area,
                             expected_vnum);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_colour_code_length_at_start") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            int expected_len = test_json_get_int(test_case, "expected_len");
            int actual_len;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "get_colour_code_length_at_start test case missing input");
                return TEST_ERROR;
            }

            actual_len = get_colour_code_length_at_start(input_str);
            if (actual_len != expected_len) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_colour_code_length_at_start('%s') returned %d, expected %d",
                             input_str,
                             actual_len,
                             expected_len);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "pronoun_helpers") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            int body_type = test_json_get_int(test_case, "body_type");
            const char *set_he_she = test_json_get_string(test_case, "set_he_she");
            const char *set_him_her = test_json_get_string(test_case, "set_him_her");
            const char *set_his_her = test_json_get_string(test_case, "set_his_her");
            const char *set_his_hers = test_json_get_string(test_case, "set_his_hers");
            const char *set_himself_herself = test_json_get_string(test_case, "set_himself_herself");
            const char *expected_he_she = test_json_get_string(test_case, "expected_he_she");
            const char *expected_him_her = test_json_get_string(test_case, "expected_him_her");
            const char *expected_his_her = test_json_get_string(test_case, "expected_his_her");
            const char *expected_his_hers = test_json_get_string(test_case, "expected_his_hers");
            const char *expected_himself_herself = test_json_get_string(test_case, "expected_himself_herself");
            const char *expected_body_type_name = test_json_get_string(test_case, "expected_body_type_name");
            CHAR_DATA ch;

            if (!expected_he_she || !expected_him_her || !expected_his_her ||
                !expected_his_hers || !expected_himself_herself || !expected_body_type_name) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "pronoun_helpers test case missing expected fields");
                return TEST_ERROR;
            }

            memset(&ch, 0, sizeof(ch));
            ch.body_type = body_type;

            ch.pronoun_he_she = (char *)(set_he_she ? set_he_she : "");
            ch.pronoun_him_her = (char *)(set_him_her ? set_him_her : "");
            ch.pronoun_his_her = (char *)(set_his_her ? set_his_her : "");
            ch.pronoun_his_hers = (char *)(set_his_hers ? set_his_hers : "");
            ch.pronoun_himself_herself = (char *)(set_himself_herself ? set_himself_herself : "");

            if (str_cmp(get_he_she(&ch), expected_he_she) != 0
                || str_cmp(get_him_her(&ch), expected_him_her) != 0
                || str_cmp(get_his_her(&ch), expected_his_her) != 0
                || str_cmp(get_his_hers(&ch), expected_his_hers) != 0
                || str_cmp(get_himself_herself(&ch), expected_himself_herself) != 0
                || str_cmp(get_body_type_name(&ch), expected_body_type_name) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "pronoun_helpers case %zu did not match expected pronoun/body-type values",
                             index);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_verb_form") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            bool use_null_character = test_json_get_bool(test_case, "use_null_character");
            int body_type = test_json_get_int(test_case, "body_type");
            int verb_preference = test_json_get_int(test_case, "verb_preference");
            const char *set_he_she = test_json_get_string(test_case, "set_he_she");
            const char *singular = test_json_get_string(test_case, "singular");
            const char *plural = test_json_get_string(test_case, "plural");
            const char *expected = test_json_get_string(test_case, "expected");
            CHAR_DATA ch;
            const char *actual;

            if (!singular || !plural || !expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "get_verb_form test case missing singular/plural/expected");
                return TEST_ERROR;
            }

            memset(&ch, 0, sizeof(ch));
            ch.body_type = body_type;
            ch.verb_preference = verb_preference;
            ch.pronoun_he_she = (char *)(set_he_she ? set_he_she : "");

            actual = get_verb_form(use_null_character ? NULL : &ch, singular, plural);
            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_verb_form case %zu returned '%s', expected '%s'",
                             index,
                             actual ? actual : "(null)",
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "reset_pronouns_to_body_type") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            int initial_body_type = test_json_get_int(test_case, "initial_body_type");
            int new_body_type = test_json_get_int(test_case, "new_body_type");
            const char *expected_he_she = test_json_get_string(test_case, "expected_he_she");
            const char *expected_him_her = test_json_get_string(test_case, "expected_him_her");
            const char *expected_his_her = test_json_get_string(test_case, "expected_his_her");
            const char *expected_his_hers = test_json_get_string(test_case, "expected_his_hers");
            const char *expected_himself_herself = test_json_get_string(test_case, "expected_himself_herself");
            int expected_verb_preference = test_json_get_int(test_case, "expected_verb_preference");
            CHAR_DATA ch;

            if (!expected_he_she || !expected_him_her || !expected_his_her
                || !expected_his_hers || !expected_himself_herself) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "reset_pronouns_to_body_type test case missing expected fields");
                return TEST_ERROR;
            }

            memset(&ch, 0, sizeof(ch));
            ch.body_type = initial_body_type;
            ch.pronoun_he_she = str_dup("custom_subj");
            ch.pronoun_him_her = str_dup("custom_obj");
            ch.pronoun_his_her = str_dup("custom_poss_adj");
            ch.pronoun_his_hers = str_dup("custom_poss_pron");
            ch.pronoun_himself_herself = str_dup("custom_refl");
            ch.verb_preference = VERB_FORM_DEFAULT;

            reset_pronouns_to_body_type(&ch, (body_type_t)new_body_type);

            if (str_cmp(ch.pronoun_he_she, expected_he_she) != 0
                || str_cmp(ch.pronoun_him_her, expected_him_her) != 0
                || str_cmp(ch.pronoun_his_her, expected_his_her) != 0
                || str_cmp(ch.pronoun_his_hers, expected_his_hers) != 0
                || str_cmp(ch.pronoun_himself_herself, expected_himself_herself) != 0
                || ch.verb_preference != expected_verb_preference) {
                free_string(ch.pronoun_he_she);
                free_string(ch.pronoun_him_her);
                free_string(ch.pronoun_his_her);
                free_string(ch.pronoun_his_hers);
                free_string(ch.pronoun_himself_herself);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "reset_pronouns_to_body_type case %zu did not match expected reset state",
                             index);
                return TEST_FAILURE;
            }

            free_string(ch.pronoun_he_she);
            free_string(ch.pronoun_him_her);
            free_string(ch.pronoun_his_her);
            free_string(ch.pronoun_his_hers);
            free_string(ch.pronoun_himself_herself);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_valid_colour_code") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            bool expected = test_json_get_bool(test_case, "expected");
            bool actual;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_valid_colour_code test case missing input");
                return TEST_ERROR;
            }

            actual = is_valid_colour_code(input_str);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_valid_colour_code('%s') returned %s, expected %s",
                             input_str,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "get_article") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            bool upper = test_json_get_bool(test_case, "upper");
            const char *expected = test_json_get_string(test_case, "expected");
            const char *actual;

            if (!input_str || !expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "get_article test case missing input/expected");
                return TEST_ERROR;
            }

            actual = get_article((char *)input_str, upper);
            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "get_article('%s', %s) returned '%s', expected '%s'",
                             input_str,
                             upper ? "true" : "false",
                             actual ? actual : "(null)",
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "colour_trunc_len") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            int limit = test_json_get_int(test_case, "limit");
            int expected = test_json_get_int(test_case, "expected");
            int actual;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "colour_trunc_len test case missing input");
                return TEST_ERROR;
            }

            actual = colour_trunc_len(input_str, limit);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "colour_trunc_len('%s', %d) returned %d, expected %d",
                             input_str,
                             limit,
                             actual,
                             expected);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "pad_string") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            int length = test_json_get_int(test_case, "length");
            const char *colour = test_json_get_string(test_case, "colour");
            const char *character = test_json_get_string(test_case, "character");
            const char *expected = test_json_get_string(test_case, "expected");
            char *actual;

            if (!input_str || !expected) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "pad_string test case missing input/expected");
                return TEST_ERROR;
            }

            actual = pad_string((char *)input_str, length, (char *)colour, (char *)character);
            if (!actual || str_cmp(actual, expected) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "pad_string('%s', %d, '%s', '%s') returned '%s', expected '%s'",
                             input_str,
                             length,
                             colour ? colour : "(null)",
                             character ? character : "(null)",
                             actual ? actual : "(null)",
                             expected);
                if (actual)
                    free_string(actual);
                return TEST_FAILURE;
            }

            free_string(actual);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_number") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *arg = test_json_get_string(test_case, "arg");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "is_number test case missing 'arg'");
                return TEST_ERROR;
            }

            bool actual = is_number(arg);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_number('%s') returned %s, expected %s",
                             arg,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_prefix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *astr = test_json_get_string(test_case, "astr");
            const char *bstr = test_json_get_string(test_case, "bstr");
            bool expected_not_prefix = test_json_get_bool(test_case, "expected_not_prefix");

            if (!astr || !bstr) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "str_prefix test case missing astr/bstr");
                return TEST_ERROR;
            }

            bool actual_not_prefix = str_prefix(astr, bstr);
            if (actual_not_prefix != expected_not_prefix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_prefix('%s','%s') returned %s, expected %s",
                             astr,
                             bstr,
                             actual_not_prefix ? "true" : "false",
                             expected_not_prefix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "number_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            int expected_number = test_json_get_int(test_case, "expected_number");
            const char *expected_arg = test_json_get_string(test_case, "expected_arg");
            char argument_buf[MAX_INPUT_LENGTH];
            char arg_out[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_arg) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "number_argument test case missing argument/expected_arg");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            int actual_number = number_argument(argument_buf, arg_out);

            if (actual_number != expected_number || str_cmp(arg_out, expected_arg) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "number_argument('%s') returned (%d,'%s'), expected (%d,'%s')",
                             argument_in,
                             actual_number,
                             arg_out,
                             expected_number,
                             expected_arg);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "one_argument") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            const char *expected_first = test_json_get_string(test_case, "expected_first");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            char argument_buf[MAX_STRING_LENGTH];
            char first[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_first || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "one_argument test case missing argument/expected_first/expected_rest");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            char *rest = one_argument(argument_buf, first);

            if (str_cmp(first, expected_first) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "one_argument('%s') returned first='%s', rest='%s'; expected first='%s', rest='%s'",
                             argument_in,
                             first,
                             rest,
                             expected_first,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "smash_tilde") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char buffer[MAX_STRING_LENGTH];

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "smash_tilde test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(buffer, sizeof(buffer), "%s", input_str);
            smash_tilde(buffer);

            if (str_cmp(buffer, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "smash_tilde('%s') returned '%s', expected '%s'",
                             input_str,
                             buffer,
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "is_exact_name") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "str");
            const char *namelist = test_json_get_string(test_case, "namelist");
            bool expected = test_json_get_bool(test_case, "expected");

            if (!needle || !namelist) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "is_exact_name test case missing str/namelist");
                return TEST_ERROR;
            }

            bool actual = is_exact_name((char *)needle, (char *)namelist);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "is_exact_name('%s','%s') returned %s, expected %s",
                             needle,
                             namelist,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_cmp") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *left = test_json_get_string(test_case, "left");
            const char *right = test_json_get_string(test_case, "right");
            bool expected_not_equal = test_json_get_bool(test_case, "expected_not_equal");

            if (!left || !right) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_cmp test case missing left/right");
                return TEST_ERROR;
            }

            bool actual_not_equal = str_cmp(left, right);
            if (actual_not_equal != expected_not_equal) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_cmp('%s','%s') returned %s, expected %s",
                             left,
                             right,
                             actual_not_equal ? "true" : "false",
                             expected_not_equal ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_infix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *needle = test_json_get_string(test_case, "needle");
            const char *haystack = test_json_get_string(test_case, "haystack");
            bool expected_not_infix = test_json_get_bool(test_case, "expected_not_infix");

            if (!needle || !haystack) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_infix test case missing needle/haystack");
                return TEST_ERROR;
            }

            bool actual_not_infix = str_infix(needle, haystack);
            if (actual_not_infix != expected_not_infix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_infix('%s','%s') returned %s, expected %s",
                             needle,
                             haystack,
                             actual_not_infix ? "true" : "false",
                             expected_not_infix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "str_suffix") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *astr = test_json_get_string(test_case, "astr");
            const char *bstr = test_json_get_string(test_case, "bstr");
            bool expected_not_suffix = test_json_get_bool(test_case, "expected_not_suffix");

            if (!astr || !bstr) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "str_suffix test case missing astr/bstr");
                return TEST_ERROR;
            }

            bool actual_not_suffix = str_suffix(astr, bstr);
            if (actual_not_suffix != expected_not_suffix) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "str_suffix('%s','%s') returned %s, expected %s",
                             astr,
                             bstr,
                             actual_not_suffix ? "true" : "false",
                             expected_not_suffix ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "first_arg") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *argument_in = test_json_get_string(test_case, "argument");
            const char *expected_first = test_json_get_string(test_case, "expected_first");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            bool fcase = test_json_get_bool(test_case, "fcase");
            char argument_buf[MAX_STRING_LENGTH];
            char first[MAX_INPUT_LENGTH];

            if (!argument_in || !expected_first || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "first_arg test case missing argument/expected_first/expected_rest");
                return TEST_ERROR;
            }

            snprintf(argument_buf, sizeof(argument_buf), "%s", argument_in);
            char *rest = first_arg(argument_buf, first, fcase);

            if (str_cmp(first, expected_first) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "first_arg('%s') returned first='%s', rest='%s'; expected first='%s', rest='%s'",
                             argument_in,
                             first,
                             rest,
                             expected_first,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_argremove_index") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *src = test_json_get_string(test_case, "src");
            int argindex = test_json_get_int(test_case, "argindex");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            bool expected_result = test_json_get_bool(test_case, "expected_result");
            char output[MAX_STRING_LENGTH];
            char src_buf[MAX_STRING_LENGTH];

            if (!src || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_argremove_index test case missing src/expected_output");
                return TEST_ERROR;
            }

            snprintf(src_buf, sizeof(src_buf), "%s", src);
            bool actual_result = string_argremove_index(src_buf, argindex, output);

            if (actual_result != expected_result || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_argremove_index('%s', %d) returned (result=%s, output='%s'), expected (result=%s, output='%s')",
                             src,
                             argindex,
                             actual_result ? "true" : "false",
                             output,
                             expected_result ? "true" : "false",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_argremove_phrase") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *src = test_json_get_string(test_case, "src");
            const char *phrase = test_json_get_string(test_case, "phrase");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            bool expected_result = test_json_get_bool(test_case, "expected_result");
            char output[MAX_STRING_LENGTH];
            char src_buf[MAX_STRING_LENGTH];
            char phrase_buf[MAX_INPUT_LENGTH];

            if (!src || !phrase || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_argremove_phrase test case missing src/phrase/expected_output");
                return TEST_ERROR;
            }

            snprintf(src_buf, sizeof(src_buf), "%s", src);
            snprintf(phrase_buf, sizeof(phrase_buf), "%s", phrase);
            bool actual_result = string_argremove_phrase(src_buf, phrase_buf, output);

            if (actual_result != expected_result || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_argremove_phrase('%s', '%s') returned (result=%s, output='%s'), expected (result=%s, output='%s')",
                             src,
                             phrase,
                             actual_result ? "true" : "false",
                             output,
                             expected_result ? "true" : "false",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "format_string") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_contains = test_json_get_string(test_case, "expected_contains");
            bool expect_trailing_crlf = test_json_get_bool(test_case, "expect_trailing_crlf");
            char *input_dup;
            char *output;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_string test case missing input");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = format_string(input_dup);
            if (!output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_string returned NULL output");
                return TEST_FAILURE;
            }

            if (expected_contains && !strstr(output, expected_contains)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_string output '%s' does not contain '%s'",
                             output,
                             expected_contains);
                free_string(output);
                return TEST_FAILURE;
            }

            if (expect_trailing_crlf && !ends_with_crlf(output)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_string output does not end with \\n\\r: '%s'",
                             output);
                free_string(output);
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "format_paragraph") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_contains = test_json_get_string(test_case, "expected_contains");
            bool expect_trailing_crlf = test_json_get_bool(test_case, "expect_trailing_crlf");
            char *input_dup;
            char *output;

            if (!input_str) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_paragraph test case missing input");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = format_paragraph(input_dup);
            if (!output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "format_paragraph returned NULL output");
                return TEST_FAILURE;
            }

            if (expected_contains && !strstr(output, expected_contains)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_paragraph output '%s' does not contain '%s'",
                             output,
                             expected_contains);
                free_string(output);
                return TEST_FAILURE;
            }

            if (expect_trailing_crlf && !ends_with_crlf(output)) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "format_paragraph output does not end with \\n\\r: '%s'",
                             output);
                free_string(output);
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_linedel") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int line = test_json_get_int(test_case, "line");
            char *input_dup;
            char *output;

            if (!input_str || !expected_output || line <= 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_linedel test case missing input/expected_output/line");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = string_linedel(input_dup, line);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_linedel(line=%d) returned '%s', expected '%s'",
                             line,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_lineadd") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *newstr = test_json_get_string(test_case, "newstr");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int line = test_json_get_int(test_case, "line");
            char *input_dup;
            char newstr_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !newstr || !expected_output || line <= 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_lineadd test case missing input/newstr/expected_output/line");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            snprintf(newstr_buf, sizeof(newstr_buf), "%s", newstr);
            output = string_lineadd(input_dup, newstr_buf, line);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_lineadd(line=%d) returned '%s', expected '%s'",
                             line,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "olc_getline") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_line = test_json_get_string(test_case, "expected_line");
            const char *expected_rest = test_json_get_string(test_case, "expected_rest");
            char input_buf[MAX_STRING_LENGTH];
            char line_buf[MAX_STRING_LENGTH];
            char *rest;

            if (!input_str || !expected_line || !expected_rest) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "olc_getline test case missing input/expected_line/expected_rest");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            rest = olc_getline(input_buf, line_buf);

            if (str_cmp(line_buf, expected_line) != 0 || str_cmp(rest, expected_rest) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "olc_getline('%s') returned line='%s' rest='%s', expected line='%s' rest='%s'",
                             input_str,
                             line_buf,
                             rest,
                             expected_line,
                             expected_rest);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "numlineas") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char input_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "numlineas test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            output = numlineas(input_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "numlineas('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_replace") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *orig = test_json_get_string(test_case, "orig");
            const char *old = test_json_get_string(test_case, "old");
            const char *new_value = test_json_get_string(test_case, "new");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char *orig_dup;
            char *output;

            if (!orig || !old || !new_value || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_replace test case missing orig/old/new/expected_output");
                return TEST_ERROR;
            }

            orig_dup = str_dup(orig);
            output = string_replace(orig_dup, (char *)old, (char *)new_value);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_replace('%s','%s','%s') returned '%s', expected '%s'",
                             orig,
                             old,
                             new_value,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_replace_static") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *orig = test_json_get_string(test_case, "orig");
            const char *old = test_json_get_string(test_case, "old");
            const char *new_value = test_json_get_string(test_case, "new");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char orig_buf[MAX_STRING_LENGTH];
            char old_buf[MAX_INPUT_LENGTH];
            char new_buf[MAX_INPUT_LENGTH];
            char *output;

            if (!orig || !old || !new_value || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_replace_static test case missing orig/old/new/expected_output");
                return TEST_ERROR;
            }

            snprintf(orig_buf, sizeof(orig_buf), "%s", orig);
            snprintf(old_buf, sizeof(old_buf), "%s", old);
            snprintf(new_buf, sizeof(new_buf), "%s", new_value);
            output = string_replace_static(orig_buf, old_buf, new_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_replace_static('%s','%s','%s') returned '%s', expected '%s'",
                             orig,
                             old,
                             new_value,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_unpad") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char *input_dup;
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_unpad test case missing input/expected_output");
                return TEST_ERROR;
            }

            input_dup = str_dup(input_str);
            output = string_unpad(input_dup);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_unpad('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                if (output) {
                    free_string(output);
                }
                return TEST_FAILURE;
            }

            free_string(output);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_proper") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            char input_buf[MAX_STRING_LENGTH];
            char *output;

            if (!input_str || !expected_output) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_proper test case missing input/expected_output");
                return TEST_ERROR;
            }

            snprintf(input_buf, sizeof(input_buf), "%s", input_str);
            output = string_proper(input_buf);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_proper('%s') returned '%s', expected '%s'",
                             input_str,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "string_indent") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_str = test_json_get_string(test_case, "input");
            const char *expected_output = test_json_get_string(test_case, "expected_output");
            int indent = test_json_get_int(test_case, "indent");
            char *output;

            if (!input_str || !expected_output || indent < 0) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "string_indent test case missing input/expected_output/indent");
                return TEST_ERROR;
            }

            output = string_indent(input_str, indent);

            if (!output || str_cmp(output, expected_output) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "string_indent('%s', %d) returned '%s', expected '%s'",
                             input_str,
                             indent,
                             output ? output : "(null)",
                             expected_output);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "tbit_subset_of") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            json_t *test_bits_json = json_object_get(test_case, "test_bits");
            json_t *mask_bits_json = json_object_get(test_case, "mask_bits");
            bool expected = test_json_get_bool(test_case, "expected");
            TYPE_BITSET test_bits;
            TYPE_BITSET mask_bits;

            if (!json_is_array(test_bits_json) || !json_is_array(mask_bits_json)) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "tbit_subset_of test case missing test_bits/mask_bits arrays");
                return TEST_ERROR;
            }

            TBIT_ZERO(test_bits);
            TBIT_ZERO(mask_bits);

            size_t bit_index;
            json_t *bit_json;

            json_array_foreach(test_bits_json, bit_index, bit_json) {
                if (!json_is_integer(bit_json)) {
                    log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                               "tbit_subset_of test_bits contains non-integer value");
                    return TEST_ERROR;
                }

                int bit = (int)json_integer_value(bit_json);
                if (bit < 0 || bit >= ITEM__MAX) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "tbit_subset_of test_bits contains out-of-range bit %d",
                                 bit);
                    return TEST_ERROR;
                }
                TBIT_SET(test_bits, bit);
            }

            json_array_foreach(mask_bits_json, bit_index, bit_json) {
                if (!json_is_integer(bit_json)) {
                    log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                               "tbit_subset_of mask_bits contains non-integer value");
                    return TEST_ERROR;
                }

                int bit = (int)json_integer_value(bit_json);
                if (bit < 0 || bit >= ITEM__MAX) {
                    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                                 "tbit_subset_of mask_bits contains out-of-range bit %d",
                                 bit);
                    return TEST_ERROR;
                }
                TBIT_SET(mask_bits, bit);
            }

            bool actual = tbit_subset_of(test_bits, mask_bits);
            if (actual != expected) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "tbit_subset_of case %zu returned %s, expected %s",
                             index,
                             actual ? "true" : "false",
                             expected ? "true" : "false");
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "flag_lookup") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *table_name = test_json_get_string(test_case, "table");
            const char *name = test_json_get_string(test_case, "name");
            bool should_find = test_json_get_bool(test_case, "should_find");
            const struct flag_type *flag_table;
            int value;

            if (!table_name || !name) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "flag_lookup test case missing table/name");
                return TEST_ERROR;
            }

            flag_table = resolve_flag_table_for_test(table_name);
            if (!flag_table) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "flag_lookup test references unknown table '%s'",
                             table_name);
                return TEST_ERROR;
            }

            value = flag_lookup(name, flag_table);
            if (should_find && value == 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "flag_lookup('%s', %s) returned 0, expected a match",
                             name, table_name);
                return TEST_FAILURE;
            }

            if (!should_find && value != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "flag_lookup('%s', %s) returned %d, expected no match",
                             name, table_name, value);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "stat_lookup") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *table_name = test_json_get_string(test_case, "table");
            const char *name = test_json_get_string(test_case, "name");
            int invalid = test_json_get_int(test_case, "invalid");
            bool should_find = test_json_get_bool(test_case, "should_find");
            const struct flag_type *flag_table;
            int value;

            if (!table_name || !name) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "stat_lookup test case missing table/name");
                return TEST_ERROR;
            }

            flag_table = resolve_flag_table_for_test(table_name);
            if (!flag_table) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_lookup test references unknown table '%s'",
                             table_name);
                return TEST_ERROR;
            }

            value = stat_lookup(name, flag_table, invalid);
            if (should_find && value == invalid) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_lookup('%s', %s) returned invalid sentinel %d, expected match",
                             name, table_name, invalid);
                return TEST_FAILURE;
            }

            if (!should_find && value != invalid) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_lookup('%s', %s) returned %d, expected invalid sentinel %d",
                             name, table_name, value, invalid);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "stat_find") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *table_name = test_json_get_string(test_case, "table");
            const char *name = test_json_get_string(test_case, "name");
            int invalid = test_json_get_int(test_case, "invalid");
            bool should_find = test_json_get_bool(test_case, "should_find");
            const struct flag_type *flag_table;
            int value;

            if (!table_name || !name) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "stat_find test case missing table/name");
                return TEST_ERROR;
            }

            flag_table = resolve_flag_table_for_test(table_name);
            if (!flag_table) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_find test references unknown table '%s'",
                             table_name);
                return TEST_ERROR;
            }

            value = stat_find(name, flag_table, invalid);
            if (should_find && value == invalid) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_find('%s', %s) returned invalid sentinel %d, expected match",
                             name, table_name, invalid);
                return TEST_FAILURE;
            }

            if (!should_find && value != invalid) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "stat_find('%s', %s) returned %d, expected invalid sentinel %d",
                             name, table_name, value, invalid);
                return TEST_FAILURE;
            }
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "wilds_resolver_consistency") == 0) {
        size_t index;
        json_t *test_case;

        json_array_foreach(test_cases, index, test_case) {
            int width = test_json_get_int(test_case, "width");
            int height = test_json_get_int(test_case, "height");
            int runtime_x = test_json_get_int(test_case, "runtime_x");
            int runtime_y = test_json_get_int(test_case, "runtime_y");
            const char *base_tile_str = test_json_get_string(test_case, "base_tile");
            const char *runtime_tile_str = test_json_get_string(test_case, "runtime_tile");
            const char *overlay_tile_str = test_json_get_string(test_case, "overlay_tile");
            char base_tile = (base_tile_str && base_tile_str[0]) ? base_tile_str[0] : '.';
            char runtime_tile = (runtime_tile_str && runtime_tile_str[0]) ? runtime_tile_str[0] : '^';
            char overlay_tile = (overlay_tile_str && overlay_tile_str[0]) ? overlay_tile_str[0] : '~';
            size_t map_len;
            WILDS_DATA wilds;
            WILDS_CHUNK chunk;
            WILDS_OVERLAY overlay;
            bool set_ok;

            if (width <= 0 || height <= 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu has invalid dimensions %dx%d",
                             index, width, height);
                return TEST_ERROR;
            }

            if (runtime_x < 0 || runtime_y < 0 || runtime_x >= width || runtime_y >= height) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu has invalid runtime coords (%d,%d) for %dx%d",
                             index, runtime_x, runtime_y, width, height);
                return TEST_ERROR;
            }

            memset(&wilds, 0, sizeof(wilds));
            memset(&chunk, 0, sizeof(chunk));
            memset(&overlay, 0, sizeof(overlay));

            map_len = (size_t)width * (size_t)height;
            wilds.staticmap = malloc(map_len);
            wilds.map = malloc(map_len);
            if (!wilds.staticmap || !wilds.map) {
                if (wilds.staticmap)
                    free(wilds.staticmap);
                if (wilds.map)
                    free(wilds.map);
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "wilds_resolver_consistency memory allocation failed");
                return TEST_ERROR;
            }

            memset(wilds.staticmap, base_tile, map_len);
            memset(wilds.map, base_tile, map_len);
            wilds.map_size_x = width;
            wilds.map_size_y = height;

            set_ok = set_wilds_runtime_tile(&wilds, runtime_x, runtime_y, runtime_tile);
            if (!set_ok) {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu failed runtime set at (%d,%d)",
                             index, runtime_x, runtime_y);
                return TEST_FAILURE;
            }

            if (get_wilds_base_tile(&wilds, runtime_x, runtime_y) != base_tile) {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu base tile changed unexpectedly",
                             index);
                return TEST_FAILURE;
            }

            if (get_wilds_effective_tile(&wilds, runtime_x, runtime_y) != runtime_tile) {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu effective tile did not reflect runtime map",
                             index);
                return TEST_FAILURE;
            }

            chunk.cx = runtime_x / WILDS_OVERLAY_CHUNK_SIZE;
            chunk.cy = runtime_y / WILDS_OVERLAY_CHUNK_SIZE;
            overlay.x1 = runtime_x;
            overlay.y1 = runtime_y;
            overlay.x2 = runtime_x;
            overlay.y2 = runtime_y;
            overlay.tile = overlay_tile;
            overlay.region = 0;
            overlay.expires_at = 0;
            chunk.overlays = &overlay;
            wilds.runtime_chunks = &chunk;

            if (get_wilds_effective_tile(&wilds, runtime_x, runtime_y) != overlay_tile) {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu overlay tile did not override effective tile",
                             index);
                return TEST_FAILURE;
            }

            if (set_wilds_runtime_tile(&wilds, -1, runtime_y, runtime_tile)) {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu accepted out-of-bounds runtime write",
                             index);
                return TEST_FAILURE;
            }

            if (get_wilds_base_tile(&wilds, width, runtime_y) != '\0') {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu expected base out-of-bounds to return NUL",
                             index);
                return TEST_FAILURE;
            }

            if (get_wilds_effective_tile(&wilds, runtime_x, height) != '\0') {
                free(wilds.staticmap);
                free(wilds.map);
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "wilds_resolver_consistency case %zu expected effective out-of-bounds to return NUL",
                             index);
                return TEST_FAILURE;
            }

            free(wilds.staticmap);
            free(wilds.map);
        }

        return TEST_SUCCESS;
    }

    if (strcmp(func_name, "sha256") == 0) {
        size_t index;
        json_t *test_case;
        json_array_foreach(test_cases, index, test_case) {
            const char *input_string = test_json_get_string(test_case, "input_string");
            const char *expected_hash = test_json_get_string(test_case, "expected_hash");

            if (!input_string || !expected_hash) {
                log_message(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                           "sha256 test case missing input_string or expected_hash");
                return TEST_ERROR;
            }

            char *actual_hash = sha256_crypt(input_string);
            if (!actual_hash) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "sha256_crypt('%s') returned NULL",
                             input_string);
                return TEST_ERROR;
            }

            if (strcmp(actual_hash, expected_hash) != 0) {
                log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                             "sha256_crypt('%s') returned '%s', expected '%s'",
                             input_string, actual_hash, expected_hash);
                return TEST_FAILURE;
            }

            if (test->verbose_output) {
                log_message_f(LOG_LEVEL_INFO, LOG_UNIT_TESTS,
                             "✓ sha256('%s') -> %s",
                             input_string, actual_hash);
            }
        }

        return TEST_SUCCESS;
    }

    log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS, "Unsupported pure function: %s", func_name);
    return TEST_SKIP;
}

#endif // BUILD_TESTS
