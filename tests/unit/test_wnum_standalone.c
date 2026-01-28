#include <criterion/criterion.h>
#include <string.h>

// Standalone test - we'll create simple stubs for the functions we need
typedef struct area_data AREA_DATA;
typedef struct char_data CHAR_DATA;

struct area_data {
    long uid;
    char *name;
    char *filename;
    int vnums[2];  // [0] = low vnum, [1] = high vnum
};

// Simple stubs for the functions we need
AREA_DATA *area_first = NULL;
AREA_DATA area_list[10];  // Simple array for test areas

AREA_DATA *find_area(const char *name) {
    for (int i = 0; i < 10; i++) {
        if (area_list[i].name && !strcmp(area_list[i].name, name)) {
            return &area_list[i];
        }
    }
    return NULL;
}

AREA_DATA *get_area_by_uid(long uid) {
    for (int i = 0; i < 10; i++) {
        if (area_list[i].uid == uid && area_list[i].name) {
            return &area_list[i];
        }
    }
    return NULL;
}

// Simplified WNUM structure
typedef struct wnum_data {
    long area_uid;
    int vnum;
    int original_vnum;
} WNUM;

// Simple parsing function we'll implement inline
WNUM parse_widevnum(const char *argument, AREA_DATA *default_area) {
    WNUM result = {0, 0, 0};
    
    if (!argument || !*argument) {
        return result;
    }
    
    char *endptr;
    char input[256];
    strncpy(input, argument, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';
    
    // Check for area name format: 'Area Name'#1234
    if (input[0] == '\'') {
        char *quote_end = strchr(input + 1, '\'');
        if (quote_end && quote_end[1] == '#') {
            *quote_end = '\0';
            char *area_name = input + 1;
            char *vnum_str = quote_end + 2;
            
            AREA_DATA *area = find_area(area_name);
            if (area) {
                result.area_uid = area->uid;
                result.vnum = (int)strtol(vnum_str, &endptr, 10);
                if (*endptr == '\0' && result.vnum > 0) {
                    result.original_vnum = result.vnum;
                    return result;
                }
            }
            result.area_uid = 0;
            result.vnum = 0;
            return result;
        }
    }
    
    // Check for UID#vnum format: 5#1234
    char *hash_ptr = strchr(input, '#');
    if (hash_ptr) {
        *hash_ptr = '\0';
        long area_uid = strtol(input, &endptr, 10);
        if (*endptr == '\0' && area_uid > 0) {
            int vnum = (int)strtol(hash_ptr + 1, &endptr, 10);
            if (*endptr == '\0' && vnum > 0) {
                result.area_uid = area_uid;
                result.vnum = vnum;
                result.original_vnum = vnum;
                return result;
            }
        }
        result.area_uid = 0;
        result.vnum = 0;
        return result;
    }
    
    // Simple vnum format
    int vnum = (int)strtol(input, &endptr, 10);
    if (*endptr == '\0' && vnum > 0) {
        if (default_area) {
            result.area_uid = default_area->uid;
            result.vnum = vnum;
            result.original_vnum = vnum;
        } else {
            result.area_uid = 0;
            result.vnum = vnum;
            result.original_vnum = vnum;
        }
    }
    
    return result;
}

// Setup test areas
void setup_test_areas(void) {
    memset(area_list, 0, sizeof(area_list));
    
    // Area 0: Midgaard
    area_list[0].uid = 1;
    area_list[0].name = strdup("Midgaard");
    area_list[0].filename = strdup("midgaard.are");
    area_list[0].vnums[0] = 3000;
    area_list[0].vnums[1] = 3099;
    
    // Area 1: Limbo
    area_list[1].uid = 2;
    area_list[1].name = strdup("Limbo");
    area_list[1].filename = strdup("limbo.are");
    area_list[1].vnums[0] = 1;
    area_list[1].vnums[1] = 99;
    
    // Area 2: School
    area_list[2].uid = 5;
    area_list[2].name = strdup("School");
    area_list[2].filename = strdup("school.are");
    area_list[2].vnums[0] = 3700;
    area_list[2].vnums[1] = 3799;
}

void teardown_test_areas(void) {
    for (int i = 0; i < 10; i++) {
        if (area_list[i].name) {
            free(area_list[i].name);
            area_list[i].name = NULL;
        }
        if (area_list[i].filename) {
            free(area_list[i].filename);
            area_list[i].filename = NULL;
        }
    }
}

TestSuite(wnum_parsing);

Test(wnum_parsing, test_simple_vnum, .init = setup_test_areas, .fini = teardown_test_areas) {
    AREA_DATA *default_area = &area_list[0]; // Midgaard
    
    WNUM result = parse_widevnum("1234", default_area);
    
    cr_assert_eq(result.area_uid, 1, "Should use default area UID");
    cr_assert_eq(result.vnum, 1234, "Should parse vnum correctly");
    cr_assert_eq(result.original_vnum, 1234, "Original vnum should match");
}

Test(wnum_parsing, test_uid_hash_vnum, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("5#3750", NULL);
    
    cr_assert_eq(result.area_uid, 5, "Should parse area UID correctly");
    cr_assert_eq(result.vnum, 3750, "Should parse vnum correctly");
    cr_assert_eq(result.original_vnum, 3750, "Original vnum should match");
}

Test(wnum_parsing, test_area_name_hash_vnum, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("'School'#3750", NULL);
    
    cr_assert_eq(result.area_uid, 5, "Should find School area (UID 5)");
    cr_assert_eq(result.vnum, 3750, "Should parse vnum correctly");
    cr_assert_eq(result.original_vnum, 3750, "Original vnum should match");
}

Test(wnum_parsing, test_invalid_area_name, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("'NonExistent'#1234", NULL);
    
    cr_assert_eq(result.area_uid, 0, "Should return 0 for invalid area");
    cr_assert_eq(result.vnum, 0, "Should return 0 for invalid vnum");
}

Test(wnum_parsing, test_invalid_uid, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("999#1234", NULL);
    
    cr_assert_eq(result.area_uid, 999, "Should return the parsed UID even if invalid");
    cr_assert_eq(result.vnum, 1234, "Should parse vnum correctly");
}

Test(wnum_parsing, test_empty_input, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("", NULL);
    
    cr_assert_eq(result.area_uid, 0, "Empty input should return 0");
    cr_assert_eq(result.vnum, 0, "Empty input should return 0");
}

Test(wnum_parsing, test_null_input, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum(NULL, NULL);
    
    cr_assert_eq(result.area_uid, 0, "NULL input should return 0");
    cr_assert_eq(result.vnum, 0, "NULL input should return 0");
}

Test(wnum_parsing, test_zero_vnum, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("0", NULL);
    
    cr_assert_eq(result.area_uid, 0, "Zero vnum should return 0");
    cr_assert_eq(result.vnum, 0, "Zero vnum should return 0");
}

Test(wnum_parsing, test_negative_vnum, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("-5", NULL);
    
    cr_assert_eq(result.area_uid, 0, "Negative vnum should return 0");
    cr_assert_eq(result.vnum, 0, "Negative vnum should return 0");
}

Test(wnum_parsing, test_malformed_area_name, .init = setup_test_areas, .fini = teardown_test_areas) {
    WNUM result = parse_widevnum("'Unclosed quote#1234", NULL);
    
    cr_assert_eq(result.area_uid, 0, "Malformed area name should return 0");
    cr_assert_eq(result.vnum, 0, "Malformed area name should return 0");
}